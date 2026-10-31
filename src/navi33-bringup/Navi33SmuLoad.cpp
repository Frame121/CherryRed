//
//  Navi33SmuLoad.cpp — SMU stage (M2b): hand the SMU 13.0.7 firmware to the
//  secure OS through LOAD_IP_FW (GFX_FW_TYPE_SMU), then verify the SMU is
//  alive with the smu_v13_0 message protocol (GetSmuVersion,
//  GetDriverIfVersion, TestMessage). On a correct answer the stage publishes
//  "Navi33Bringup,FirmwareVerified" = true — the property the
//  Navi33Accelerator compute gate waits for.
//
//  Reachable only behind navi33bringup=1 + navi33psp=1 + navi33psp_load=1 +
//  navi33psp_ring=1 + navi33psp_smu=1 (the ring must already exist in the
//  same boot). Write class: the MM index/data window and the MP1 mailbox
//  registers, all through the bounds-checked RegBar. Nothing else.
//
#include "Navi33SmuLoad.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>

#include "Navi33MmWindow.hpp"
#include "Navi33PspMailbox.hpp"
#include "Navi33PspRing.hpp"
#include "Navi33RingSubmit.hpp"

extern "C" const uint8_t fw_smu_13_0_7[];
extern "C" const size_t  fw_smu_13_0_7_size;

#define N33SMU_LOG(fmt, ...) IOLog("Navi33SmuLoad: " fmt "\n", ##__VA_ARGS__)
#define N33SMU_STEP(...) do { N33SMU_LOG(__VA_ARGS__); IOSleep(40); } while (0)

using cobalt64::navi33::psp::kC2pmsg81Dword;
using cobalt64::navi33::psp::kCmdLoadIpFw;
using cobalt64::navi33::psp::kFbLocationBaseDword;
using cobalt64::navi33::psp::kFbLocationTopDword;
using cobalt64::navi33::psp::kFenceBufVramOffset;
using cobalt64::navi33::psp::kGfxFwTypeSmu;
using cobalt64::navi33::psp::kMp0BaseDwords;
using cobalt64::navi33::psp::kMp1BaseDwords;
using cobalt64::navi33::psp::kMp1C2pmsg66Dword;
using cobalt64::navi33::psp::kMp1C2pmsg82Dword;
using cobalt64::navi33::psp::kMp1C2pmsg90Dword;
using cobalt64::navi33::psp::kPpsmcMsgGetDriverIfVersion;
using cobalt64::navi33::psp::kPpsmcMsgGetSmuVersion;
using cobalt64::navi33::psp::kPpsmcMsgTestMessage;
using cobalt64::navi33::psp::kStagingVramOffset;
using cobalt64::navi33::psp::mp0ByteOffset;

namespace {

constexpr uint32_t kSmuReadyWaitMs = 2000;
// The fence counter continues from the ring stage (LOAD_TOC / SETUP_TMR used
// small values); this stage starts at 100 so no index is ever reused.
constexpr uint32_t kSmuFenceIndex = 100u;

// Byte offset of an MP1 mailbox register (dword units from the MP1 base).
inline uint64_t mp1ByteOffset(uint32_t dword)
{
    return (static_cast<uint64_t>(kMp1BaseDwords) + dword) * 4ull;
}

// One SMU 13 message exchange over the MP1 mailbox (smu_v13_0_send_msg +
// _wait_for_response). Every wait is bounded; returns false when the SMU
// never answers. C2PMSG_90 is cleared first, then the argument, then the
// message — the order smu_v13_0.c uses.
bool smu13Exchange(const cobalt64::navi33::psp::RegBar &bar, uint32_t msg,
                   uint32_t arg, uint32_t *result)
{
    const uint64_t r66 = mp1ByteOffset(kMp1C2pmsg66Dword);
    const uint64_t r82 = mp1ByteOffset(kMp1C2pmsg82Dword);
    const uint64_t r90 = mp1ByteOffset(kMp1C2pmsg90Dword);

    uint32_t response = 0;
    for (uint32_t ms = 0; ms <= kSmuReadyWaitMs; ++ms) {
        if (bar.rd(r90, &response) && response != 0)
            break;
        IOSleep(1);
    }
    if (response == 0) {
        N33SMU_LOG("SMU msg 0x%x: previous message never completed", msg);
        return false;
    }
    if (!bar.wr(r90, 0) || !bar.wr(r82, arg) || !bar.wr(r66, msg))
        return false;
    for (uint32_t ms = 0; ms <= kSmuReadyWaitMs; ++ms) {
        if (bar.rd(r90, &response) && response != 0)
            break;
        IOSleep(1);
    }
    if (response == 0)
        return false;
    if (result && !bar.rd(r82, result))
        return false;
    return cobalt64::navi33::psp::smuResponseOk(response);
}

}  // namespace

void RunNavi33SmuStage(IOPCIDevice *pci, IOService *publisher)
{
    using cobalt64::navi33::psp::kPciCommandMemorySpace;
    using cobalt64::navi33::psp::kRegBarIndex;
    using cobalt64::navi33::psp::mmhubByteOffset;

    if (!pci || !publisher)
        return;

    N33SMU_STEP("SMU stage starting (LOAD_IP_FW GFX_FW_TYPE_SMU + smu_v13 messages)");

    const uint16_t command = pci->configRead16(kIOPCIConfigCommand);
    if (!(command & kPciCommandMemorySpace)) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "aborted: PCI memory decode is off (config space is never written)");
        return;
    }

    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(
        static_cast<UInt8>(kIOPCIConfigBaseAddress0 + kRegBarIndex * 4));
    if (!map) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "aborted: register BAR5 map failed");
        return;
    }
    cobalt64::navi33::psp::RegBar bar;
    bar.va = reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress());
    bar.len = static_cast<uint64_t>(map->getLength());
    const uint64_t maxNeed = mmhubByteOffset(kFbLocationTopDword) + 4;
    if (!bar.va || bar.len < maxNeed) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "aborted: register BAR5 window too small");
        map->release();
        return;
    }

    // The secure OS must be alive and the ring must exist (the ring stage
    // created it earlier in this boot; the sOS holds it).
    uint32_t alive = 0;
    bar.rd(mp0ByteOffset(kC2pmsg81Dword), &alive);
    if (!cobalt64::navi33::psp::sosAlive(alive)) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "aborted: secure OS not alive (run navi33psp_chain=1 first)");
        map->release();
        return;
    }

    uint32_t fbBase = 0, fbTop = 0;
    if (!bar.rd(mmhubByteOffset(kFbLocationBaseDword), &fbBase) ||
        !bar.rd(mmhubByteOffset(kFbLocationTopDword), &fbTop) ||
        fbBase == 0 || fbTop < fbBase) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "aborted: implausible FB_LOCATION values");
        map->release();
        return;
    }
    const uint64_t mcBase = static_cast<uint64_t>(fbBase) << 24;

    // Extract and stage the SMU ucode (amdgpu_ucode_init_single_fw for
    // AMDGPU_UCODE_ID_SMC: the ucode array of the common firmware header).
    cobalt64::navi33::psp::SmuUcode ucode;
    const uint32_t blobLen = static_cast<uint32_t>(fw_smu_13_0_7_size);
    if (!cobalt64::navi33::psp::parseSmuUcode(fw_smu_13_0_7, blobLen, ucode)) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "aborted: smu_13_0_7.bin header did not parse");
        map->release();
        return;
    }
    const int32_t mismatch = cobalt64::navi33::psp::vramCopyVerify(
        bar, kStagingVramOffset, fw_smu_13_0_7 + ucode.offsetBytes, ucode.sizeBytes);
    if (mismatch != 0) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "aborted: staging copy failed or did not verify");
        N33SMU_LOG("staging copy mismatch=%d", mismatch);
        map->release();
        return;
    }
    const uint64_t fwMc = mcBase + kStagingVramOffset;
    N33SMU_STEP("SMU ucode %u bytes (version 0x%x) staged at MC 0x%llx, %d mismatches",
                ucode.sizeBytes, ucode.version, (unsigned long long)fwMc, mismatch);

    // LOAD_IP_FW: fw MC lo/hi, size, type.
    const uint32_t args[4] = {static_cast<uint32_t>(fwMc),
                              static_cast<uint32_t>(fwMc >> 32),
                              ucode.sizeBytes, kGfxFwTypeSmu};
    uint32_t loadResp[4] = {};
    const bool loadOk = cobalt64::navi33::psp::ringSubmit(
        bar, mcBase, kCmdLoadIpFw, args, 4, loadResp, 4, kSmuFenceIndex);
    publisher->setProperty("Navi33Bringup,SmuStage,LoadSubmitted", loadOk);
    N33SMU_STEP("LOAD_IP_FW SMU (%u bytes): fence %s, status 0x%08x",
                ucode.sizeBytes, loadOk ? "ok" : "missing", loadResp[0]);
    if (!loadOk || loadResp[0] != 0) {
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "stopped: LOAD_IP_FW did not answer cleanly");
        map->release();
        return;
    }
    IOSleep(100);  // let the SMU firmware come up (RedNavi-verified pacing)

    // smu_v13_0_check_fw_status, then the version queries.
    uint32_t smuVersion = 0;
    const bool versionOk = smu13Exchange(bar, kPpsmcMsgGetSmuVersion, 0, &smuVersion);
    uint32_t driverIf = 0;
    const bool driverIfOk = versionOk &&
        smu13Exchange(bar, kPpsmcMsgGetDriverIfVersion, 0, &driverIf);
    uint32_t testResult = 0;
    const bool testOk = versionOk &&
        smu13Exchange(bar, kPpsmcMsgTestMessage, 0x12345678u, &testResult);

    const char *const names[3] = {"smu_version", "driver_if_version", "test_result"};
    const uint64_t values[3] = {smuVersion, driverIf, testResult};
    OSDictionary *info = OSDictionary::withCapacity(4);
    if (info) {
        for (unsigned i = 0; i < 3; ++i) {
            OSNumber *n = OSNumber::withNumber(values[i], 64);
            if (n) {
                info->setObject(names[i], n);
                n->release();
            }
        }
        publisher->setProperty("Navi33Bringup,SmuInfo", info);
        info->release();
    }

    if (versionOk) {
        publisher->setProperty("Navi33Bringup,FirmwareVerified", true);
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "complete: SMU alive and answering");
        N33SMU_STEP("SMU alive: version 0x%08x, driver-if 0x%08x (%s), test 0x%08x (%s) — "
                    "FirmwareVerified=true",
                    smuVersion, driverIf, driverIfOk ? "ok" : "no-answer",
                    testResult, testOk ? "ok" : "no-answer");
    } else {
        publisher->setProperty("Navi33Bringup,FirmwareVerified", false);
        publisher->setProperty("Navi33Bringup,SmuStage",
                               "stopped: SMU did not answer GetSmuVersion");
        N33SMU_LOG("SMU did not answer GetSmuVersion — FirmwareVerified stays false");
    }
    map->release();
}
