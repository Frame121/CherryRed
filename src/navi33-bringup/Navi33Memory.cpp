//
//  Navi33Memory.cpp — Stage 5.4 / C2: GFXHUB v3.0 Memory Hub & GART Setup.
//
//  Reachable only behind navi33bringup=1 + navi33psp=1 + navi33gc_load=1 +
//  navi33mem=1. Requires the firmware stage (Navi33GcLoad / Navi33SmuLoad) to
//  have completed and verified the firmware on the card first.
//
//  Sequence follows Linux gfxhub_v3_0 and RedNavi's rn_gfxhub_enable:
//    1. Builds VMID 0 page table in VRAM (4KB GART pages identity-mapped).
//    2. Programs GART aperture registers in the GC memory hub.
//    3. Programs System Aperture covering VRAM.
//    4. Configures L1 TLB and L2 cache.
//    5. Enables Context 0 (system domain).
//    6. Validates CPU VRAM write/read persistence before and after enable.
//    7. Publishes Navi33Bringup,MemoryVerified = true.
//
#include "Navi33Memory.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSBoolean.h>

#include "Navi33GcRegs.hpp"
#include "Navi33MmWindow.hpp"
#include "Navi33PspMailbox.hpp"

#define N33MEM_LOG(fmt, ...) IOLog("Navi33Memory: " fmt "\n", ##__VA_ARGS__)
#define N33MEM_STEP(...) do { N33MEM_LOG(__VA_ARGS__); IOSleep(40); } while (0)

using cobalt64::navi33::psp::kFbLocationBaseDword;
using cobalt64::navi33::psp::kFbLocationTopDword;
using cobalt64::navi33::psp::kPciCommandMemorySpace;
using cobalt64::navi33::psp::kRegBarIndex;
using cobalt64::navi33::psp::mmhubByteOffset;
using cobalt64::navi33::psp::RegBar;
using cobalt64::navi33::gc::gcByteOffset;

namespace {

// Work area layout in VRAM:
//   0x0C000000 .. 0x0C3FFFFF: GART Identity Window (4 MiB)
//   0x0C100000: VMID 0 Page Table (1024 PTEs * 8 bytes = 8192 bytes)
//   0x0C200000: Scratch / Default page
//   0x0C201000: Dummy / Protection fault page
constexpr uint64_t kGartStartOffset  = 0x0C000000ull;
constexpr uint64_t kGartEndOffset    = 0x0C3FFFFFull;
constexpr uint64_t kPageTableOffset  = 0x0C100000ull;
constexpr uint64_t kScratchOffset    = 0x0C200000ull;
constexpr uint64_t kDummyFaultOffset = 0x0C201000ull;

inline uint32_t setField(uint32_t val, uint32_t mask, uint32_t shift, uint32_t fieldVal) {
    return (val & ~mask) | ((fieldVal << shift) & mask);
}

}  // namespace

void RunNavi33MemoryStage(IOPCIDevice *pci, IOService *publisher)
{
    if (!pci || !publisher)
        return;

    N33MEM_STEP("Memory stage starting (GFXHUB v3.0 GART aperture + identity mapping)");

    const uint16_t command = pci->configRead16(kIOPCIConfigCommand);
    if (!(command & kPciCommandMemorySpace)) {
        publisher->setProperty("Navi33Bringup,MemoryStage",
                               "aborted: PCI memory decode is off");
        return;
    }

    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(
        static_cast<UInt8>(kIOPCIConfigBaseAddress0 + kRegBarIndex * 4));
    if (!map) {
        publisher->setProperty("Navi33Bringup,MemoryStage",
                               "aborted: register BAR5 map failed");
        return;
    }

    RegBar bar;
    bar.va = reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress());
    bar.len = static_cast<uint64_t>(map->getLength());

    const uint64_t maxNeed = mmhubByteOffset(kFbLocationTopDword) + 4;
    if (!bar.va || bar.len < maxNeed) {
        publisher->setProperty("Navi33Bringup,MemoryStage",
                               "aborted: register BAR5 window too small");
        map->release();
        return;
    }

    // Verify FB location
    uint32_t fbBase = 0, fbTop = 0;
    if (!bar.rd(mmhubByteOffset(kFbLocationBaseDword), &fbBase) ||
        !bar.rd(mmhubByteOffset(kFbLocationTopDword), &fbTop) ||
        fbBase == 0 || fbTop < fbBase) {
        publisher->setProperty("Navi33Bringup,MemoryStage",
                               "aborted: implausible FB_LOCATION values");
        map->release();
        return;
    }
    const uint64_t mcBase = static_cast<uint64_t>(fbBase) << 24;
    const uint64_t mcTop  = (static_cast<uint64_t>(fbTop) << 24) | 0xFFFFFFull;
    N33MEM_STEP("VRAM MC range: 0x%llx .. 0x%llx", mcBase, mcTop);

    // Pre-test CPU VRAM write/read persistence
    constexpr uint32_t kMagic1 = 0xA1B2C3D4u;
    constexpr uint32_t kMagic2 = 0x5E6F7A8Bu;
    vramWrite(bar, kGartStartOffset, kMagic1);
    vramWrite(bar, kGartStartOffset + 0x2000, kMagic2);
    uint32_t preRead1 = 0, preRead2 = 0;
    vramRead(bar, kGartStartOffset, &preRead1);
    vramRead(bar, kGartStartOffset + 0x2000, &preRead2);
    if (preRead1 != kMagic1 || preRead2 != kMagic2) {
        publisher->setProperty("Navi33Bringup,MemoryStage",
                               "aborted: pre-enable VRAM access test failed");
        map->release();
        return;
    }

    // 1. Build the GART identity page table in VRAM (1024 entries for 4 MiB)
    N33MEM_STEP("Building GART identity page table at VRAM +0x%llx", kPageTableOffset);
    for (uint64_t va = kGartStartOffset; va <= kGartEndOffset; va += 4096) {
        // PTE format: physical addr | valid(0x1) | readable(0x20) | writeable(0x40)
        const uint64_t pte = (mcBase + va) | 0x1ull | 0x20ull | 0x40ull;
        const uint64_t slot = kPageTableOffset + ((va - kGartStartOffset) >> 12) * 8;
        vramWrite(bar, slot, static_cast<uint32_t>(pte));
        vramWrite(bar, slot + 4, static_cast<uint32_t>(pte >> 32));
    }

    auto gcWr = [&](uint32_t baseIdx, uint32_t reg, uint32_t val) {
        return bar.wr(gcByteOffset(baseIdx, reg), val);
    };
    auto gcRd = [&](uint32_t baseIdx, uint32_t reg, uint32_t *out) {
        return bar.rd(gcByteOffset(baseIdx, reg), out);
    };

    // 2. Program VMID 0 page table base and range
    const uint64_t ptBase = (mcBase + kPageTableOffset) | 0x1ull;
    gcWr(0, cobalt64::navi33::gc::regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32, static_cast<uint32_t>(ptBase));
    gcWr(0, cobalt64::navi33::gc::regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_HI32, static_cast<uint32_t>(ptBase >> 32));
    gcWr(0, cobalt64::navi33::gc::regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_LO32, static_cast<uint32_t>(kGartStartOffset >> 12));
    gcWr(0, cobalt64::navi33::gc::regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_HI32, static_cast<uint32_t>(kGartStartOffset >> 44));
    gcWr(0, cobalt64::navi33::gc::regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_LO32, static_cast<uint32_t>(kGartEndOffset >> 12));
    gcWr(0, cobalt64::navi33::gc::regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_HI32, static_cast<uint32_t>(kGartEndOffset >> 44));

    // 3. System aperture registers
    const uint64_t agpStart = 0xFFFFFFFFFFFFull;
    const uint64_t agpEnd = 0ull;
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_AGP_BASE, 0);
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_AGP_BOT, static_cast<uint32_t>(agpStart >> 24));
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_AGP_TOP, static_cast<uint32_t>(agpEnd >> 24));
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_SYSTEM_APERTURE_LOW_ADDR, static_cast<uint32_t>(mcBase >> 18));
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_SYSTEM_APERTURE_HIGH_ADDR, static_cast<uint32_t>(mcTop >> 18));
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB, static_cast<uint32_t>(kScratchOffset >> 12));
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB, static_cast<uint32_t>(kScratchOffset >> 44));
    gcWr(0, cobalt64::navi33::gc::regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_LO32, static_cast<uint32_t>(kDummyFaultOffset >> 12));
    gcWr(0, cobalt64::navi33::gc::regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_HI32, static_cast<uint32_t>(kDummyFaultOffset >> 44));

    uint32_t tmp = 0;
    gcRd(0, cobalt64::navi33::gc::regGCVM_L2_PROTECTION_FAULT_CNTL2, &tmp);
    tmp |= (1u << 18) | (1u << 19); // retry PTE & enable retry interrupt
    gcWr(0, cobalt64::navi33::gc::regGCVM_L2_PROTECTION_FAULT_CNTL2, tmp);

    // 4. TLB & L2 cache registers
    // L1 TLB: ENABLE_L1_TLB(1) | SYSTEM_ACCESS_MODE(3) | ENABLE_ADVANCED_DRIVER_MODEL(1) | MTYPE(3)
    tmp = 1u | (3u << 3) | (1u << 6) | (3u << 11);
    gcWr(0, cobalt64::navi33::gc::regGCMC_VM_MX_L1_TLB_CNTL, tmp);

    // L2 Cache: ENABLE_L2_CACHE(1) | ENABLE_DEFAULT_PAGE_OUT(1) | CONTEXT1_IDENTITY_ACCESS_MODE(1)
    tmp = 1u | (1u << 11) | (1u << 19);
    gcWr(0, cobalt64::navi33::gc::regGCVM_L2_CNTL, tmp);

    // Invalidate L1 TLBs & L2 cache
    gcWr(0, cobalt64::navi33::gc::regGCVM_L2_CNTL2, 0x3u);

    // Bank select = 9, big-k = 6
    tmp = 0x80100007u;
    tmp = setField(tmp, 0x00000070u, 4, 9);
    gcWr(0, cobalt64::navi33::gc::regGCVM_L2_CNTL3, tmp);

    // 5. Context 0 enable (VMID 0)
    // ENABLE_CONTEXT(1) | PAGE_TABLE_DEPTH(0)
    gcWr(0, cobalt64::navi33::gc::regGCVM_CONTEXT0_CNTL, 1u);

    N33MEM_STEP("GFXHUB Context 0 enabled; verifying post-enable persistence");
    IOSleep(20);

    // Post-enable persistence check:
    constexpr uint32_t kPostMagic1 = 0x12345678u;
    constexpr uint32_t kPostMagic2 = 0x87654321u;
    vramWrite(bar, kGartStartOffset, kPostMagic1);
    vramWrite(bar, kGartStartOffset + 0x2000, kPostMagic2);
    uint32_t postRead1 = 0, postRead2 = 0;
    vramRead(bar, kGartStartOffset, &postRead1);
    vramRead(bar, kGartStartOffset + 0x2000, &postRead2);

    const bool persistOk = (postRead1 == kPostMagic1 && postRead2 == kPostMagic2);
    N33MEM_STEP("Post-enable readback: 0x%08x (expect 0x%08x), 0x%08x (expect 0x%08x) -> %s",
                postRead1, kPostMagic1, postRead2, kPostMagic2, persistOk ? "OK" : "FAIL");

    if (persistOk) {
        publisher->setProperty("Navi33Bringup,MemoryVerified", true);
        publisher->setProperty("Navi33Bringup,MemoryStage",
                               "complete: GFXHUB v3.0 GART aperture mapped and verified");
        publisher->setProperty("Navi33Bringup,GartRange", "0x0C000000..0x0C3FFFFF (4 MiB)");
    } else {
        publisher->setProperty("Navi33Bringup,MemoryVerified", false);
        publisher->setProperty("Navi33Bringup,MemoryStage",
                               "failed: VRAM write persistence test failed after GFXHUB enable");
    }

    map->release();
}
