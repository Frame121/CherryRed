//
//  Navi33PspRing.cpp — PSP GPCOM ring + LOAD_TOC + SETUP_TMR (completes M2a).
//
//  Gated behind `navi33psp_ring=1` on top of the existing opt-ins
//  (navi33bringup=1 + navi33psp=1). The ring stage REQUIRES the secure OS to
//  be alive (so in practice it runs after `navi33psp_chain=1` in the same
//  boot) and aborts with evidence otherwise.
//
//  TMR placement: `navi33psp_tmr=1` (default) = VRAM top - 512 MiB, the Linux
//  placement RedNavi verified on the 16 GiB Navi 32; `navi33psp_tmr=2` = the
//  32 MiB diagnostic placement inside the CPU-visible BAR window. The Navi 33
//  values are UNVERIFIED until this stage runs on the RX 7600.
//
//  Writes are confined to the same pinned set as the load stage plus the ring
//  mailboxes (C2PMSG_64/67/69/70/71) and the MM window. PCI config space is
//  read, never written.
//
#include "Navi33PspRing.hpp"
#include "Navi33PspMailbox.hpp"
#include "Navi33MmWindow.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <string.h>

#define N33RL(fmt, ...) IOLog("Navi33Bringup/PSPRING: " fmt "\n", ##__VA_ARGS__)
#define N33RL_STEP(...) do { N33RL(__VA_ARGS__); IOSleep(40); } while (0)

extern "C" const uint8_t fw_psp_13_0_7_sos[];
extern "C" const size_t  fw_psp_13_0_7_sos_size;

using cobalt64::navi33::psp::RegBar;
using cobalt64::navi33::psp::vramCopyVerify;
using cobalt64::navi33::psp::vramRead;
using cobalt64::navi33::psp::vramWrite;
using cobalt64::navi33::psp::FwToc;
using cobalt64::navi33::psp::findBin;
using cobalt64::navi33::psp::parseSosHeaderV2;
using cobalt64::navi33::psp::kC2pmsg64Dword;
using cobalt64::navi33::psp::kC2pmsg67Dword;
using cobalt64::navi33::psp::kC2pmsg69Dword;
using cobalt64::navi33::psp::kC2pmsg70Dword;
using cobalt64::navi33::psp::kC2pmsg71Dword;
using cobalt64::navi33::psp::kC2pmsg81Dword;
using cobalt64::navi33::psp::kCmdBufArgsOff;
using cobalt64::navi33::psp::kCmdBufBytes;
using cobalt64::navi33::psp::kCmdBufCmdIdOff;
using cobalt64::navi33::psp::kCmdBufRespOff;
using cobalt64::navi33::psp::kCmdBufVramOffset;
using cobalt64::navi33::psp::kCmdLoadToc;
using cobalt64::navi33::psp::kCmdSetupTmr;
using cobalt64::navi33::psp::kFbLocationBaseDword;
using cobalt64::navi33::psp::kFbLocationTopDword;
using cobalt64::navi33::psp::kFenceBufVramOffset;
using cobalt64::navi33::psp::kFenceWaitMs;
using cobalt64::navi33::psp::kFrameBytes;
using cobalt64::navi33::psp::kFrameCmdHi;
using cobalt64::navi33::psp::kFrameCmdLo;
using cobalt64::navi33::psp::kFrameFenceHi;
using cobalt64::navi33::psp::kFrameFenceLo;
using cobalt64::navi33::psp::kFrameFenceValue;
using cobalt64::navi33::psp::kRingPostCmdDelayMs;
using cobalt64::navi33::psp::kRingReadyWaitMs;
using cobalt64::navi33::psp::kRingSizeBytes;
using cobalt64::navi33::psp::kRingTypeKm;
using cobalt64::navi33::psp::kRingVramOffset;
using cobalt64::navi33::psp::kSetupTmrFlagsPhyAddr;
using cobalt64::navi33::psp::kTocStagingVramOffset;
using cobalt64::navi33::psp::kTmrModeLinuxLike;
using cobalt64::navi33::psp::kTmrModeVisibleBar;
using cobalt64::navi33::psp::frameVramOffset;
using cobalt64::navi33::psp::loadTocTmrSizeIndex;
using cobalt64::navi33::psp::mp0ByteOffset;
using cobalt64::navi33::psp::nextWptr;
using cobalt64::navi33::psp::ringCreateAccepted;
using cobalt64::navi33::psp::sosAlive;
using cobalt64::navi33::psp::tmrOffsetForMode;
using cobalt64::navi33::psp::wptrValid;

namespace {

// Wait until (C2PMSG_64 & mask) == 0x80000000; returns the last value and
// whether the bound was met.
bool waitRingMbox(const RegBar &bar, uint32_t *last, uint32_t *waitedMs)
{
    const uint64_t off = mp0ByteOffset(kC2pmsg64Dword);
    uint32_t v = 0;
    for (uint32_t ms = 0; ms <= kRingReadyWaitMs; ++ms) {
        if (bar.rd(off, &v) && ringCreateAccepted(v)) {
            *last = v;
            *waitedMs = ms;
            return true;
        }
        IOSleep(1);
    }
    *last = v;
    *waitedMs = kRingReadyWaitMs;
    return false;
}

// Zero a VRAM region through the window, paced.
bool vramZero(const RegBar &bar, uint64_t vramByte, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; i += 4) {
        if (!vramWrite(bar, vramByte + i, 0))
            return false;
        if ((i & 0x3FFFu) == 0)
            IOSleep(1);
    }
    return true;
}

// Submit one command through the kernel ring and wait for its fence
// (psp_ring_cmd_submit + psp_cmd_submit_buf, as RedNavi verified on Navi 32).
bool ringSubmit(const RegBar &bar, uint64_t mcBase, uint32_t cmdId,
                const uint32_t *args, uint32_t nargs,
                uint32_t *resp, uint32_t nresp, uint32_t fenceIndex)
{
    if (!vramZero(bar, kCmdBufVramOffset, kCmdBufBytes) ||
        !vramZero(bar, kFenceBufVramOffset, kCmdBufBytes))
        return false;

    if (!vramWrite(bar, kCmdBufVramOffset + kCmdBufCmdIdOff, cmdId))
        return false;
    for (uint32_t i = 0; i < nargs; ++i) {
        if (!vramWrite(bar, kCmdBufVramOffset + kCmdBufArgsOff + i * 4, args[i]))
            return false;
    }

    uint32_t wptr = 0;
    if (!bar.rd(mp0ByteOffset(kC2pmsg67Dword), &wptr) || !wptrValid(wptr)) {
        N33RL("ringSubmit: write pointer read failed or odd value 0x%x, not sending", wptr);
        return false;
    }

    const uint64_t frame = frameVramOffset(wptr);
    const uint64_t cmdMc = mcBase + kCmdBufVramOffset;
    const uint64_t fenceMc = mcBase + kFenceBufVramOffset;
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

}  // namespace

void RunPspRingStage(IOPCIDevice *pci, IOService *publisher, uint32_t tmrMode)
{
    using cobalt64::navi33::psp::kPciCommandMemorySpace;
    using cobalt64::navi33::psp::kRegBarIndex;
    using cobalt64::navi33::psp::mmhubByteOffset;

    if (!pci || !publisher)
        return;
    if (tmrMode != kTmrModeLinuxLike && tmrMode != kTmrModeVisibleBar)
        tmrMode = kTmrModeLinuxLike;

    N33RL_STEP("PSP ring stage starting (TMR mode %u)", tmrMode);

    const uint16_t command = pci->configRead16(kIOPCIConfigCommand);
    publisher->setProperty("Navi33Bringup,PspRing,PciCommandRaw",
                           static_cast<uint64_t>(command), 16);
    if (!(command & kPciCommandMemorySpace)) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: PCI memory decode is off (config space is never written)");
        N33RL("aborted: PCI command 0x%04x has no memory-space bit", command);
        return;
    }

    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(
        static_cast<UInt8>(kIOPCIConfigBaseAddress0 + kRegBarIndex * 4));
    if (!map) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: register BAR5 map failed");
        return;
    }
    RegBar bar;
    bar.va = reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress());
    bar.len = static_cast<uint64_t>(map->getLength());
    const uint64_t maxNeed = mmhubByteOffset(kFbLocationTopDword) + 4;
    if (!bar.va || bar.len < maxNeed) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: register BAR5 window too small for the pinned register set");
        map->release();
        return;
    }

    // The ring belongs to the secure OS: it must be alive.
    uint32_t alive = 0;
    bar.rd(mp0ByteOffset(kC2pmsg81Dword), &alive);
    publisher->setProperty("Navi33Bringup,PspRing,SecureOsAlive", sosAlive(alive));
    if (!sosAlive(alive)) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: secure OS not alive (run the chain stage first: navi33psp_chain=1)");
        N33RL("aborted: C2PMSG_81 = 0x%08x", alive);
        map->release();
        return;
    }

    // Live VRAM topology (same reads as the load stage).
    uint32_t fbBase = 0, fbTop = 0;
    const bool fbOk = bar.rd(mmhubByteOffset(kFbLocationBaseDword), &fbBase) &&
                      bar.rd(mmhubByteOffset(kFbLocationTopDword), &fbTop) &&
                      fbBase != 0 && fbTop >= fbBase && fbBase < 0x1000000u &&
                      fbTop < 0x1000000u;
    if (!fbOk) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: implausible FB_LOCATION values");
        map->release();
        return;
    }
    const uint64_t mcBase = static_cast<uint64_t>(fbBase) << 24;
    const uint64_t vramBytes = static_cast<uint64_t>(fbTop - fbBase + 1) << 24;
    publisher->setProperty("Navi33Bringup,PspRing,VramMcBase", mcBase, 64);
    publisher->setProperty("Navi33Bringup,PspRing,VramBytes", vramBytes, 64);
    N33RL_STEP("MC base 0x%llx, VRAM %llu MiB", (unsigned long long)mcBase,
               (unsigned long long)(vramBytes >> 20));

    // 1. Ring create (psp_v13_0_ring_create).
    uint32_t st = 0, waited = 0;
    const bool readyBefore = waitRingMbox(bar, &st, &waited);
    publisher->setProperty("Navi33Bringup,PspRing,RingReadyBefore", readyBefore);
    publisher->setProperty("Navi33Bringup,PspRing,RingMboxBefore",
                           static_cast<uint64_t>(st), 32);
    N33RL_STEP("C2PMSG_64 before = 0x%08x (ready for ring creation: %u, %u ms)",
               st, readyBefore ? 1u : 0u, waited);
    if (!readyBefore) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: sOS not ready for ring creation");
        map->release();
        return;
    }

    if (!vramZero(bar, kRingVramOffset, kRingSizeBytes)) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: VRAM window failed while zeroing the ring");
        map->release();
        return;
    }
    const uint64_t ringMc = mcBase + kRingVramOffset;
    const bool wrote = bar.wr(mp0ByteOffset(kC2pmsg69Dword),
                              static_cast<uint32_t>(ringMc)) &&
                       bar.wr(mp0ByteOffset(kC2pmsg70Dword),
                              static_cast<uint32_t>(ringMc >> 32)) &&
                       bar.wr(mp0ByteOffset(kC2pmsg71Dword), kRingSizeBytes) &&
                       bar.wr(mp0ByteOffset(kC2pmsg64Dword), kRingTypeKm << 16);
    if (!wrote) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "aborted: ring mailbox write failed");
        map->release();
        return;
    }
    IOSleep(kRingPostCmdDelayMs);
    waited = 0;
    const bool created = waitRingMbox(bar, &st, &waited);
    publisher->setProperty("Navi33Bringup,PspRing,RingCreated", created);
    publisher->setProperty("Navi33Bringup,PspRing,RingMboxAfter",
                           static_cast<uint64_t>(st), 32);
    publisher->setProperty("Navi33Bringup,PspRing,RingMc", ringMc, 64);
    N33RL_STEP("C2PMSG_64 after = 0x%08x (ring created: %u, %u ms), ring at MC 0x%llx",
               st, created ? 1u : 0u, waited, (unsigned long long)ringMc);
    if (!created) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "ring-create failed — evidence captured, nothing further submitted");
        map->release();
        return;
    }

    // 2. LOAD_TOC: the TOC bin rides in the embedded SOS blob.
    uint32_t tocResp[5] = {};
    uint32_t fenceIndex = 1;
    bool tocFence = false;
    {
        cobalt64::navi33::psp::SosImage sos;
        const uint32_t sosLen = static_cast<uint32_t>(fw_psp_13_0_7_sos_size);
        const int tocBin = parseSosHeaderV2(fw_psp_13_0_7_sos, sosLen, sos)
                               ? findBin(sos, FwToc)
                               : -1;
        if (tocBin < 0) {
            publisher->setProperty("Navi33Bringup,PspRing,LoadTocSubmitted", false);
            publisher->setProperty("Navi33Bringup,PspRing,Stage",
                                   "ring created; LOAD_TOC skipped: no TOC bin in the embedded SOS image");
            N33RL("ring created; no TOC bin — skipping LOAD_TOC/SETUP_TMR");
            map->release();
            return;
        }
        const uint32_t tocOff = sos.arrayOffset + sos.bins[tocBin].offsetBytes;
        const uint32_t tocLen = sos.bins[tocBin].sizeBytes;
        if (tocLen > kCmdBufBytes) {
            publisher->setProperty("Navi33Bringup,PspRing,Stage",
                                   "aborted: TOC bin larger than the staging buffer");
            map->release();
            return;
        }
        N33RL_STEP("LOAD_TOC: %u bytes (version 0x%08x) to staging +0x%llx",
                   tocLen, sos.bins[tocBin].fwVersion,
                   (unsigned long long)kTocStagingVramOffset);
        if (!vramZero(bar, kTocStagingVramOffset, kCmdBufBytes)) {
            publisher->setProperty("Navi33Bringup,PspRing,Stage",
                                   "aborted: VRAM window failed while zeroing TOC staging");
            map->release();
            return;
        }
        const int32_t mismatch = vramCopyVerify(bar, kTocStagingVramOffset,
                                                fw_psp_13_0_7_sos + tocOff, tocLen);
        if (mismatch < 0) {
            publisher->setProperty("Navi33Bringup,PspRing,Stage",
                                   "aborted: VRAM window failed while staging the TOC");
            map->release();
            return;
        }
        N33RL("TOC staged: %d mismatching dwords", mismatch);

        const uint64_t tocMc = mcBase + kTocStagingVramOffset;
        const uint32_t args[3] = {static_cast<uint32_t>(tocMc),
                                  static_cast<uint32_t>(tocMc >> 32), tocLen};
        tocFence = ringSubmit(bar, mcBase, kCmdLoadToc, args, 3, tocResp, 5, fenceIndex);
        publisher->setProperty("Navi33Bringup,PspRing,LoadTocSubmitted", true);
        publisher->setProperty("Navi33Bringup,PspRing,LoadTocFenceOk", tocFence);
        publisher->setProperty("Navi33Bringup,PspRing,LoadTocStatus",
                               static_cast<uint64_t>(tocResp[0]), 32);
        publisher->setProperty("Navi33Bringup,PspRing,TmrSize",
                               static_cast<uint64_t>(tocResp[loadTocTmrSizeIndex]), 32);
        N33RL_STEP("LOAD_TOC: fence %s, status 0x%08x, tmr_size 0x%08x",
                   tocFence ? "ok" : "missing", tocResp[0],
                   tocResp[loadTocTmrSizeIndex]);
    }

    // 3. SETUP_TMR (psp_tmr_load): needs the TMR size from LOAD_TOC and the
    //    BAR0 physical address for the virt_phy_addr form.
    const uint64_t tmrSize = tocResp[loadTocTmrSizeIndex];
    if (!tocFence || tocResp[0] != 0 || tmrSize == 0) {
        publisher->setProperty("Navi33Bringup,PspRing,SetupTmrSubmitted", false);
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "ring + LOAD_TOC done; SETUP_TMR skipped (no usable tmr_size)");
        map->release();
        return;
    }

    uint64_t tmrOff = 0;
    if (!tmrOffsetForMode(vramBytes, tmrSize, tmrMode, &tmrOff)) {
        publisher->setProperty("Navi33Bringup,PspRing,SetupTmrSubmitted", false);
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "ring + LOAD_TOC done; SETUP_TMR skipped (implausible TMR placement)");
        map->release();
        return;
    }

    IOMemoryDescriptor *bar0 = pci->getDeviceMemoryWithRegister(
        static_cast<UInt8>(kIOPCIConfigBaseAddress0 + 0 * 4));
    if (!bar0) {
        publisher->setProperty("Navi33Bringup,PspRing,SetupTmrSubmitted", false);
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "ring + LOAD_TOC done; SETUP_TMR skipped (no BAR0)");
        map->release();
        return;
    }
    const uint64_t tmrMc = mcBase + tmrOff;
    const uint64_t tmrPa = static_cast<uint64_t>(bar0->getPhysicalAddress()) + tmrOff;
    publisher->setProperty("Navi33Bringup,PspRing,TmrMode",
                           static_cast<uint64_t>(tmrMode), 8);
    publisher->setProperty("Navi33Bringup,PspRing,TmrOffset", tmrOff, 64);
    publisher->setProperty("Navi33Bringup,PspRing,TmrMc", tmrMc, 64);
    publisher->setProperty("Navi33Bringup,PspRing,TmrBar0Physical", tmrPa, 64);
    N33RL_STEP("SETUP_TMR: 0x%llx bytes at MC 0x%llx (PA 0x%llx), mode %u",
               (unsigned long long)tmrSize, (unsigned long long)tmrMc,
               (unsigned long long)tmrPa, tmrMode);

    uint32_t tmrResp[1] = {};
    const uint32_t args[6] = {static_cast<uint32_t>(tmrMc),
                              static_cast<uint32_t>(tmrMc >> 32),
                              static_cast<uint32_t>(tmrSize),
                              kSetupTmrFlagsPhyAddr,
                              static_cast<uint32_t>(tmrPa),
                              static_cast<uint32_t>(tmrPa >> 32)};
    const bool tmrFence = ringSubmit(bar, mcBase, kCmdSetupTmr, args, 6, tmrResp, 1, ++fenceIndex);
    publisher->setProperty("Navi33Bringup,PspRing,SetupTmrSubmitted", true);
    publisher->setProperty("Navi33Bringup,PspRing,SetupTmrFenceOk", tmrFence);
    publisher->setProperty("Navi33Bringup,PspRing,SetupTmrStatus",
                           static_cast<uint64_t>(tmrResp[0]), 32);
    N33RL_STEP("SETUP_TMR: fence %s, status 0x%08x", tmrFence ? "ok" : "missing",
               tmrResp[0]);

    if (tmrFence && tmrResp[0] == 0) {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "ring-complete: ring created, LOAD_TOC answered, SETUP_TMR accepted");
        N33RL("ring stage COMPLETE: TMR of 0x%llx bytes accepted at MC 0x%llx",
              (unsigned long long)tmrSize, (unsigned long long)tmrMc);
    } else {
        publisher->setProperty("Navi33Bringup,PspRing,Stage",
                               "ring-created-and-toc-done; SETUP_TMR not verified — evidence captured");
    }

    map->release();
}
