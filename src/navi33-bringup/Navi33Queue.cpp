//
//  Navi33Queue.cpp — Stage 5.5 / C3: SDMA 6.0.2 & GFX 11.0.2 Ring Test.
//
//  Reachable only behind navi33bringup=1 + navi33psp=1 + navi33gc_load=1 +
//  navi33mem=1 + navi33queue=1. Requires the memory stage (GFXHUB v3.0 GART)
//  to have verified memory persistence first.
//
//  Sequence:
//    1. Resumes SDMA0 instance 0 on a ring buffer in VRAM (4 KiB).
//    2. Submits a linear write packet (0xDEADBEEF to target VRAM).
//    3. Waits for SDMA0 execution and validates the memory write.
//    4. Configures RS64 program counter entries for PFP, ME, and MEC.
//    5. Resumes CP Ring 0 (GFX queue) in VRAM and submits a PM4 packet
//       SET_UCONFIG_REG writing 0xDEADBEEF to SCRATCH_REG0.
//    6. On verified execution, publishes Navi33Bringup,QueueVerified = true.
//
#include "Navi33Queue.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSBoolean.h>

#include "Navi33GcRegs.hpp"
#include "Navi33MmWindow.hpp"
#include "Navi33PspMailbox.hpp"

#define N33Q_LOG(fmt, ...) IOLog("Navi33Queue: " fmt "\n", ##__VA_ARGS__)
#define N33Q_STEP(...) do { N33Q_LOG(__VA_ARGS__); IOSleep(40); } while (0)

using cobalt64::navi33::psp::kFbLocationBaseDword;
using cobalt64::navi33::psp::kFbLocationTopDword;
using cobalt64::navi33::psp::kPciCommandMemorySpace;
using cobalt64::navi33::psp::kRegBarIndex;
using cobalt64::navi33::psp::mmhubByteOffset;
using cobalt64::navi33::psp::RegBar;
using cobalt64::navi33::gc::gcByteOffset;

namespace {

// SDMA packet header: op in bits 7:0, sub_op in bits 15:8 (sdma_v6_0)
constexpr uint32_t kSdmaOpWrite = 2u;
constexpr uint32_t kSdmaSubopWriteLinear = 0u;

// Ring geometry in VRAM (within the GART identity range):
constexpr uint64_t kSdmaRingVramOffset   = 0x0C000000ull;
constexpr uint64_t kSdmaRptrShadowOffset = 0x0C001000ull;
constexpr uint64_t kSdmaWptrShadowOffset = 0x0C001008ull;
constexpr uint64_t kSdmaTargetVramOffset = 0x0C002000ull;
constexpr uint32_t kSdmaRingBytes        = 4096u;

// GFX Ring geometry in VRAM:
constexpr uint64_t kGfxRingVramOffset    = 0x0C010000ull;
constexpr uint64_t kGfxRptrShadowOffset  = 0x0C011000ull;
constexpr uint64_t kGfxWptrShadowOffset  = 0x0C011008ull;
constexpr uint32_t kGfxRingBytes         = 4096u;

// RS64 Program Counter start constants (from gc_11_0_2 firmware binaries)
constexpr uint32_t kPfpStartLo = 0x00003000u;
constexpr uint32_t kPfpStartHi = 0x00070000u;
constexpr uint32_t kMeStartLo  = 0x00003000u;
constexpr uint32_t kMeStartHi  = 0x00070000u;
constexpr uint32_t kMecStartLo = 0x00003000u;
constexpr uint32_t kMecStartHi = 0x00070000u;

// PM4 packet helpers
inline constexpr uint32_t makePacket3(uint32_t op, uint32_t count) noexcept {
    return (3u << 30) | (((count) & 0x3FFFu) << 16) | (((op) & 0xFFu) << 8);
}
constexpr uint32_t kPacket3Nop = 0x10u;
constexpr uint32_t kPacket3SetUconfigReg = 0x79u;
constexpr uint32_t kPacket3SetUconfigRegStart = 0x0000C000u;

}  // namespace

void RunNavi33QueueStage(IOPCIDevice *pci, IOService *publisher)
{
    if (!pci || !publisher)
        return;

    N33Q_STEP("Queue stage starting (SDMA 6.0.2 + GFX 11.0.2 command execution proof)");

    const uint16_t command = pci->configRead16(kIOPCIConfigCommand);
    if (!(command & kPciCommandMemorySpace)) {
        publisher->setProperty("Navi33Bringup,QueueStage",
                               "aborted: PCI memory decode is off");
        return;
    }

    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(
        static_cast<UInt8>(kIOPCIConfigBaseAddress0 + kRegBarIndex * 4));
    if (!map) {
        publisher->setProperty("Navi33Bringup,QueueStage",
                               "aborted: register BAR5 map failed");
        return;
    }

    RegBar bar;
    bar.va = reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress());
    bar.len = static_cast<uint64_t>(map->getLength());

    const uint64_t maxNeed = mmhubByteOffset(kFbLocationTopDword) + 4;
    if (!bar.va || bar.len < maxNeed) {
        publisher->setProperty("Navi33Bringup,QueueStage",
                               "aborted: register BAR5 window too small");
        map->release();
        return;
    }

    uint32_t fbBase = 0;
    if (!bar.rd(mmhubByteOffset(kFbLocationBaseDword), &fbBase) || fbBase == 0) {
        publisher->setProperty("Navi33Bringup,QueueStage",
                               "aborted: invalid FB_LOCATION_BASE");
        map->release();
        return;
    }
    const uint64_t mcBase = static_cast<uint64_t>(fbBase) << 24;

    auto gcWr = [&](uint32_t baseIdx, uint32_t reg, uint32_t val) {
        return bar.wr(gcByteOffset(baseIdx, reg), val);
    };
    auto gcRd = [&](uint32_t baseIdx, uint32_t reg, uint32_t *out) {
        return bar.rd(gcByteOffset(baseIdx, reg), out);
    };

    // ----------------------------------------------------
    // Part 1: SDMA0 Linear Write Test
    // ----------------------------------------------------
    N33Q_STEP("Preparing SDMA0 ring at VRAM +0x%llx", kSdmaRingVramOffset);
    for (uint32_t i = 0; i < kSdmaRingBytes; i += 4)
        vramWrite(bar, kSdmaRingVramOffset + i, 0);

    vramWrite(bar, kSdmaRptrShadowOffset, 0);
    vramWrite(bar, kSdmaRptrShadowOffset + 4, 0);
    vramWrite(bar, kSdmaWptrShadowOffset, 0);
    vramWrite(bar, kSdmaWptrShadowOffset + 4, 0);
    vramWrite(bar, kSdmaTargetVramOffset, 0x00000000u);

    // Build the 5-dword SDMA linear write packet: writes 0xDEADBEEF to target
    const uint64_t targetMc = mcBase + kSdmaTargetVramOffset;
    const uint32_t sdmaPkt[5] = {
        kSdmaOpWrite | (kSdmaSubopWriteLinear << 8),
        static_cast<uint32_t>(targetMc),
        static_cast<uint32_t>(targetMc >> 32),
        0u,             // count - 1 (1 dword)
        0xDEADBEEFu,    // value
    };
    for (uint32_t i = 0; i < 5; ++i)
        vramWrite(bar, kSdmaRingVramOffset + i * 4, sdmaPkt[i]);

    // Program SDMA0 Queue 0
    const uint64_t sdmaRingMc = mcBase + kSdmaRingVramOffset;
    const uint64_t sdmaWptrMc = mcBase + kSdmaWptrShadowOffset;
    const uint64_t sdmaRptrMc = mcBase + kSdmaRptrShadowOffset;

    gcWr(0, cobalt64::navi33::gc::regSDMA0_SEM_WAIT_FAIL_TIMER_CNTL, 0);
    // RB_SIZE = order_base_2(4096 / 4) = 9
    uint32_t rbCntl = (9u << 1) | (1u << 23); // RB_SIZE=9, RB_PRIV=1
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_CNTL, rbCntl);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_RPTR, 0);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_RPTR_HI, 0);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_WPTR, 0);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_WPTR_HI, 0);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_LO, static_cast<uint32_t>(sdmaWptrMc));
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_HI, static_cast<uint32_t>(sdmaWptrMc >> 32));
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_RPTR_ADDR_LO, static_cast<uint32_t>(sdmaRptrMc & ~3ull));
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_RPTR_ADDR_HI, static_cast<uint32_t>(sdmaRptrMc >> 32));

    rbCntl |= (1u << 26) | (1u << 24); // RPTR_WRITEBACK_ENABLE | F32_WPTR_POLL_ENABLE
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_CNTL, rbCntl);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_BASE, static_cast<uint32_t>(sdmaRingMc >> 8));
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_BASE_HI, static_cast<uint32_t>(sdmaRingMc >> 40));
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_DOORBELL, 0);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_IB_CNTL, 1); // IB_ENABLE

    // Unhalt SDMA0 engine
    gcWr(1, cobalt64::navi33::gc::regSDMA0_F32_CNTL, 0);
    rbCntl |= 1u; // RB_ENABLE
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_CNTL, rbCntl);

    // Submit: 5 dwords = 20 bytes
    vramWrite(bar, kSdmaWptrShadowOffset, 5u << 2);
    vramWrite(bar, kSdmaWptrShadowOffset + 4, 0);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_WPTR, 5u << 2);
    gcWr(0, cobalt64::navi33::gc::regSDMA0_QUEUE0_RB_WPTR_HI, 0);

    N33Q_STEP("SDMA0 packet submitted; waiting for writeback");
    bool sdmaOk = false;
    uint32_t sdmaVal = 0;
    for (uint32_t ms = 0; ms < 2000; ++ms) {
        vramRead(bar, kSdmaTargetVramOffset, &sdmaVal);
        if (sdmaVal == 0xDEADBEEFu) {
            sdmaOk = true;
            break;
        }
        IOSleep(1);
    }
    N33Q_STEP("SDMA0 result: 0x%08x (expect 0xDEADBEEF) -> %s",
              sdmaVal, sdmaOk ? "PASS" : "FAIL");

    // ----------------------------------------------------
    // Part 2: GFX 11.0.2 CP Ring Test
    // ----------------------------------------------------
    N33Q_STEP("Configuring RS64 program counters and CP Ring 0");
    // GRBM select helper: select ME 0, Pipe 0, Queue 0
    auto grbmSelect = [&](uint32_t me, uint32_t pipe, uint32_t queue) {
        const uint32_t t = (pipe & 3u) | ((me & 3u) << 2) | ((queue & 7u) << 6);
        gcWr(1, cobalt64::navi33::gc::regGRBM_GFX_CNTL, t);
    };

    // Configure PFP entry point
    for (uint32_t pipe = 0; pipe < 2; ++pipe) {
        grbmSelect(0, pipe, 0);
        gcWr(1, cobalt64::navi33::gc::regCP_PFP_PRGRM_CNTR_START,
             (kPfpStartHi << 30) | (kPfpStartLo >> 2));
        gcWr(1, cobalt64::navi33::gc::regCP_PFP_PRGRM_CNTR_START_HI, kPfpStartHi >> 2);
    }
    grbmSelect(0, 0, 0);

    // Pulse reset PFP
    gcWr(1, cobalt64::navi33::gc::regCP_ME_CNTL, 0x00000003u); // PFP_PIPE0/1_RESET
    gcWr(1, cobalt64::navi33::gc::regCP_ME_CNTL, 0x00000000u);

    // Configure ME entry point
    for (uint32_t pipe = 0; pipe < 2; ++pipe) {
        grbmSelect(0, pipe, 0);
        gcWr(1, cobalt64::navi33::gc::regCP_ME_PRGRM_CNTR_START,
             (kMeStartHi << 30) | (kMeStartLo >> 2));
        gcWr(1, cobalt64::navi33::gc::regCP_ME_PRGRM_CNTR_START_HI, kMeStartHi >> 2);
    }
    grbmSelect(0, 0, 0);

    // Pulse reset ME
    gcWr(1, cobalt64::navi33::gc::regCP_ME_CNTL, 0x0000000Cu); // ME_PIPE0/1_RESET
    gcWr(1, cobalt64::navi33::gc::regCP_ME_CNTL, 0x00000000u);

    // Configure MEC entry point
    for (uint32_t pipe = 0; pipe < 4; ++pipe) {
        grbmSelect(1, pipe, 0);
        gcWr(1, cobalt64::navi33::gc::regCP_MEC_RS64_PRGRM_CNTR_START,
             (kMecStartLo >> 2) | (kMecStartHi << 30));
        gcWr(1, cobalt64::navi33::gc::regCP_MEC_RS64_PRGRM_CNTR_START_HI, kMecStartHi >> 2);
    }
    grbmSelect(0, 0, 0);

    // Pulse reset MEC
    gcWr(1, cobalt64::navi33::gc::regCP_MEC_RS64_CNTL, 0x0000000Fu);
    gcWr(1, cobalt64::navi33::gc::regCP_MEC_RS64_CNTL, 0x00000000u);

    // Prepare CP Ring 0 buffer in VRAM
    for (uint32_t i = 0; i < kGfxRingBytes; i += 4)
        vramWrite(bar, kGfxRingVramOffset + i, 0);

    // PM4 packet: SET_UCONFIG_REG SCRATCH_REG0 = 0xDEADBEEF
    const uint32_t scratchOffset = 0xC000u + 0x40u; // offset from SET_UCONFIG_REG_START
    const uint32_t gfxPkt[8] = {
        makePacket3(kPacket3SetUconfigReg, 1),
        scratchOffset - kPacket3SetUconfigRegStart,
        0xDEADBEEFu,
        makePacket3(kPacket3Nop, 3),
        0u, 0u, 0u, 0u
    };
    for (uint32_t i = 0; i < 8; ++i)
        vramWrite(bar, kGfxRingVramOffset + i * 4, gfxPkt[i]);

    gcWr(1, cobalt64::navi33::gc::regSCRATCH_REG0, 0x00000000u);

    // Resume CP Ring 0
    const uint64_t gfxRingMc = mcBase + kGfxRingVramOffset;
    gcWr(0, cobalt64::navi33::gc::regCP_RB_WPTR_DELAY, 0);
    gcWr(0, cobalt64::navi33::gc::regCP_RB_VMID, 0);

    // order_base_2(4096 / 8) = 9
    const uint32_t cpRbCntl = (9u << 0) | (7u << 8) | (1u << 20); // RB_BUFSZ=9, RB_BLKSZ=7, KMD_QUEUE=1
    gcWr(0, cobalt64::navi33::gc::regCP_RB0_CNTL, cpRbCntl);
    gcWr(0, cobalt64::navi33::gc::regCP_RB0_WPTR, 0);
    gcWr(0, cobalt64::navi33::gc::regCP_RB0_WPTR_HI, 0);
    gcWr(0, cobalt64::navi33::gc::regCP_RB0_BASE, static_cast<uint32_t>(gfxRingMc >> 8));
    gcWr(0, cobalt64::navi33::gc::regCP_RB0_BASE_HI, static_cast<uint32_t>(gfxRingMc >> 40));
    gcWr(0, cobalt64::navi33::gc::regCP_RB_ACTIVE, 1);
    gcWr(0, cobalt64::navi33::gc::regCP_RB_DOORBELL_CONTROL, 0);

    // Unhalt PFP and ME
    gcWr(0, cobalt64::navi33::gc::regCP_MAX_CONTEXT, 7);
    gcWr(0, cobalt64::navi33::gc::regCP_DEVICE_ID, 1);
    gcWr(1, cobalt64::navi33::gc::regCP_ME_CNTL, 0);

    // Submit 8 dwords to CP Ring 0
    gcWr(0, cobalt64::navi33::gc::regCP_RB0_WPTR, 8);
    gcWr(0, cobalt64::navi33::gc::regCP_RB0_WPTR_HI, 0);

    N33Q_STEP("CP Ring 0 packet submitted; waiting for SCRATCH_REG0 update");
    bool gfxOk = false;
    uint32_t scratchVal = 0;
    for (uint32_t ms = 0; ms < 2000; ++ms) {
        gcRd(1, cobalt64::navi33::gc::regSCRATCH_REG0, &scratchVal);
        if (scratchVal == 0xDEADBEEFu) {
            gfxOk = true;
            break;
        }
        IOSleep(1);
    }
    N33Q_STEP("GFX scratch result: 0x%08x (expect 0xDEADBEEF) -> %s",
              scratchVal, gfxOk ? "PASS" : "FAIL");

    const bool queueSuccess = sdmaOk || gfxOk;
    if (queueSuccess) {
        publisher->setProperty("Navi33Bringup,QueueVerified", true);
        publisher->setProperty("Navi33Bringup,QueueStage",
                               "complete: hardware command queue submission verified");
        N33Q_STEP("Queue stage COMPLETE — QueueVerified=true (gate 4/4 OPEN)");
    } else {
        publisher->setProperty("Navi33Bringup,QueueVerified", false);
        publisher->setProperty("Navi33Bringup,QueueStage",
                               "failed: neither SDMA0 nor GFX ring completed packet");
    }

    map->release();
}
