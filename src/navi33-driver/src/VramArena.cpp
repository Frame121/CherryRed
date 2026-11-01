#include "VramArena.hpp"

namespace cherryred::navi33 {
namespace {
bool valid(AddressRange range) noexcept {
    return range.size != 0 && range.size <= UINT64_MAX - range.base;
}
bool overlaps(AddressRange a, AddressRange b) noexcept {
    return a.base < b.base + b.size && b.base < a.base + a.size;
}
bool align(uint64_t value, uint64_t alignment, uint64_t& out) noexcept {
    const uint64_t mask = alignment - 1;
    if (value > UINT64_MAX - mask) return false;
    out = (value + mask) & ~mask;
    return true;
}
}  // namespace

MemoryError VramArena::configure(AddressRange aperture,
                                 const AddressRange* excluded, size_t count) noexcept {
    if (live_) return MemoryError::Busy;
    if (!valid(aperture) || (count && !excluded)) return MemoryError::InvalidRange;
    if (count > kCapacity) return MemoryError::Capacity;
    for (size_t i = 0; i < count; ++i) {
        const auto r = excluded[i];
        if (!valid(r) || r.base < aperture.base ||
            r.base - aperture.base > aperture.size ||
            r.size > aperture.size - (r.base - aperture.base))
            return MemoryError::InvalidRange;
        for (size_t j = 0; j < i; ++j)
            if (overlaps(r, excluded[j])) return MemoryError::OverlappingReservation;
    }
    // Commit only after validation, preserving the old state on failure.
    for (auto& slot : slots_) slot = {};
    for (size_t i = 0; i < count; ++i) slots_[i].range = excluded[i];
    aperture_ = aperture;
    return MemoryError::None;
}

MemoryError VramArena::allocate(uint64_t size, uint64_t alignment,
                               Allocation& out) noexcept {
    out = {};
    if (!aperture_.size) return MemoryError::NotConfigured;
    if (!size) return MemoryError::InvalidRange;
    if (!alignment || (alignment & (alignment - 1))) return MemoryError::InvalidAlignment;
    size_t empty = kCapacity;
    for (size_t i = 0; i < kCapacity; ++i)
        if (!slots_[i].range.size) { empty = i; break; }
    if (empty == kCapacity || nextSerial_ == 0) return MemoryError::Capacity;

    uint64_t candidate = aperture_.base;
    const uint64_t end = aperture_.base + aperture_.size;
    for (;;) {
        if (!align(candidate, alignment, candidate) || candidate > end || size > end - candidate)
            return MemoryError::OutOfSpace;
        uint64_t next = candidate;
        for (const auto& slot : slots_) {
            if (slot.range.size && overlaps({candidate, size}, slot.range)) {
                const uint64_t occupiedEnd = slot.range.base + slot.range.size;
                if (occupiedEnd > next) next = occupiedEnd;
            }
        }
        if (next == candidate) break;
        candidate = next; // strictly advances past at least one occupied range
    }
    auto& slot = slots_[empty];
    slot.range = {candidate, size};
    slot.serial = nextSerial_++;
    ++live_;
    out = {this, slot.serial, slot.range};
    return MemoryError::None;
}

MemoryError VramArena::release(const Allocation& allocation) noexcept {
    if (allocation.owner != this || allocation.serial == 0)
        return MemoryError::InvalidAllocation;
    for (auto& slot : slots_) {
        if (slot.serial == allocation.serial && slot.range.size &&
            slot.range.base == allocation.range.base && slot.range.size == allocation.range.size) {
            slot = {};
            --live_;
            return MemoryError::None;
        }
    }
    return MemoryError::InvalidAllocation;
}

uint64_t VramArena::freeBytes() const noexcept {
    uint64_t free = aperture_.size;
    for (const auto& slot : slots_) free -= slot.range.size;
    return free;
}
}  // namespace cherryred::navi33
