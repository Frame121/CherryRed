//
// test_Navi33PspRing.cpp — host unit tests for the pure logic of the PSP ring
// stage (Navi33PspRing.hpp). No IOKit, no hardware: the ring geometry, frame
// layout, write-pointer arithmetic, response decodes and TMR placement rules
// are pinned here so the kext cannot drift from the RedNavi-verified sequence.
//
#include "Navi33PspRing.hpp"

#include <cstdio>

using namespace cobalt64::navi33::psp;

namespace {

int failures = 0;

void check(bool condition, const char *label)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}

void testPinnedOffsets()
{
    // Ring mailboxes from the MP0 base (mp_13_0_2_offset.h, 0x16000 dwords).
    check(mp0ByteOffset(kC2pmsg64Dword) == 0x58200u, "C2PMSG_64 byte offset");
    check(mp0ByteOffset(kC2pmsg67Dword) == 0x5820Cu, "C2PMSG_67 byte offset");
    check(mp0ByteOffset(kC2pmsg69Dword) == 0x58214u, "C2PMSG_69 byte offset");
    check(mp0ByteOffset(kC2pmsg70Dword) == 0x58218u, "C2PMSG_70 byte offset");
    check(mp0ByteOffset(kC2pmsg71Dword) == 0x5821Cu, "C2PMSG_71 byte offset");

    // Ring geometry (RedNavi's proven VRAM placement).
    check(kRingVramOffset == 0x40200000ull, "ring VRAM offset");
    check(kRingSizeBytes == 0x1000u, "ring size");
    check(kRingDwords == 0x400u, "ring size in dwords");
    check(kCmdBufVramOffset == 0x40210000ull, "command buffer offset");
    check(kFenceBufVramOffset == 0x40220000ull, "fence buffer offset");
    check(kTocStagingVramOffset == 0x40230000ull, "TOC staging offset");

    // Frame layout (psp_gfx_cmd_resp as RedNavi writes it).
    check(kFrameDwords == 16u, "frame is 16 dwords");
    check(kFrameBytes == 64u, "frame is 64 bytes");
    check(kFrameCmdLo == 0u && kFrameCmdHi == 4u, "cmd MC lo/hi fields");
    check(kFrameFenceLo == 12u && kFrameFenceHi == 16u, "fence MC lo/hi fields");
    check(kFrameFenceValue == 20u, "fence value field");

    // Command-buffer layout.
    check(kCmdBufCmdIdOff == 8u, "cmd_id offset");
    check(kCmdBufArgsOff == 28u, "args offset");
    check(kCmdBufRespOff == 864u, "response offset");
}

void testRingDecodes()
{
    // Success: bit 31 + status 0 (0x80C20000 was RedNavi's real response:
    // the middle bits are not interpreted, the mask keeps bit31+status).
    check(ringCreateAccepted(0x80000000u), "ring created (clean)");
    check(ringCreateAccepted(0x80C20000u), "ring created (RedNavi's real value)");
    check(!ringCreateAccepted(0x00000000u), "not created");
    check(!ringCreateAccepted(0x80000001u), "status 1 is not success");
    check(!ringCreateAccepted(0x8000FFFFu), "status FFFF is not success");

    // Write-pointer arithmetic: a frame is 16 dwords, wptr wraps modulo 0x400.
    check(wptrValid(0), "wptr 0 valid");
    check(wptrValid(16), "wptr 16 valid");
    check(wptrValid(0x3F0), "wptr 0x3F0 valid (last frame slot)");
    check(!wptrValid(4), "unaligned wptr invalid");
    check(!wptrValid(0x400), "wptr at ring end invalid");
    check(nextWptr(0) == 16u, "wptr advance");
    check(nextWptr(0x3F0) == 0u, "wptr wraps");
    check(frameVramOffset(0) == 0x40200000ull, "frame at wptr 0");
    check(frameVramOffset(16) == 0x40200040ull, "frame at wptr 16");
}

void testTmrPlacement()
{
    // A 16 GiB card, the Navi 32 reference: top 0x400000000 - 512 MiB.
    uint64_t off = 0;
    check(tmrOffsetForMode(0x400000000ull, 0xA700000ull, kTmrModeLinuxLike, &off),
          "Linux-like placement accepted");
    check(off == 0x3E0000000ull, "matches the verified 0x83e0000000 (MC) offset");

    // An 8 GiB card (RX 7600): top - 512 MiB — plausible but UNVERIFIED.
    check(tmrOffsetForMode(0x200000000ull, 0xA700000ull, kTmrModeLinuxLike, &off),
          "8 GiB Linux-like placement accepted");
    check(off == 0x1E0000000ull, "8 GiB placement arithmetic");

    // Too little VRAM for the margin refuses.
    check(!tmrOffsetForMode(0x20000000ull, 0x1000000ull, kTmrModeLinuxLike, &off),
          "VRAM smaller than the margin refuses");

    // The diagnostic placement is fixed at 32 MiB.
    check(tmrOffsetForMode(0x200000000ull, 0x1000000ull, kTmrModeVisibleBar, &off),
          "visible-BAR placement accepted");
    check(off == 0x02000000ull, "visible-BAR placement is 32 MiB");

    // Zero-size TMR and unknown modes refuse.
    check(!tmrOffsetForMode(0x200000000ull, 0, kTmrModeLinuxLike, &off),
          "zero-size TMR refuses");
    check(!tmrOffsetForMode(0x200000000ull, 0x1000000ull, 99u, &off),
          "unknown mode refuses");
}

}  // namespace

int main()
{
    testPinnedOffsets();
    testRingDecodes();
    testTmrPlacement();

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("Navi33PspRing: all checks passed\n");
    return 0;
}
