#ifndef REDNAVI_NAVI33_BOARD_H
#define REDNAVI_NAVI33_BOARD_H

#include <stdint.h>

namespace rednavi {

// Board facts captured from the user's PowerColor Fighter RX 7600 (PCI 1002:7480)
// on Linux 2026-11-01. These are board-specific observations, not universal values
// for every Navi 33 card or every VBIOS revision.
struct Navi33BoardProfile {
    uint16_t vendorId;
    uint16_t deviceId;
    uint32_t vramMegabytes;
    uint64_t vramMcBase;
    uint64_t vramBytes;
    uint64_t pspTmrVramOffset;
    uint32_t pspTmrBytes;
    uint32_t vbiosBytes;
    uint32_t pspSosVersion;
    uint32_t smuFirmwareVersion;
    uint32_t dmubFirmwareVersion;
};

static constexpr Navi33BoardProfile kNavi33Board = {
    0x1002, 0x7480,
    8192,
    0x8000000000ull,
    0x200000000ull,
    0x1fc000000ull,
    0x01300000u,
    57856u,
    0x00330024u,
    0x00526100u,
    0x07003600u
};

constexpr bool navi33BoardProfileIsConsistent() noexcept {
    return kNavi33Board.vramBytes == static_cast<uint64_t>(kNavi33Board.vramMegabytes) * 1024ull * 1024ull &&
           kNavi33Board.pspTmrVramOffset < kNavi33Board.vramBytes &&
           kNavi33Board.pspTmrBytes <= kNavi33Board.vramBytes - kNavi33Board.pspTmrVramOffset &&
           kNavi33Board.vramMcBase + kNavi33Board.pspTmrVramOffset == 0x81fc000000ull;
}

static_assert(navi33BoardProfileIsConsistent(), "RX7600 board memory/TMR profile is inconsistent");

} // namespace rednavi

#endif // REDNAVI_NAVI33_BOARD_H
