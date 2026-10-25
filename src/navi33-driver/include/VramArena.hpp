#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cherryred::navi33 {

struct AddressRange {
    uint64_t base = 0;
    uint64_t size = 0;
};

class VramArena;
struct Allocation {
    const VramArena* owner = nullptr;
    uint64_t serial = 0;
    AddressRange range{};
};

enum class MemoryError {
    None, InvalidRange, InvalidAlignment, NotConfigured, Busy,
    OverlappingReservation, OutOfSpace, Capacity, InvalidAllocation
};

// Software address allocator only: no GPU mapping, CPU access, or residency.
// Caller supplies an independently verified free aperture and exclusions.
// Single-threaded; callers must serialize all operations.
class VramArena {
public:
    static constexpr size_t kCapacity = 64;
    VramArena() = default;
    VramArena(const VramArena&) = delete;
    VramArena& operator=(const VramArena&) = delete;
    VramArena(VramArena&&) = delete;
    VramArena& operator=(VramArena&&) = delete;

    // Failed configure leaves the previous configuration unchanged. Cannot
    // reconfigure with live allocations. Excluded ranges must be disjoint.
    MemoryError configure(AddressRange aperture, const AddressRange* excluded,
                          size_t count) noexcept;
    MemoryError allocate(uint64_t size, uint64_t alignment, Allocation& out) noexcept;
    MemoryError release(const Allocation& allocation) noexcept;
    size_t liveAllocations() const noexcept { return live_; }
    uint64_t freeBytes() const noexcept;

private:
    struct Slot {
        AddressRange range{};
        uint64_t serial = 0; // zero denotes a reservation or unused slot
    };
    Slot slots_[kCapacity]{};
    AddressRange aperture_{};
    uint64_t nextSerial_ = 1; // survives reconfiguration to reject stale handles
    size_t live_ = 0;
};
}  // namespace cherryred::navi33
