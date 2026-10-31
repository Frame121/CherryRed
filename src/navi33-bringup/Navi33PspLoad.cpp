//
//  Navi33PspLoad.cpp — first WRITE stage: PSP 13.0.7 firmware chain (C1).
//
//  Gated behind `navi33psp_load=1` (KDB only — the smallest possible first
//  write) or `navi33psp_chain=1` (full chain: KDB -> SPL -> SYS_DRV -> SOS),
//  on top of the existing opt-ins (`navi33bringup=1` + `navi33psp=1`).
//
//  This is the point where the kext stops being read-only. Everything it
//  writes is confined to:
//    - the MM_INDEX/MM_DATA/MM_INDEX_HI window (BAR5 byte 0x00/0x04/0x18) to
//      stage firmware into VRAM and read it back,
//    - MP0_C2PMSG_36 / MP0_C2PMSG_35 (the bootloader handshake).
//  PCI config space is READ but never written. The stage aborts instead of
//  proceeding when any pre-condition is not met (fail-closed).
//
#include "Navi33PspLoad.hpp"
#include "Navi33PspMailbox.hpp"
#include "Navi33MmWindow.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSData.h>
#include <string.h>

#define N33PLLOG(fmt, ...) IOLog("Navi33Bringup/PSPLOAD: " fmt "\n", ##__VA_ARGS__)
#define N33PL_STEP(...) do { N33PLLOG(__VA_ARGS__); IOSleep(40); } while (0)

// The 14 RX 7600 blobs are embedded by the Makefile (src/fw/fw_*.c, generated
// by tools/embed-firmware.py from firmware/*.bin — see FIRMWARE-PROVENANCE.md).
// Only the SOS blob is used by this stage.
extern "C" const uint8_t fw_psp_13_0_7_sos[];
extern "C" const size_t  fw_psp_13_0_7_sos_size;

using cobalt64::navi33::psp::RegBar;
using cobalt64::navi33::psp::vramCopyVerify;
using cobalt64::navi33::psp::vramRead;
using cobalt64::navi33::psp::vramWrite;

using cobalt64::navi33::psp::kBootloaderReady;
using cobalt64::navi33::psp::kBootloaderWaitMs;
using cobalt64::navi33::psp::kC2pmsg35Dword;
using cobalt64::navi33::psp::kC2pmsg36Dword;
using cobalt64::navi33::psp::kC2pmsg58Dword;
using cobalt64::navi33::psp::kC2pmsg81Dword;
using cobalt64::navi33::psp::kDiscoverySignature;
using cobalt64::navi33::psp::kDiscoveryTopMargin;
using cobalt64::navi33::psp::kFbLocationBaseDword;
using cobalt64::navi33::psp::kFbLocationTopDword;
using cobalt64::navi33::psp::kMaxLoadSteps;
using cobalt64::navi33::psp::kRccConfigMemsizeByte;
using cobalt64::navi33::psp::kSosAliveWaitMs;
using cobalt64::navi33::psp::kSosPostCmdDelayMs;
using cobalt64::navi33::psp::kStagingVramOffset;
using cobalt64::navi33::psp::LoadStep;
using cobalt64::navi33::psp::bootloaderError;
using cobalt64::navi33::psp::bootloaderReady;
using cobalt64::navi33::psp::buildLoadPlan;
using cobalt64::navi33::psp::mmhubByteOffset;
using cobalt64::navi33::psp::mp0ByteOffset;
using cobalt64::navi33::psp::parseSosHeaderV2;
using cobalt64::navi33::psp::sosAlive;

namespace {

// Poll C2PMSG_35 for the ready bit; returns the last value read and whether
// the wait finished in time.
bool waitBootloaderReady(const RegBar &bar, uint32_t *last, uint32_t *waitedMs)
{
    const uint64_t off = mp0ByteOffset(kC2pmsg35Dword);
    uint32_t v = 0;
    for (uint32_t ms = 0; ms <= kBootloaderWaitMs; ++ms) {
        if (bar.rd(off, &v) && bootloaderReady(v)) {
            *last = v;
            *waitedMs = ms;
            return true;
        }
        IOSleep(1);
    }
    *last = v;
    *waitedMs = kBootloaderWaitMs;
    return false;
}

}  // namespace

void RunPspLoadStage(IOPCIDevice *pci, IOService *publisher, bool chainEnabled)
{
    using cobalt64::navi33::psp::kPciCommandMemorySpace;
    using cobalt64::navi33::psp::kRegBarIndex;

    if (!pci || !publisher)
        return;

    N33PL_STEP("PSP firmware load stage starting (FIRST WRITE STAGE; chain=%u)",
               chainEnabled ? 1u : 0u);

    // Read-only check of the PCI command register (same rule as the survey:
    // never write config space).
    const uint16_t command = pci->configRead16(kIOPCIConfigCommand);
    publisher->setProperty("Navi33Bringup,PspLoad,PciCommandRaw",
                           static_cast<uint64_t>(command), 16);
    if (!(command & kPciCommandMemorySpace)) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: PCI memory decode is off (config space is never written)");
        N33PLLOG("aborted: PCI command 0x%04x has no memory-space bit", command);
        return;
    }

    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(
        static_cast<UInt8>(kIOPCIConfigBaseAddress0 + kRegBarIndex * 4));
    if (!map) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: register BAR5 map failed");
        N33PLLOG("could not map register BAR%u", kRegBarIndex);
        return;
    }
    RegBar bar;
    bar.va = reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress());
    bar.len = static_cast<uint64_t>(map->getLength());
    publisher->setProperty("Navi33Bringup,PspLoad,RegisterBARIndex",
                           static_cast<uint64_t>(kRegBarIndex), 8);
    publisher->setProperty("Navi33Bringup,PspLoad,RegisterBARLength", bar.len, 64);

    // Every address this stage touches must fit the mapped window.
    const uint64_t maxNeed = mmhubByteOffset(kFbLocationTopDword) + 4;
    if (!bar.va || bar.len < maxNeed) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: register BAR5 window too small for the pinned register set");
        N33PLLOG("aborted: BAR5 %llu bytes < %llu needed", (unsigned long long)bar.len,
                 (unsigned long long)maxNeed);
        map->release();
        return;
    }

    // Pre-conditions, all live reads: bootloader must be ready and the secure
    // OS must NOT be running yet (loading over a live sOS is refused).
    // MP0_C2PMSG_33 = kMailboxSpecs[kSpecIfwiInit] (IFWI init done, bit 31).
    const uint64_t c2pmsg33Off = mp0ByteOffset(0x061);
    uint32_t msg33 = 0, msg35 = 0, msg81 = 0;
    bar.rd(c2pmsg33Off, &msg33);
    bar.rd(mp0ByteOffset(kC2pmsg35Dword), &msg35);
    bar.rd(mp0ByteOffset(kC2pmsg81Dword), &msg81);
    publisher->setProperty("Navi33Bringup,PspLoad,PrecheckIfwiInitDone",
                           (msg33 & kBootloaderReady) != 0);
    publisher->setProperty("Navi33Bringup,PspLoad,PrecheckBootloaderReady",
                           bootloaderReady(msg35));
    publisher->setProperty("Navi33Bringup,PspLoad,PrecheckSecureOsWasAlive",
                           sosAlive(msg81));
    N33PL_STEP("prechecks: C2PMSG_33=0x%08x C2PMSG_35=0x%08x C2PMSG_81=0x%08x",
               msg33, msg35, msg81);
    if (!(msg33 & kBootloaderReady) || !bootloaderReady(msg35)) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: PSP bootloader not ready");
        map->release();
        return;
    }
    if (sosAlive(msg81)) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: secure OS already running — refusing to load over it");
        map->release();
        return;
    }

    // Live VRAM topology: MMHUB FB location (24-bit, 16 MiB units) and the
    // NBIF memsize register. Both are published as evidence; the load refuses
    // to run on implausible values.
    uint32_t fbBase = 0, fbTop = 0, memsize = 0;
    const bool fbBaseOk = bar.rd(mmhubByteOffset(kFbLocationBaseDword), &fbBase);
    const bool fbTopOk = bar.rd(mmhubByteOffset(kFbLocationTopDword), &fbTop);
    const bool memsizeOk = bar.rd(kRccConfigMemsizeByte, &memsize);
    publisher->setProperty("Navi33Bringup,PspLoad,FbLocationReadOk",
                           fbBaseOk && fbTopOk);
    publisher->setProperty("Navi33Bringup,PspLoad,FbLocationBase", static_cast<uint64_t>(fbBase), 32);
    publisher->setProperty("Navi33Bringup,PspLoad,FbLocationTop", static_cast<uint64_t>(fbTop), 32);
    publisher->setProperty("Navi33Bringup,PspLoad,RccConfigMemsizeMB",
                           static_cast<uint64_t>(memsize), 32);
    N33PL_STEP("VRAM topology: FB_LOCATION 0x%05x..0x%05x, RCC memsize %u MB%s",
               fbBase, fbTop, memsize, memsizeOk ? "" : " (unreadable)");
    const bool fbSane = fbBaseOk && fbTopOk && fbBase != 0 && fbTop >= fbBase &&
                        fbBase < 0x1000000u && fbTop < 0x1000000u;
    if (!fbSane) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: implausible FB_LOCATION values — not loading firmware");
        map->release();
        return;
    }
    const uint64_t mcBase = static_cast<uint64_t>(fbBase) << 24;
    const uint64_t vramBytes = static_cast<uint64_t>(fbTop - fbBase + 1) << 24;
    publisher->setProperty("Navi33Bringup,PspLoad,VramMcBase", mcBase, 64);
    publisher->setProperty("Navi33Bringup,PspLoad,VramBytes", vramBytes, 64);
    N33PL_STEP("MC base 0x%llx, VRAM %llu MiB (from FB_LOCATION)",
               (unsigned long long)mcBase, (unsigned long long)(vramBytes >> 20));

    // Probe the MM window + the live IP Discovery table (VRAM top - 64 KiB).
    // This also proves the window works before any firmware byte is written.
    const uint64_t discOff = vramBytes - kDiscoveryTopMargin;
    uint32_t discSig = 0;
    const bool discSigOk = vramRead(bar, discOff, &discSig);
    publisher->setProperty("Navi33Bringup,PspLoad,DiscoverySignatureReadOk", discSigOk);
    publisher->setProperty("Navi33Bringup,PspLoad,DiscoverySignatureMatches",
                           discSigOk && discSig == kDiscoverySignature);
    N33PL_STEP("live IP discovery signature @VRAM+0x%llx: 0x%08x (%s)",
               (unsigned long long)discOff, discSig,
               (discSigOk && discSig == kDiscoverySignature) ? "MATCH" : "no match");

    // Parse the embedded SOS image. Refuse to touch the card when the header
    // is not a valid v2.0 image with a loadable plan.
    const uint32_t sosLen = static_cast<uint32_t>(fw_psp_13_0_7_sos_size);
    cobalt64::navi33::psp::SosImage sos;
    if (!parseSosHeaderV2(fw_psp_13_0_7_sos, sosLen, sos)) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: embedded psp_13_0_7_sos.bin failed v2.0 header validation");
        N33PLLOG("aborted: SOS header invalid");
        map->release();
        return;
    }
    publisher->setProperty("Navi33Bringup,PspLoad,SosUcodeVersion",
                           static_cast<uint64_t>(sos.ucodeVersion), 32);
    publisher->setProperty("Navi33Bringup,PspLoad,SosBinCount",
                           static_cast<uint64_t>(sos.binCount), 8);
    N33PL_STEP("embedded psp_13_0_7_sos.bin: %u bytes, ucode 0x%08x, %u bins",
               sosLen, sos.ucodeVersion, sos.binCount);

    LoadStep plan[kMaxLoadSteps];
    const uint32_t steps = buildLoadPlan(sos, plan);
    if (steps == 0) {
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "aborted: no loadable components in the SOS image");
        map->release();
        return;
    }

    // KDB-only mode is the first-ever write: one component, no SOS.
    const uint32_t stepsToRun = chainEnabled ? steps : 1u;
    publisher->setProperty("Navi33Bringup,PspLoad,PlanSteps",
                           static_cast<uint64_t>(steps), 8);
    publisher->setProperty("Navi33Bringup,PspLoad,StepsRun",
                           static_cast<uint64_t>(stepsToRun), 8);
    N33PL_STEP("load plan: %u steps, running %u (%s)", steps, stepsToRun,
               chainEnabled ? "full chain" : "KDB only");

    uint32_t accepted = 0;
    bool chainSosAlive = false;
    uint32_t sosVersion = 0;
    for (uint32_t s = 0; s < stepsToRun; ++s) {
        const LoadStep &step = plan[s];
        N33PL_STEP("step %u/%u: %s (%u bytes, version 0x%08x) -> cmd 0x%08x",
                   s + 1, stepsToRun, step.name, step.sizeBytes, step.version,
                   step.cmd);

        if (step.fileOffset + step.sizeBytes > sosLen) {
            N33PLLOG("step %s: payload escapes the embedded blob — aborted", step.name);
            break;
        }

        uint32_t waited = 0, last35 = 0;
        if (!waitBootloaderReady(bar, &last35, &waited)) {
            N33PLLOG("step %s: bootloader not ready after %u ms (C2PMSG_35=0x%08x) — aborted",
                     step.name, waited, last35);
            break;
        }

        const int32_t mismatch = vramCopyVerify(bar, kStagingVramOffset,
                                                fw_psp_13_0_7_sos + step.fileOffset,
                                                step.sizeBytes);
        if (mismatch < 0) {
            N33PLLOG("step %s: VRAM window failed during staging — aborted", step.name);
            break;
        }
        N33PL_STEP("staged %s: %u bytes at VRAM+0x%llx, %d mismatching dwords",
                   step.name, step.sizeBytes,
                   (unsigned long long)kStagingVramOffset, mismatch);

        const uint64_t mc = mcBase + kStagingVramOffset;
        if (!bar.wr(mp0ByteOffset(kC2pmsg36Dword),
                    static_cast<uint32_t>(mc >> 20))) {
            N33PLLOG("step %s: C2PMSG_36 write failed — aborted", step.name);
            break;
        }
        if (!bar.wr(mp0ByteOffset(kC2pmsg35Dword), step.cmd)) {
            N33PLLOG("step %s: C2PMSG_35 write failed — aborted", step.name);
            break;
        }

        waited = 0;
        last35 = 0;
        const bool ready = waitBootloaderReady(bar, &last35, &waited);
        const uint32_t errBits = bootloaderError(last35);
        N33PL_STEP("step %s: ready=%u C2PMSG_35=0x%08x errBits=0x%02x after %u ms",
                   step.name, ready ? 1u : 0u, last35, errBits, waited);
        if (!ready || errBits != 0)
            break;
        accepted++;

        if (step.fwType == cobalt64::navi33::psp::FwSos) {
            // psp_v13_0_bootloader_load_sos: mdelay(20), then wait for the
            // sign-of-life register to CHANGE, then read the sOS version.
            IOSleep(kSosPostCmdDelayMs);
            uint32_t alive = msg81;
            uint32_t waitedMs = 0;
            for (; waitedMs < kSosAliveWaitMs; ++waitedMs) {
                uint32_t v = 0;
                if (bar.rd(mp0ByteOffset(kC2pmsg81Dword), &v) && v != msg81) {
                    alive = v;
                    break;
                }
                IOSleep(1);
            }
            chainSosAlive = sosAlive(alive);
            bar.rd(mp0ByteOffset(kC2pmsg58Dword), &sosVersion);
            N33PL_STEP("sOS sign of life: 0x%08x (%s) after %u ms; C2PMSG_58=0x%08x",
                       alive, chainSosAlive ? "ALIVE" : "no change", waitedMs,
                       sosVersion);
        }
    }

    publisher->setProperty("Navi33Bringup,PspLoad,StepsAccepted",
                           static_cast<uint64_t>(accepted), 8);
    publisher->setProperty("Navi33Bringup,PspLoad,ChainComplete",
                           chainEnabled && chainSosAlive);
    publisher->setProperty("Navi33Bringup,PspLoad,SecureOsAlive", chainSosAlive);
    publisher->setProperty("Navi33Bringup,PspLoad,SosVersion",
                           static_cast<uint64_t>(sosVersion), 32);

    if (chainEnabled && chainSosAlive) {
        // The accelerator gate's firmware leg (Navi33Accelerator reads
        // Navi33Bringup,FirmwareVerified). Memory/Queue verified legs are the
        // C2/C3 milestones and stay unset.
        publisher->setProperty("Navi33Bringup,FirmwareVerified", true);
        publisher->setProperty("Navi33Bringup,FirmwareLoaded", true);
        publisher->setProperty("Navi33Bringup,FirmwareRefused", false);
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               "chain-complete: sOS alive, firmware leg of the compute gate verified");
        N33PL_STEP("PSP chain COMPLETE: secure OS alive (version 0x%08x)", sosVersion);
    } else {
        publisher->setProperty("Navi33Bringup,FirmwareVerified", false);
        publisher->setProperty("Navi33Bringup,FirmwareLoaded", false);
        publisher->setProperty("Navi33Bringup,PspLoad,Stage",
                               chainEnabled ? "chain incomplete — evidence captured, no verification claimed"
                                            : "kdb-only run complete — smallest write exercised");
        N33PLLOG("stage done: %u/%u steps accepted, chain complete=%u",
                 accepted, stepsToRun, chainEnabled && chainSosAlive ? 1u : 0u);
    }

    map->release();
}
