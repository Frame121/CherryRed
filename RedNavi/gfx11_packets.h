#ifndef REDNAVI_GFX11_PACKETS_H
#define REDNAVI_GFX11_PACKETS_H

#include <stdint.h>

namespace rednavi {
namespace gfx11 {

static constexpr uint32_t kPacket3Type = 3u;
static constexpr uint32_t kPacket3SetUconfigReg = 0x79u;
static constexpr uint32_t kPacket3Nop = 0x10u;
static constexpr uint32_t kSetUconfigRegStart = 0xC000u;
static constexpr uint32_t kRingTestMagic = 0xDEADBEEFu;

constexpr uint32_t packet3Header(uint32_t opcode, uint32_t count) noexcept {
    return (kPacket3Type << 30) | ((count & 0x3fffu) << 16) | ((opcode & 0xffu) << 8);
}

struct ScratchWritePacket {
    uint32_t dwords[8];
};

// Build the existing GFX11 smoke-test packet. `registerDwordAddress` must be
// the fully resolved GC register address (IP discovery base + register offset).
// SET_UCONFIG_REG encodes an offset from 0xC000, followed by the value; the NOP
// pads the ring packet to 8 dwords. This tests packet construction only.
constexpr ScratchWritePacket makeScratchWritePacket(uint32_t registerDwordAddress,
                                                     uint32_t value) noexcept {
    return {{packet3Header(kPacket3SetUconfigReg, 1),
             registerDwordAddress - kSetUconfigRegStart,
             value,
             packet3Header(kPacket3Nop, 3),
             0, 0, 0, 0}};
}

} // namespace gfx11
} // namespace rednavi

#endif // REDNAVI_GFX11_PACKETS_H
