#include "VramArena.hpp"
#include "Fixture.hpp"
#include <random>
#include <vector>
using namespace cherryred::navi33;

int main() {
    VramArena arena;
    Allocation item;
    CHECK(arena.allocate(1, 1, item) == MemoryError::NotConfigured);
    CHECK(arena.configure({UINT64_MAX - 3, 8}, nullptr, 0) == MemoryError::InvalidRange);
    const AddressRange reserved[] = {{128, 128}, {512, 64}};
    CHECK(arena.configure({0, 1024}, reserved, 2) == MemoryError::None);
    CHECK(arena.freeBytes() == 832);
    const AddressRange overlap[] = {{100, 100}, {150, 100}};
    CHECK(arena.configure({0, 1024}, overlap, 2) == MemoryError::OverlappingReservation);
    CHECK(arena.freeBytes() == 832); // failed configuration is atomic
    CHECK(arena.allocate(128, 128, item) == MemoryError::None && item.range.base == 0);
    const auto stale = item;
    CHECK(arena.configure({0, 4096}, nullptr, 0) == MemoryError::Busy);
    Allocation second;
    CHECK(arena.allocate(257, 128, second) == MemoryError::None && second.range.base == 640);
    CHECK(arena.release(item) == MemoryError::None);
    CHECK(arena.release(item) == MemoryError::InvalidAllocation);
    CHECK(arena.allocate(128, 128, item) == MemoryError::None && item.range.base == 0);
    CHECK(arena.release(stale) == MemoryError::InvalidAllocation);
    VramArena other;
    CHECK(other.release(item) == MemoryError::InvalidAllocation);
    auto forged = item;
    forged.range.size++;
    CHECK(arena.release(forged) == MemoryError::InvalidAllocation);
    CHECK(arena.release(item) == MemoryError::None && arena.release(second) == MemoryError::None);
    CHECK(arena.configure({0, 1024}, nullptr, 0) == MemoryError::None);
    CHECK(arena.allocate(128, 128, item) == MemoryError::None);
    CHECK(arena.release(stale) == MemoryError::InvalidAllocation);
    CHECK(arena.release(item) == MemoryError::None);
    CHECK(arena.allocate(1, 3, item) == MemoryError::InvalidAlignment);
    CHECK(arena.allocate(1, 0, item) == MemoryError::InvalidAlignment);
    CHECK(arena.allocate(0, 1, item) == MemoryError::InvalidRange);
    CHECK(arena.configure({UINT64_MAX - 15, 15}, nullptr, 0) == MemoryError::None);
    CHECK(arena.allocate(8, 16, item) == MemoryError::None);
    CHECK(arena.allocate(8, 16, second) == MemoryError::OutOfSpace);
    CHECK(arena.release(item) == MemoryError::None);

    CHECK(arena.configure({0, 4096}, nullptr, 0) == MemoryError::None);
    std::vector<Allocation> slots;
    for (size_t i = 0; i < VramArena::kCapacity; ++i) {
        CHECK(arena.allocate(1, 1, item) == MemoryError::None);
        slots.push_back(item);
    }
    CHECK(arena.allocate(1, 1, item) == MemoryError::Capacity);
    for (const auto& slot : slots) CHECK(arena.release(slot) == MemoryError::None);

    // Compare every operation with an independent byte-occupancy model.
    CHECK(arena.configure({0, 1024}, reserved, 2) == MemoryError::None);
    bool occupied[1024]{};
    for (const auto r : reserved)
        for (uint64_t i = r.base; i < r.base + r.size; ++i) occupied[i] = true;
    slots.clear();
    std::mt19937 random(0x7600);
    for (unsigned trial = 0; trial < 5000; ++trial) {
        if (!slots.empty() && (random() % 3 == 0)) {
            const size_t index = random() % slots.size();
            const auto released = slots[index];
            CHECK(arena.release(released) == MemoryError::None);
            for (uint64_t i = released.range.base; i < released.range.base + released.range.size; ++i)
                occupied[i] = false;
            slots.erase(slots.begin() + static_cast<std::ptrdiff_t>(index));
        } else {
            const uint64_t size = 1 + random() % 100;
            const uint64_t alignment = uint64_t{1} << (random() % 7);
            uint64_t expected = 1024;
            for (uint64_t base = 0; base + size <= 1024; base += alignment) {
                bool free = true;
                for (uint64_t i = base; i < base + size; ++i) if (occupied[i]) free = false;
                if (free) { expected = base; break; }
            }
            const auto result = arena.allocate(size, alignment, item);
            if (result == MemoryError::None) {
                CHECK(item.range.base == expected && item.range.size == size);
                for (uint64_t i = expected; i < expected + size; ++i) occupied[i] = true;
                slots.push_back(item);
            } else {
                CHECK(result == MemoryError::OutOfSpace || result == MemoryError::Capacity);
                CHECK(expected == 1024 || slots.size() + 2 == VramArena::kCapacity);
            }
        }
        uint64_t free = 0;
        for (bool byte : occupied) if (!byte) ++free;
        CHECK(arena.freeBytes() == free && arena.liveAllocations() == slots.size());
    }
}
