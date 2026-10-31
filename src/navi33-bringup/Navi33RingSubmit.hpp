#ifndef COBALT64_NAVI33_RING_SUBMIT_HPP
#define COBALT64_NAVI33_RING_SUBMIT_HPP

// Shared ring submission for the stages that come after the ring stage
// (SMU load, GC load). The sequence is the one the PSP ring stage uses
// (psp_ring_cmd_submit + psp_cmd_submit_buf, verified on Navi 32 by
// RedNavi), built from the same public constants in Navi33PspRing.hpp so the
// stages cannot drift apart. Header-only so every stage compiles the same
// code; kernel-only (IOSleep), not part of the host test surface.

#include <IOKit/IOLib.h>

#include "Navi33MmWindow.hpp"
#include "Navi33PspRing.hpp"

namespace cobalt64::navi33::psp {

// Zero a VRAM region through the window, paced.
inline bool ringVramZero(const RegBar &bar, uint64_t vramByte, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; i += 4) {
        if (!vramWrite(bar, vramByte + i, 0))
            return false;
        if ((i & 0x3FFFu) == 0)
            IOSleep(1);
    }
    return true;
}

// Submit one command through the kernel ring and wait for its fence.
// fenceIndex must be unique per submission within the boot (the ring stage
// counts up from 1; later stages start higher, e.g. 100 / 200).
inline bool ringSubmit(const RegBar &bar, uint64_t mcBase, uint32_t cmdId,
                       const uint32_t *args, uint32_t nargs,
                       uint32_t *resp, uint32_t nresp, uint32_t fenceIndex)
{
    const uint64_t cmdMc = mcBase + kCmdBufVramOffset;
    const uint64_t fenceMc = mcBase + kFenceBufVramOffset;

    if (!ringVramZero(bar, kCmdBufVramOffset, kCmdBufBytes) ||
        !ringVramZero(bar, kFenceBufVramOffset, kCmdBufBytes))
        return false;
    if (!vramWrite(bar, kCmdBufVramOffset + kCmdBufCmdIdOff, cmdId))
        return false;
    for (uint32_t i = 0; i < nargs; ++i) {
        if (!vramWrite(bar, kCmdBufVramOffset + kCmdBufArgsOff + i * 4, args[i]))
            return false;
    }

    uint32_t wptr = 0;
    if (!bar.rd(mp0ByteOffset(kC2pmsg67Dword), &wptr) || !wptrValid(wptr)) {
        IOLog("Navi33RingSubmit: write pointer read failed or odd value 0x%x, not sending",
              wptr);
        return false;
    }
    const uint64_t frame = frameVramOffset(wptr);
    for (uint32_t i = 0; i < kFrameBytes; i += 4) {
        if (!vramWrite(bar, frame + i, 0))
            return false;
    }
    if (!vramWrite(bar, frame + kFrameCmdLo, static_cast<uint32_t>(cmdMc)) ||
        !vramWrite(bar, frame + kFrameCmdHi, static_cast<uint32_t>(cmdMc >> 32)) ||
        !vramWrite(bar, frame + kFrameFenceLo, static_cast<uint32_t>(fenceMc)) ||
        !vramWrite(bar, frame + kFrameFenceHi, static_cast<uint32_t>(fenceMc >> 32)) ||
        !vramWrite(bar, frame + kFrameFenceValue, fenceIndex))
        return false;
    if (!bar.wr(mp0ByteOffset(kC2pmsg67Dword), nextWptr(wptr)))
        return false;

    uint32_t fence = 0;
    for (uint32_t ms = 0; ms <= kFenceWaitMs; ++ms) {
        if (vramRead(bar, kFenceBufVramOffset, &fence) && fence == fenceIndex)
            break;
        IOSleep(1);
    }
    for (uint32_t i = 0; i < nresp; ++i)
        vramRead(bar, kCmdBufVramOffset + kCmdBufRespOff + i * 4, &resp[i]);
    return fence == fenceIndex;
}

}  // namespace cobalt64::navi33::psp

#endif
