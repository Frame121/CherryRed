#include "gfx11_packets.h"
#include <stdio.h>

int main() {
    constexpr uint32_t gcBase1 = 0xA000u; // RX7600 IP Discovery base segment 1
    constexpr uint32_t scratchReg = 0x2040u;
    constexpr auto packet = rednavi::gfx11::makeScratchWritePacket(
        gcBase1 + scratchReg, rednavi::gfx11::kRingTestMagic);
    static_assert(packet.dwords[0] == 0xC0017900u, "SET_UCONFIG_REG packet header");
    static_assert(packet.dwords[1] == 0x40u, "UCONFIG register offset from 0xC000");
    static_assert(packet.dwords[2] == 0xDEADBEEFu, "scratch write magic");
    static_assert(packet.dwords[3] == 0xC0031000u, "NOP packet header");
    static_assert(packet.dwords[4] == 0 && packet.dwords[7] == 0, "packet padding");
    puts("GFX11 packet tests: PASS (SET_UCONFIG_REG + NOP, 8 dwords)");
    return 0;
}
