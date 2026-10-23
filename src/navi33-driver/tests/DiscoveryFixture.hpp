#pragma once

#include "Fixture.hpp"

inline void put16(uint8_t* bytes, uint16_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
}

// Synthetic v1 binary, v3 IP table at 64, die at 144, two records at 148/160.
// The IP tuples are test values, not measurements of a Navi33 board.
inline void refreshDiscovery(std::array<uint8_t, 512>& bytes) {
    uint32_t tableSum = 0;
    const size_t tableSize = bytes[70] | (static_cast<size_t>(bytes[71]) << 8);
    for (size_t i = 64; i < 64 + tableSize && i < bytes.size(); ++i) tableSum += bytes[i];
    put16(bytes.data() + 14, static_cast<uint16_t>(tableSum));
    uint32_t binarySum = 0;
    for (size_t i = 10; i < bytes.size(); ++i) binarySum += bytes[i];
    put16(bytes.data() + 8, static_cast<uint16_t>(binarySum));
}

inline std::array<uint8_t, 512> discoveryFixture() {
    std::array<uint8_t, 512> bytes{};
    put32(bytes.data(), 0x28211407);
    put16(bytes.data() + 4, 1);
    put16(bytes.data() + 10, 512);
    put16(bytes.data() + 12, 64);
    put16(bytes.data() + 16, 108);
    put32(bytes.data() + 64, 0x53445049);
    put16(bytes.data() + 68, 3);
    put16(bytes.data() + 70, 108);
    put16(bytes.data() + 76, 1);
    put16(bytes.data() + 80, 144);
    put16(bytes.data() + 146, 2);
    put16(bytes.data() + 148, 11);
    bytes[151] = 1; bytes[152] = 99; bytes[153] = 3; bytes[154] = 7; bytes[155] = 0x42;
    put32(bytes.data() + 156, 0x100);
    put16(bytes.data() + 160, 42);
    bytes[162] = 1; bytes[163] = 1; bytes[164] = 98; bytes[165] = 2; bytes[166] = 6;
    put32(bytes.data() + 168, 0x200);
    refreshDiscovery(bytes);
    return bytes;
}
