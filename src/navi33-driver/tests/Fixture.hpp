#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::abort(); } } while (false)

inline void put32(uint8_t* bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<uint8_t>(value >> (i * 8));
}
inline std::array<uint8_t, 128> fixture() {
    std::array<uint8_t, 128> data{};
    put32(data.data(), 128);
    put32(data.data() + 4, 48);
    data[8] = 2; data[10] = 7; // synthetic header version
    data[12] = 99; data[14] = 3; // deliberately not a Navi33 IP claim
    put32(data.data() + 16, 0x12345678);
    put32(data.data() + 20, 64);
    put32(data.data() + 24, 64);
    put32(data.data() + 28, 0xdeadbeef); // deliberately not a verified CRC
    return data;
}
