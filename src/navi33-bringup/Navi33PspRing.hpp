#ifndef COBALT64_NAVI33_PSP_RING_HPP
#define COBALT64_NAVI33_PSP_RING_HPP

#include <stdint.h>

#include "Navi33PspLoad.hpp"

// Pure, host-testable pieces of the PSP ring stage (completes M2a: GPCOM ring
// + LOAD_TOC + SETUP_TMR). No IOKit includes.
//
// Provenance of every constant here:
//  - The ring-create handshake (C2PMSG_64 ready/response with the
//    0x8000FFFF mask, C2PMSG_69/70/71 = ring MC lo/hi/size, command
//    PSP_RING_TYPE__KM << 16, mdelay(20) then wait) is psp_v13_0_ring_create
//    in the Linux kernel (MIT), and the identical sequence RedNavi verified on
//    a real RX 7800 XT (response 0x80C20000: bit 31 + status 0).
//  - The ring-frame layout (64-byte frame: cmd MC lo/hi at +0/+4, fence MC
//    lo/hi at +12/+16, fence value at +20; write pointer advances 16 dwords;
//    command buffer: cmd_id at +8, args at +28, response at +864) and the
//    LOAD_TOC / SETUP_TMR argument shapes (toc MC lo/hi/size;
//    buf MC lo/hi/size/flags=PHY_ADDR/system PA lo/hi) are the working RedNavi
//    implementation (psp_cmd_submit_buf / psp_ring_cmd_submit equivalents),
//    verified on Navi 32: LOAD_TOC answered status 0 + tmr_size 0xA700000,
//    SETUP_TMR answered status 0.
//  - TMR mode 1 (start = VRAM top - 512 MiB) reproduces the Linux reference
//    placement RedNavi confirmed on the 16 GiB card ("reserve 0xa700000 from
//    0x83e0000000" = top - 0x20000000); the Navi 33 value is UNVERIFIED until
//    the RX 7600's own evidence is captured. Mode 2 (32 MiB, inside the
//    CPU-visible BAR window) is the diagnostic placement RedNavi used first.

namespace cobalt64::navi33::psp {

// Ring mailboxes: dword offsets from the MP0 base (mp_13_0_2_offset.h).
inline constexpr uint32_t kC2pmsg64Dword = 0x080;   // ring command/response
inline constexpr uint32_t kC2pmsg67Dword = 0x083;   // ring write pointer (dwords)
inline constexpr uint32_t kC2pmsg69Dword = 0x085;   // ring MC address lo
inline constexpr uint32_t kC2pmsg70Dword = 0x086;   // ring MC address hi
inline constexpr uint32_t kC2pmsg71Dword = 0x087;   // ring size

// psp_v13_0_ring_create: response flag bit 31 + status in the low 16 bits.
inline constexpr uint32_t kRingMboxMask = 0x8000FFFFu;
inline constexpr uint32_t kRingTypeKm = 2u;         // PSP_RING_TYPE__KM

// VRAM geometry of the ring and its buffers (RedNavi's proven placement).
inline constexpr uint64_t kRingVramOffset = 0x40200000ull;   // 4 KiB ring
inline constexpr uint32_t kRingSizeBytes = 0x1000u;
inline constexpr uint32_t kRingDwords = kRingSizeBytes / 4;
inline constexpr uint64_t kCmdBufVramOffset = 0x40210000ull; // 4 KiB command buffer
inline constexpr uint64_t kFenceBufVramOffset = 0x40220000ull; // 4 KiB fence buffer
inline constexpr uint64_t kTocStagingVramOffset = 0x40230000ull; // TOC staging (4 KiB)
inline constexpr uint32_t kCmdBufBytes = 0x1000u;

// Ring frame (struct psp_gfx_cmd_resp as submitted by psp_ring_cmd_submit):
// 64 bytes, 16 dwords; the PSP writes the response into the same frame.
inline constexpr uint32_t kFrameDwords = 16;
inline constexpr uint32_t kFrameBytes = kFrameDwords * 4;
inline constexpr uint32_t kFrameCmdLo = 0;     // command buffer MC lo
inline constexpr uint32_t kFrameCmdHi = 4;     // command buffer MC hi
inline constexpr uint32_t kFrameFenceLo = 12;  // fence buffer MC lo
inline constexpr uint32_t kFrameFenceHi = 16;  // fence buffer MC hi
inline constexpr uint32_t kFrameFenceValue = 20;

// Command-buffer layout (RedNavi's working offsets): cmd_id at +8, arguments
// from +28, the PSP's response from +864 (resp[0] = status).
inline constexpr uint32_t kCmdBufCmdIdOff = 8;
inline constexpr uint32_t kCmdBufArgsOff = 28;
inline constexpr uint32_t kCmdBufRespOff = 864;

// GFX command IDs (amdgpu_psp.h).
inline constexpr uint32_t kCmdLoadToc = 0x20u;    // GFX_CMD_ID_LOAD_TOC
inline constexpr uint32_t kCmdSetupTmr = 0x05u;   // GFX_CMD_ID_SETUP_TMR

// SETUP_TMR flags: bit 1 = the given addresses are physical (virt_phy_addr).
inline constexpr uint32_t kSetupTmrFlagsPhyAddr = 2u;

// TMR placement modes.
inline constexpr uint32_t kTmrModeLinuxLike = 1;  // VRAM top - 512 MiB
inline constexpr uint32_t kTmrModeVisibleBar = 2; // 32 MiB (diagnostic)
inline constexpr uint64_t kTmrTopMarginBytes = 0x20000000ull;   // 512 MiB
inline constexpr uint64_t kTmrVisibleBarOffset = 0x02000000ull; // 32 MiB

// Waits (RedNavi used 2 s bounds on Navi 32).
inline constexpr uint32_t kRingReadyWaitMs = 2000;
inline constexpr uint32_t kFenceWaitMs = 2000;
inline constexpr uint32_t kRingPostCmdDelayMs = 20;

// psp_v13_0_ring_create success: (C2PMSG_64 & 0x8000FFFF) == 0x80000000.
inline constexpr bool ringCreateAccepted(uint32_t c2pmsg64) noexcept
{
    return (c2pmsg64 & kRingMboxMask) == 0x80000000u;
}

// The write pointer is a dword offset; a frame is 16 dwords and must be
// wholly inside the ring.
inline constexpr bool wptrValid(uint32_t wptrDwords) noexcept
{
    return wptrDwords % kFrameDwords == 0 && wptrDwords < kRingDwords;
}

inline constexpr uint32_t nextWptr(uint32_t wptrDwords) noexcept
{
    return (wptrDwords + kFrameDwords) % kRingDwords;
}

inline constexpr uint64_t frameVramOffset(uint32_t wptrDwords) noexcept
{
    return kRingVramOffset + static_cast<uint64_t>(wptrDwords) * 4;
}

// TMR placement. Mode 1 requires enough VRAM to keep the 512 MiB top margin
// plus the TMR itself inside the frame-buffer window; mode 2 is the fixed
// diagnostic offset. Returns false when the mode/values are implausible.
inline constexpr bool tmrOffsetForMode(uint64_t vramBytes, uint64_t tmrSize,
                                       uint32_t mode, uint64_t *out) noexcept
{
    if (!out || tmrSize == 0)
        return false;
    if (mode == kTmrModeLinuxLike) {
        if (vramBytes <= kTmrTopMarginBytes + tmrSize)
            return false;
        *out = vramBytes - kTmrTopMarginBytes;
        return true;
    }
    if (mode == kTmrModeVisibleBar) {
        *out = kTmrVisibleBarOffset;
        return true;
    }
    return false;
}

// LOAD_TOC response: dword 0 = status, dword 4 = tmr_size.
inline constexpr uint32_t loadTocTmrSizeIndex = 4;

}  // namespace cobalt64::navi33::psp

#endif
