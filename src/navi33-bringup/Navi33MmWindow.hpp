#ifndef COBALT64_NAVI33_MM_WINDOW_HPP
#define COBALT64_NAVI33_MM_WINDOW_HPP

#include <IOKit/IOLib.h>
#include <string.h>

#include "Navi33PspLoad.hpp"

// Shared pieces of the register-BAR + VRAM-window access used by the write
// stages (PSP load, PSP ring). The bounds-checked RegBar and the
// MM_INDEX/MM_DATA window helpers live here so every stage uses the same
// checked accessors instead of raw pointer arithmetic.
//
// The window addressing (BAR5 byte 0x00/0x04/0x18, index = vramByte |
// 0x80000000, hi = vramByte >> 31) is the RedNavi-verified sequence; the
// Navi 33 values stay UNVERIFIED until the stages run on the card.

namespace cobalt64::navi33::psp {

// The register BAR (BAR5) with bounds-checked 32-bit reads/writes.
struct RegBar {
    volatile uint32_t *va = nullptr;
    uint64_t len = 0;

    bool rd(uint64_t byteOff, uint32_t *out) const
    {
        if (!va || byteOff > len || len - byteOff < 4)
            return false;
        *out = va[byteOff / 4];
        return true;
    }

    bool wr(uint64_t byteOff, uint32_t v) const
    {
        if (!va || byteOff > len || len - byteOff < 4)
            return false;
        va[byteOff / 4] = v;
        return true;
    }
};

// VRAM access through the SOC15 MM_INDEX/MM_DATA window (NBIF base 0).
inline bool vramWrite(const RegBar &bar, uint64_t vramByte, uint32_t v)
{
    return bar.wr(kMmIndexByte, mmIndexLow(vramByte)) &&
           bar.wr(kMmIndexHiByte, mmIndexHigh(vramByte)) &&
           bar.wr(kMmDataByte, v);
}

inline bool vramRead(const RegBar &bar, uint64_t vramByte, uint32_t *out)
{
    if (!bar.wr(kMmIndexByte, mmIndexLow(vramByte)) ||
        !bar.wr(kMmIndexHiByte, mmIndexHigh(vramByte)))
        return false;
    return bar.rd(kMmDataByte, out);
}

// Copy `bytes` (dword-multiple) into VRAM through the index/data window and
// verify every dword on readback. Returns the mismatch count, or -1 when the
// window itself fails. Paced so a hang leaves a readable log line.
inline int32_t vramCopyVerify(const RegBar &bar, uint64_t vramByte,
                              const uint8_t *src, uint32_t bytes)
{
    if (!src || (bytes & 3u) != 0)
        return -1;
    uint32_t mismatches = 0;
    for (uint32_t i = 0; i < bytes; i += 4) {
        uint32_t v;
        memcpy(&v, src + i, 4);
        if (!vramWrite(bar, vramByte + i, v))
            return -1;
        if ((i & 0x3FFFu) == 0)
            IOSleep(1);                       // pace ~every 16 KiB
        uint32_t back = 0;
        if (!vramRead(bar, vramByte + i, &back))
            return -1;
        if (back != v)
            mismatches++;
    }
    return static_cast<int32_t>(mismatches);
}

}  // namespace cobalt64::navi33::psp

#endif
