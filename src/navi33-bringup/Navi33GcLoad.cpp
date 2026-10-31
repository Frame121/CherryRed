//
//  Navi33GcLoad.cpp — GC firmware stage: load the GC 11.0.2 / SDMA 6.0.2
//  image set (26 images) through the PSP ring with GFX_CMD_ID_LOAD_IP_FW, in
//  the AMDGPU_UCODE_ID order, then start the RLC autoload once RLC_G is in,
//  and finally read the RLC autoload completion status (read-only GC
//  registers). Linux order: psp_load_non_psp_fw + gfx_v11_0 wait-for-autoload.
//
//  Reachable only behind navi33bringup=1 + navi33psp=1 + navi33psp_load=1 +
//  navi33psp_ring=1 + navi33psp_smu=1 + navi33gc_load=1 (the SMU must answer
//  first — Linux loads SMU before the other firmware). Write class: the MM
//  index/data window and PSP ring mailboxes through the bounds-checked
//  RegBar; the GC registers at the end are READS ONLY.
//
#include "Navi33GcLoad.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>

#include "Navi33MmWindow.hpp"
#include "Navi33PspMailbox.hpp"
#include "Navi33PspRing.hpp"
#include "Navi33RingSubmit.hpp"
#include "Navi33SmuLoad.hpp"

extern "C" const uint8_t fw_gc_11_0_2_pfp[];
extern "C" const size_t  fw_gc_11_0_2_pfp_size;
extern "C" const uint8_t fw_gc_11_0_2_me[];
extern "C" const size_t  fw_gc_11_0_2_me_size;
extern "C" const uint8_t fw_gc_11_0_2_mec[];
extern "C" const size_t  fw_gc_11_0_2_mec_size;
extern "C" const uint8_t fw_gc_11_0_2_rlc[];
extern "C" const size_t  fw_gc_11_0_2_rlc_size;
extern "C" const uint8_t fw_gc_11_0_2_imu[];
extern "C" const size_t  fw_gc_11_0_2_imu_size;
extern "C" const uint8_t fw_gc_11_0_2_mes_2[];
extern "C" const size_t  fw_gc_11_0_2_mes_2_size;
extern "C" const uint8_t fw_gc_11_0_2_mes1[];
extern "C" const size_t  fw_gc_11_0_2_mes1_size;
extern "C" const uint8_t fw_sdma_6_0_2[];
extern "C" const size_t  fw_sdma_6_0_2_size;

#define N33GC_LOG(fmt, ...) IOLog("Navi33GcLoad: " fmt "\n", ##__VA_ARGS__)
#define N33GC_STEP(...) do { N33GC_LOG(__VA_ARGS__); IOSleep(40); } while (0)

namespace {

using cobalt64::navi33::psp::GcBlobRef;

// GFX_CMD_ID_AUTOLOAD_RLC (psp_gfx_if.h); RLC_G is the trigger (type 8).
constexpr uint32_t kCmdAutoloadRlc = 0x21u;
constexpr uint32_t kGcFenceIndexBase = 200u;

// GC 11 register dwords for the autoload-completion read. The GC 11 register
// file (gc_11_0_0_offset.h) is shared across 11.0.x; the offsets are verified
// on 11.0.3 hardware by RedNavi and expected identical on 11.0.2 — the reads
// are the evidence that confirms or corrects that.
constexpr uint32_t kGcInst0BaseDwords = 0x1260;  // captured table: GC base[0]
constexpr uint32_t kGcInst1BaseDwords = 0x0A000; // captured table: GC base[1]
constexpr uint32_t kCpStatDword = 0xf40;         // regCP_STAT
constexpr uint32_t kRlcsBootloadStatusDword = 0x4e82;  // regRLC_RLCS_BOOTLOAD_STATUS
constexpr uint32_t kGcAutoloadWaitMs = 2000;

}  // namespace

void RunNavi33GcLoadStage(IOPCIDevice *pci, IOService *publisher)
{
    using cobalt64::navi33::psp::kC2pmsg81Dword;
    using cobalt64::navi33::psp::kFbLocationBaseDword;
    using cobalt64::navi33::psp::kFbLocationTopDword;
    using cobalt64::navi33::psp::kPciCommandMemorySpace;
    using cobalt64::navi33::psp::kRegBarIndex;
    using cobalt64::navi33::psp::kStagingVramOffset;
    using cobalt64::navi33::psp::mmhubByteOffset;
    using cobalt64::navi33::psp::mp0ByteOffset;

    if (!pci || !publisher)
        return;

    N33GC_STEP("GC firmware load starting (LOAD_IP_FW image set + RLC autoload)");

    const uint16_t command = pci->configRead16(kIOPCIConfigCommand);
    if (!(command & kPciCommandMemorySpace)) {
        publisher->setProperty("Navi33Bringup,GcLoad",
                               "aborted: PCI memory decode is off (config space is never written)");
        return;
    }

    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(
        static_cast<UInt8>(kIOPCIConfigBaseAddress0 + kRegBarIndex * 4));
    if (!map) {
        publisher->setProperty("Navi33Bringup,GcLoad",
                               "aborted: register BAR5 map failed");
        return;
    }
    cobalt64::navi33::psp::RegBar bar;
    bar.va = reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress());
    bar.len = static_cast<uint64_t>(map->getLength());
    const uint64_t maxNeed = mmhubByteOffset(kFbLocationTopDword) + 4;
    if (!bar.va || bar.len < maxNeed) {
        publisher->setProperty("Navi33Bringup,GcLoad",
                               "aborted: register BAR5 window too small");
        map->release();
        return;
    }

    // The secure OS must be alive (ring + SMU stages ran earlier this boot).
    uint32_t alive = 0;
    bar.rd(mp0ByteOffset(kC2pmsg81Dword), &alive);
    if (!cobalt64::navi33::psp::sosAlive(alive)) {
        publisher->setProperty("Navi33Bringup,GcLoad",
                               "aborted: secure OS not alive (run navi33psp_chain=1 first)");
        map->release();
        return;
    }

    uint32_t fbBase = 0, fbTop = 0;
    if (!bar.rd(mmhubByteOffset(kFbLocationBaseDword), &fbBase) ||
        !bar.rd(mmhubByteOffset(kFbLocationTopDword), &fbTop) ||
        fbBase == 0 || fbTop < fbBase) {
        publisher->setProperty("Navi33Bringup,GcLoad",
                               "aborted: implausible FB_LOCATION values");
        map->release();
        return;
    }
    const uint64_t mcBase = static_cast<uint64_t>(fbBase) << 24;

    const GcBlobRef blobs[cobalt64::navi33::psp::kGcBlobCount] = {
        {fw_gc_11_0_2_pfp, static_cast<uint32_t>(fw_gc_11_0_2_pfp_size)},
        {fw_gc_11_0_2_me, static_cast<uint32_t>(fw_gc_11_0_2_me_size)},
        {fw_gc_11_0_2_mec, static_cast<uint32_t>(fw_gc_11_0_2_mec_size)},
        {fw_gc_11_0_2_rlc, static_cast<uint32_t>(fw_gc_11_0_2_rlc_size)},
        {fw_gc_11_0_2_imu, static_cast<uint32_t>(fw_gc_11_0_2_imu_size)},
        {fw_gc_11_0_2_mes_2, static_cast<uint32_t>(fw_gc_11_0_2_mes_2_size)},
        {fw_gc_11_0_2_mes1, static_cast<uint32_t>(fw_gc_11_0_2_mes1_size)},
        {fw_sdma_6_0_2, static_cast<uint32_t>(fw_sdma_6_0_2_size)},
    };
    cobalt64::navi33::psp::GcImage images[cobalt64::navi33::psp::kMaxGcImages];
    const uint32_t imageCount =
        cobalt64::navi33::psp::buildGcImagePlan(blobs, images);
    if (imageCount == 0) {
        publisher->setProperty("Navi33Bringup,GcLoad",
                               "aborted: GC blob headers did not parse (fail-closed)");
        map->release();
        return;
    }
    N33GC_STEP("GC image plan: %u images", imageCount);

    uint32_t loaded = 0;
    uint32_t failedIndex = 0xFFFFFFFFu;
    uint32_t failedStatus = 0;
    uint32_t autoloadStatus = 0xFFFFFFFEu;
    bool autoloadAttempted = false;
    for (uint32_t i = 0; i < imageCount; ++i) {
        const cobalt64::navi33::psp::GcImage &image = images[i];
        if (image.sizeBytes > (1u << 20)) {
            N33GC_LOG("%s: larger than the 1 MiB staging buffer", image.name);
            failedIndex = i;
            break;
        }
        const int32_t mismatch = cobalt64::navi33::psp::vramCopyVerify(
            bar, kStagingVramOffset, blobs[image.blobIndex].data + image.offsetBytes,
            image.sizeBytes);
        if (mismatch != 0) {
            N33GC_LOG("%s: staging copy mismatch=%d", image.name, mismatch);
            failedIndex = i;
            break;
        }
        const uint64_t fwMc = mcBase + kStagingVramOffset;
        const uint32_t args[4] = {static_cast<uint32_t>(fwMc),
                                  static_cast<uint32_t>(fwMc >> 32),
                                  image.sizeBytes, image.fwType};
        uint32_t resp[4] = {};
        const bool ok = cobalt64::navi33::psp::ringSubmit(
            bar, mcBase, kCmdLoadIpFw, args, 4, resp, 4,
            kGcFenceIndexBase + i);
        N33GC_LOG("%s (type %u, %u bytes): fence %s, status 0x%08x",
                  image.name, image.fwType, image.sizeBytes,
                  ok ? "ok" : "missing", resp[0]);
        if (!ok || resp[0] != 0) {
            failedIndex = i;
            failedStatus = resp[0];
            break;
        }
        ++loaded;
        if (image.fwType == cobalt64::navi33::psp::kGfxFwTypeRlcG) {
            // All graphics firmware is in: start the RLC autoload.
            uint32_t r[1] = {};
            autoloadAttempted = true;
            const bool a = cobalt64::navi33::psp::ringSubmit(
                bar, mcBase, kCmdAutoloadRlc, nullptr, 0, r, 1,
                kGcFenceIndexBase + imageCount);
            autoloadStatus = a ? r[0] : 0xFFFFFFFEu;
            N33GC_STEP("AUTOLOAD_RLC: fence %s, status 0x%08x",
                       a ? "ok" : "missing", r[0]);
            if (!a || r[0] != 0)
                break;
        }
    }

    // RLC autoload completion (gfx_v11_0_wait_for_rlc_autoload_complete):
    // CP_STAT must read 0 and RLC_RLCS_BOOTLOAD_STATUS bit 31 must set. Reads
    // only — the graphics block is never written from this stage.
    uint32_t cpStat = 0xFFFFFFFFu;
    uint32_t bootloadStatus = 0;
    uint32_t waitedMs = kGcAutoloadWaitMs;
    if (autoloadAttempted && autoloadStatus == 0) {
        const uint64_t cpOff = (static_cast<uint64_t>(kGcInst0BaseDwords) + kCpStatDword) * 4;
        const uint64_t bootOff =
            (static_cast<uint64_t>(kGcInst1BaseDwords) + kRlcsBootloadStatusDword) * 4;
        if (bar.len >= cpOff + 4 && bar.len >= bootOff + 4) {
            uint32_t ms = 0;
            for (; ms < kGcAutoloadWaitMs; ++ms) {
                bar.rd(cpOff, &cpStat);
                bar.rd(bootOff, &bootloadStatus);
                if (cpStat == 0 && (bootloadStatus & 0x80000000u))
                    break;
                IOSleep(1);
            }
            waitedMs = ms;
        }
    }
    N33GC_STEP("GC autoload: CP_STAT=0x%08x BOOTLOAD_STATUS=0x%08x after %u ms",
               cpStat, bootloadStatus, waitedMs);

    const char *const names[7] = {"images",   "loaded",       "failed_index",
                                  "failed_status", "autoload_status", "cp_stat",
                                  "bootload_status"};
    const uint64_t values[7] = {imageCount, loaded,  failedIndex,     failedStatus,
                                autoloadStatus, cpStat, bootloadStatus};
    OSDictionary *out = OSDictionary::withCapacity(8);
    if (out) {
        for (unsigned i = 0; i < 7; ++i) {
            OSNumber *n = OSNumber::withNumber(values[i], 64);
            if (n) {
                out->setObject(names[i], n);
                n->release();
            }
        }
        publisher->setProperty("Navi33Bringup,GcLoad", out);
        out->release();
    }
    map->release();
}
