// RedNavi probe (milestone M1): read-only. Matches the RX 7800 XT (Navi 32, 1002:747E),
// maps the register BAR, and logs what the GPU tells us about itself: PCI config, BAR sizes
// and the IP discovery table (the authoritative list of IP block versions).
//
// Second stage: reads the PSP (MP0) and SMU (MP1) mailbox registers, using the base addresses
// found in that table, to show what the VBIOS left running. Third stage: reads the MMHUB
// framebuffer location registers, which give the VRAM address as the GPU itself sees it (needed
// to hand buffers to the PSP bootloader). Results are logged and published as the
// "ip-discovery", "mailbox-regs" and "mmhub-regs" properties (ioreg -rn RedNaviProbe -l -x).
//
// Fourth stage (opt-in, writes to the GPU): with the boot-arg rednavi_psp_load=1 and a build that
// has RedNavi/psp_fw.h (see tools/gen_psp_fw_header.py), copies each PSP component (KDB, system
// driver, ..., secure OS) into VRAM and hands it to the PSP bootloader, in the order and with the
// handshake of psp_hw_start / psp_v13_0_bootloader_load_component in amdgpu. Stops at the first error.
// Fifth stage (opt-in, rednavi_psp_ring=1): asks the running secure OS to create the kernel-mode
// ring (psp_v13_0_ring_create). The sOS answering is the first proof that it accepts commands.
// Sixth stage (opt-in, rednavi_psp_cmd=1): sends one harmless command through that ring, the
// boot-config query (GFX_CMD_ID_BOOT_CFG, BOOTCFG_CMD_GET), and logs the response.
// Seventh stage (opt-in, rednavi_psp_toc=1): sends GFX_CMD_ID_LOAD_TOC with the TOC component of the
// PSP firmware file; the response carries the TMR size. amdgpu does not send SETUP_TMR on this PSP
// (boot_time_tmr and autoload are both set for MP0 13.0.10, see docs/PSP_BRINGUP.md).
//
// Ninth stage (read-only, always on): reads the DMCUB control, boot-status and mailbox registers
// (DCN 3.2, register offsets from dcn_3_2_0_offset.h, all at DMU base index 2) to see whether the
// VBIOS left a display microcontroller running. Published as "dmcub-regs".
//
// Seventh-and-a-half stage (opt-in, rednavi_psp_tmr=1): GFX_CMD_ID_SETUP_TMR with the size LOAD_TOC
// returned. For MP0 13.0.10 amdgpu sets boot_time_tmr = false, so it does send this (the Linux
// reference dmesg on this card shows "reserve 0xa700000 from 0x83e0000000 for PSP TMR").
//
// Eleventh stage (read-only, always on): reads the DCN hot-plug-detect registers (HPDn_DC_HPD_INT_STATUS,
// bit 1 = DC_HPD_SENSE, bit 4 = SENSE_DELAYED, and the raw pin levels in DC_GPIO_HPD_Y) to see which
// connectors have a monitor. Published as "hpd-regs".
//
// Twelfth stage (opt-in, rednavi_edid=1, writes the DCN AUX engines): runs a native DPCD read (16 bytes at
// 0x000) on each of the five AUX engines, as dce_aux.c does, to find the one wired to the monitor; on the
// first engine that ACKs it then reads the EDID with I2C-over-AUX at address 0x50. Published as
// "aux-probe" (per-engine status), "dpcd" and "edid" (tools/decode_edid.py).
//
// Thirteenth stage (opt-in, rednavi_dp_train=1, writes DCN and the sink): enables the DisplayPort transmitter
// of path 0 through the VBIOS command table (via DMUB) and runs the DP link training (clock recovery with
// TPS1, channel equalisation with TPS3 or TPS2) against the monitor over AUX4, the way link_dp_training.c does.
// The result is the sink's own verdict: DPCD 0x202-0x204 (CR_DONE, CHANNEL_EQ_DONE, SYMBOL_LOCKED, align).
// Published as "dp-train".
//
// Fifteenth stage (opt-in, rednavi_dcn_replay=N, writes the DCN pipe): replays the register state of a lit
// pipe captured on Linux (RedNavi/dcn_replay.h, tools/gen_dcn_replay.py) in nine groups: pixel clock DTO,
// OPTC clocks, OTG timing, OPP/FMT, MPC output, DIG/DP stream setup, OTG enable, test pattern, DP stream
// enable. N limits how many groups are applied (1 to 9), so the pipe can be brought up step by step. Needs
// the DP link trained first (rednavi_dp_train=1). Every group is logged and read back.
//
// Groups 10 to 14 of the replay add a plane: the probe writes a 1920x1080 XRGB8888 test image into VRAM
// (kRNScanoutVramOff, linear, pitch 7680 as the Linux text console uses), then replays the DCHUBBUB
// watermarks, DPP, MPC tree and HUBP configuration captured on Linux for that linear surface, points the
// primary surface address at the image and finally switches the pattern generator off.
//
// Fourteenth stage (UNSAFE, rednavi_dcn_dump_unsafe=1): reading every DCN 3.2 register of the list from macOS
// HANGS THE MACHINE (found on 2026-10-28; the console freezes with stale lines on screen). Kept only as a
// record; use tools/linux_dcn_dump.py, which reads the same list safely on Linux. Reads every DCN 3.2 register of tools/dcn32_reglist.txt
// (4366 registers) after everything else has run and publishes the values, in list order, as the "dcn-dump"
// property. tools/linux_dcn_dump.py produces the same list on Linux, so the two states can be diffed.
//
// Tenth stage (opt-in, writes to the GPU, rednavi_dmub_load=1 and a build with RedNavi/dmub_fw.h):
// stops the DMCUB the VBIOS left running, builds the DMUB memory block in VRAM, backdoor-loads the
// DAL firmware and releases the reset, mirroring dmub_srv_hw_init and the dmub_dcn32_* hooks in amdgpu.
// rednavi_dmub_cmd=1 then sends DMUB_CMD__QUERY_FEATURE_CAPS through inbox1 and reads the reply from
// the command slot (the mailbox ring is a 8 KiB array of 64 byte commands in the DMUB block).
// rednavi_dmub_psp=1 first hands the signed image to the PSP (GFX_CMD_ID_LOAD_IP_FW, type DMUB) and
// skips the backdoor load, which is how amdgpu loads it when load_type is PSP.
// rednavi_dmub_mode=1 programs VRAM offsets instead of MC addresses for the data windows.
//
// Eighth stage (read-only, always on): reads the VBIOS image from the PCI expansion ROM, which the
// DMUB framebuffer window needs (amdgpu_dm_dmub.c copies adev->bios into DMUB_WINDOW_3_VBIOS).
// Published as the "vbios" property (decode with tools/check_vbios.py). Enabling the ROM decode
// bit in config space is the only write.
//
// Without that boot-arg nothing is written to the GPU except the MM_INDEX/MM_DATA window used to
// read VRAM.

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <pexpert/pexpert.h>
#include "dcn_reglist.h"
#include "dcn_replay.h"
#include "edid.h"
#include "mode_regs.h"
#include "smu13.h"

#ifdef RN_HAVE_PSP_FW
#include "psp_fw.h"
#endif
#ifdef RN_HAVE_DMUB_FW
#include "dmub_fw.h"
#endif

// Log a step and give the console time to print it, so a hang shows the last step reached on screen.
#define RN_STEP(...) do { IOLog(__VA_ARGS__); IOSleep(40); } while (0)

#define kRNRegBar          5          // MMIO registers (512 KB on Navi 3x)
#define kRNVramBar         0          // VRAM aperture (size depends on Resizable BAR)

// SOC15 MMIO window: byte offsets inside the register BAR (dword 0x0, 0x1, 0x6 in amdgpu).
#define kRNMmIndex         0x00
#define kRNMmData          0x04
#define kRNMmIndexHi       0x18

// Register byte offsets (dword index * 4) from amdgpu_discovery.c, "consistent across all the SOCs".
#define kRNRccConfigMemsize   (0xde3 * 4)    // VRAM size in MB (mmRCC_CONFIG_MEMSIZE)
#define kRNMp0C2pmsg33        (0x16061 * 4)  // bit 31 set once IFWI init is done (mmMP0_SMN_C2PMSG_33)

// IP discovery sits DISCOVERY_TMR_OFFSET below the end of VRAM and is DISCOVERY_TMR_SIZE long
// (amdgpu_discovery.h). BINARY_SIGNATURE is in include/discovery.h.
#define kRNDiscoveryTmrOffset (64 * 1024)
#define kRNDiscoveryTmrSize   (10 * 1024)
#define kRNBinarySignature    0x28211407u
#define kRNFallbackVramMB     (16u * 1024)  // RX 7800 XT, used if the memsize register reads 0 or all ones

// Hardware IDs in the discovery table (soc15_hw_ip.h).
#define kRNMp0C2pmsg35        0x063       // dword offsets from the MP0 base
#define kRNMp0C2pmsg36        0x064
#define kRNMp0C2pmsg58        0x07a
#define kRNPspBlLoadSos       0x20000u    // PSP_BL__LOAD_SOSDRV
#define kRNMp0C2pmsg81        0x091
#define kRNPspBlReady         0x80000000u
#define kRNVramMcBase         0x8000000000ull  // MMHUB FB_LOCATION_BASE 0x8000 << 24, read by probe stage 3
#define kRNPspBufVramOffset   0x40000000ull    // 1 GB into VRAM, 1 MB aligned, well clear of the table at the end
#define kRNPspRingVramOffset  0x40200000ull    // 4 KB ring, 2 MB into the staging area
#define kRNPspCmdVramOffset   0x40210000ull    // 4 KB command/response buffer, 4 KB aligned
#define kRNPspFenceVramOffset 0x40220000ull    // 4 KB fence buffer
#define kRNMp0C2pmsg67        0x083
#define kRNPspCmdSetupTmr     0x05u            // GFX_CMD_ID_SETUP_TMR
#define kRNPspTmrVramOffset   0x02000000ull    // 32 MB into VRAM: inside the 256 MB CPU-visible BAR0
#define kRNPspCmdLoadToc      0x20u            // GFX_CMD_ID_LOAD_TOC
#define kRNPspCmdBootCfg      0x22u            // GFX_CMD_ID_BOOT_CFG
#define kRNPspBootCfgGet      2u               // BOOTCFG_CMD_GET
#define kRNPspRingSize        0x1000u
#define kRNPspRingTypeKm      2u               // PSP_RING_TYPE__KM
#define kRNMp0C2pmsg64        0x080
#define kRNMp0C2pmsg69        0x085
#define kRNMp0C2pmsg70        0x086
#define kRNMp0C2pmsg71        0x087
#define kRNPspMboxMask        0x8000FFFFu      // response flag plus status bits
#define kRNScanoutVramOff     0x08000000ull    // 128 MB into VRAM, inside the 256 MB macOS maps through BAR0: MC address 0x8008000000, the value in dcn_replay.h
#define kRNPspBufSize         (1u << 20)
#define kRNHwIdMp1            1
#define kRNHwIdMmhub          34
#define kRNHwIdDmu            271
#define kRNDmubBlockVramOff   0x50000000ull    // 1.25 GB into VRAM
#define kRNPspCmdLoadIpFw     0x06u            // GFX_CMD_ID_LOAD_IP_FW
#define kRNPspFwTypeDmub      51u              // GFX_FW_TYPE_DMUB
#define kRNDmubGpintStopFw    0x10020000u      // status 1, command DMUB_GPINT__STOP_FW (2)
#define kRNDmubStopResponse   0xDEADDEADu
#define kRNDmubCw0Base        0x60000000u
#define kRNDmubCw1Base        0x61000000u
#define kRNDmubCw3Base        0x63000000u
#define kRNDmubCw4Base        0x64000000u
#define kRNDmubCw5Base        0x65000000u
#define kRNDmubCw6Base        0x66000000u
#define kRNDmubRegion5Base    0xA0000000u
#define kRNDmubRbSize         8192u
#define kRNDmubTraceEntryOff  16u
#define kRNHwIdMp0            255

// Mailbox registers: dword offsets from the IP base address, taken from mp_13_0_0_offset.h
// (PSP, used by psp_v13_0.c) and smu_v13_0.c (SMU message, argument and response).
struct RNMailboxReg {
    const char *name;
    uint16_t hwId;
    uint32_t dword;
    uint32_t baseIdx;   // which of the block's base addresses the offset is relative to
};
static const RNMailboxReg kRNMailboxRegs[] = {
    {"MP0_C2PMSG_33", kRNHwIdMp0, 0x061},  // bit 31: IFWI init done
    {"MP0_C2PMSG_35", kRNHwIdMp0, 0x063},  // bootloader command / status, bit 31: ready
    {"MP0_C2PMSG_36", kRNHwIdMp0, 0x064},  // bootloader address
    {"MP0_C2PMSG_64", kRNHwIdMp0, 0x080},  // ring command / response
    {"MP0_C2PMSG_67", kRNHwIdMp0, 0x083},  // ring write pointer
    {"MP0_C2PMSG_71", kRNHwIdMp0, 0x087},  // ring size
    {"MP0_C2PMSG_81", kRNHwIdMp0, 0x091},  // sOS alive when not zero
    {"MP1_C2PMSG_66", kRNHwIdMp1, 0x282},  // SMU message
    {"MP1_C2PMSG_82", kRNHwIdMp1, 0x292},  // SMU argument
    {"MP1_C2PMSG_90", kRNHwIdMp1, 0x29a},  // SMU response, 1 means OK
};

// MMHUB 3.0 registers (mmhub_3_0_0_offset.h). FB_BASE, FB_TOP and FB_OFFSET are 24-bit values in
// units of 16 MB (mmhub_v3_0_get_fb_location and _get_mc_fb_offset shift them left by 24).
static const RNMailboxReg kRNMmhubRegs[] = {
    {"MMHUB_FB_LOCATION_BASE", kRNHwIdMmhub, 0x8ec},
    {"MMHUB_FB_LOCATION_TOP",  kRNHwIdMmhub, 0x8ed},
    {"MMHUB_FB_OFFSET",        kRNHwIdMmhub, 0x8d7},
    {"MMHUB_AGP_BASE",         kRNHwIdMmhub, 0x8f0},
    {"MMHUB_AGP_BOT",          kRNHwIdMmhub, 0x8ef},
    {"MMHUB_AGP_TOP",          kRNHwIdMmhub, 0x8ee},
};

// DMCUB registers (dcn_3_2_0_offset.h), relative to base index 2 of the DMU block. DMCUB_SCRATCH0 is
// the firmware boot status (dmub_fw_boot_status in dmub_cmd.h); DMCUB_CNTL.DMCUB_ENABLE is bit 0 in
// the amdgpu field definitions and DMCUB_SEC_CNTL.DMCUB_SEC_RESET is the reset bit.
static const RNMailboxReg kRNDmcubRegs[] = {
    {"DMCUB_CNTL",         kRNHwIdDmu, 0x1f6, 2},
    {"DMCUB_CNTL2",        kRNHwIdDmu, 0x200, 2},
    {"DMCUB_SEC_CNTL",     kRNHwIdDmu, 0x1ce, 2},
    {"DMCUB_SCRATCH0",     kRNHwIdDmu, 0x1e3, 2},
    {"DMCUB_SCRATCH1",     kRNHwIdDmu, 0x1e4, 2},
    {"DMCUB_SCRATCH2",     kRNHwIdDmu, 0x1e5, 2},
    {"DMCUB_INBOX1_WPTR",  kRNHwIdDmu, 0x1d6, 2},
    {"DMCUB_INBOX1_RPTR",  kRNHwIdDmu, 0x1d7, 2},
    {"DMCUB_OUTBOX1_WPTR", kRNHwIdDmu, 0x1de, 2},
    {"DMCUB_GPINT_DATAOUT", kRNHwIdDmu, 0x1f9, 2},
    {"DMCUB_REGION3_CW0_OFFSET", kRNHwIdDmu, 0x1b5, 2},
};

static const RNMailboxReg kRNHpdRegs[] = {
    {"HPD0_INT_STATUS", kRNHwIdDmu, 0x1f14, 2}, {"HPD0_CONTROL", kRNHwIdDmu, 0x1f16, 2},
    {"HPD1_INT_STATUS", kRNHwIdDmu, 0x1f1c, 2}, {"HPD1_CONTROL", kRNHwIdDmu, 0x1f1e, 2},
    {"HPD2_INT_STATUS", kRNHwIdDmu, 0x1f24, 2}, {"HPD2_CONTROL", kRNHwIdDmu, 0x1f26, 2},
    {"HPD3_INT_STATUS", kRNHwIdDmu, 0x1f2c, 2}, {"HPD3_CONTROL", kRNHwIdDmu, 0x1f2e, 2},
    {"HPD4_INT_STATUS", kRNHwIdDmu, 0x1f34, 2}, {"HPD4_CONTROL", kRNHwIdDmu, 0x1f36, 2},
    {"DC_GPIO_HPD_MASK", kRNHwIdDmu, 0x28f4, 2}, {"DC_GPIO_HPD_A", kRNHwIdDmu, 0x28f5, 2},
    {"DC_GPIO_HPD_EN", kRNHwIdDmu, 0x28f6, 2}, {"DC_GPIO_HPD_Y", kRNHwIdDmu, 0x28f7, 2},
};

static void rnPublish(IOService *svc, const char *property, const char *const *names, const uint32_t *vals, unsigned n) {
    OSDictionary *out = OSDictionary::withCapacity(n);
    if (!out) return;
    for (unsigned i = 0; i < n; i++) {
        OSNumber *num = OSNumber::withNumber(static_cast<uint64_t>(vals[i]), 64);
        if (num) { out->setObject(names[i], num); num->release(); }
    }
    svc->setProperty(property, out);
    out->release();
}

static void rnPublishOne(OSDictionary *d, const char *key, uint64_t v) {
    OSNumber *n = OSNumber::withNumber(v, 64);
    if (n) { d->setObject(key, n); n->release(); }
}

class RedNaviProbe : public IOService {
    OSDeclareDefaultStructors(RedNaviProbe)

    IOPCIDevice *pci {nullptr};
    IOMemoryMap *regs {nullptr};
    OSData *disc {nullptr};   // copy of the IP discovery table
    OSData *vbiosCopy {nullptr};   // VBIOS image read from the PCI ROM
    bool dmubUp {false};           // our DMCUB is running and its mailbox is set up
    uint32_t dmubDmu {0};          // DMU base index 2
    uint64_t dmubMailbox {0};      // VRAM offset of the DMUB mailbox window (inbox1 ring)

    uint32_t rd(uint32_t off) { return *reinterpret_cast<volatile uint32_t *>(regs->getVirtualAddress() + off); }
    void wr(uint32_t off, uint32_t v) { *reinterpret_cast<volatile uint32_t *>(regs->getVirtualAddress() + off) = v; }

    // Read one dword of VRAM through the index/data pair (works regardless of BAR0 size).
    uint32_t vramRead(uint64_t pos) {
        wr(kRNMmIndex, static_cast<uint32_t>(pos) | 0x80000000u);
        wr(kRNMmIndexHi, static_cast<uint32_t>(pos >> 31));
        uint32_t v = rd(kRNMmData);
        return v;
    }

    void vramWrite(uint64_t pos, uint32_t v) {
        wr(kRNMmIndex, static_cast<uint32_t>(pos) | 0x80000000u);
        wr(kRNMmIndexHi, static_cast<uint32_t>(pos >> 31));
        wr(kRNMmData, v);
    }

    void dumpPci();
    void dumpDiscovery();
    void dumpVbios();
    void loadDmcub();
    void probeAux();
    bool dmubSend(const uint8_t *cmd, uint32_t len, uint32_t reply[16]);
    void dpInit();
    void dpTrain();
    void dcnDump();
    void dcnReplay();
    void fillScanout();
    void publishFramebuffer();
    void smuProbe(const char *property, bool send);
    void publishModes(const uint8_t *edid, size_t len);
    rn_mode selMode {};            // the mode the pipe is programmed for (boot-arg rednavi_mode=WxH@R, else the EDID's preferred, else 1080p60)
    uint32_t selRate {0x06};       // DisplayPort link rate code for that mode
    uint32_t selPitch {1920};      // scanout pitch in pixels
    void selectMode();
    rn_edid_info edidInfo {};      // parsed EDID of the connected monitor, valid when edidInfo.mode_count > 0
    bool dpTx(uint8_t action, uint8_t modeOrLane, uint8_t lanes, uint32_t symclk10k);
    bool dpcdWrite(uint32_t eng, uint32_t addr, const uint8_t *d, uint32_t n);
    bool dpcdRead(uint32_t eng, uint32_t addr, uint8_t *d, uint32_t n);
    bool auxTransfer(uint32_t eng, uint32_t action, uint32_t addr, uint8_t *data, uint32_t len, uint32_t *status, uint32_t *reply);
    void dmcubSnapshot(const char *property);
    bool ipBase(uint16_t hwId, uint32_t *base, uint32_t idx = 0);
    bool loadComponent(uint32_t mp0, const char *name, uint32_t cmd, const uint8_t *data, uint32_t len, uint32_t version);
    void loadPspChain();
    bool createPspRing();
    bool pspSubmit(uint32_t cmdId, const uint32_t *args, uint32_t nargs, uint32_t *resp, uint32_t nresp);
    void sendPspBootCfgGet();
    void sendPspLoadToc();
    void sendPspSetupTmr();
    uint32_t pspTmrSize {0};
    uint32_t pspFence {0};
    void dumpRegs(const RNMailboxReg *list, size_t count, const char *property);

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
};

OSDefineMetaClassAndStructors(RedNaviProbe, IOService)

void RedNaviProbe::dumpPci() {
    uint32_t id = pci->configRead32(0x00);
    uint32_t sub = pci->configRead32(0x2C);
    uint8_t rev = pci->configRead8(0x08);
    IOLog("RedNavi: PCI %04x:%04x subsys %04x:%04x rev %02x\n", id & 0xFFFF, id >> 16, sub & 0xFFFF, sub >> 16, rev);

    for (uint8_t i = 0; i < 6; i++) {
        IODeviceMemory *m = pci->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0 + i * 4);
        if (m) IOLog("RedNavi: BAR%u phys 0x%llx size 0x%llx\n", i, (uint64_t)m->getPhysicalAddress(), (uint64_t)m->getLength());
    }
}

void RedNaviProbe::dumpDiscovery() {
    // amdgpu waits up to 2 s for IFWI init before reading the table.
    uint32_t msg = 0;
    for (int i = 0; i < 2000; i++) {
        msg = rd(kRNMp0C2pmsg33);
        if (msg & 0x80000000u) break;
        IOSleep(1);
    }
    IOLog("RedNavi: C2PMSG_33 = 0x%08x\n", msg);

    uint32_t vramMB = rd(kRNRccConfigMemsize);
    IOLog("RedNavi: RCC_CONFIG_MEMSIZE = 0x%08x\n", vramMB);
    if (vramMB == 0 || vramMB == 0xFFFFFFFFu) {
        vramMB = kRNFallbackVramMB;
        IOLog("RedNavi: memsize register unusable, assuming %u MB\n", vramMB);
    }

    uint64_t base = ((uint64_t)vramMB << 20) - kRNDiscoveryTmrOffset;
    uint32_t sig = vramRead(base);
    IOLog("RedNavi: discovery base=0x%llx sig=0x%08x (expect 0x%08x)\n", base, sig, kRNBinarySignature);
    if (sig != kRNBinarySignature) {
        IOLog("RedNavi: IP discovery signature not found\n");
        return;
    }

    // struct binary_header: u32 signature, u16 ver_major, u16 ver_minor, u16 checksum, u16 size, then table list.
    uint32_t ver = vramRead(base + 4);
    uint32_t csumSize = vramRead(base + 8);
    uint32_t size = csumSize >> 16;
    IOLog("RedNavi: IP discovery found: version %u.%u size %u bytes\n", ver & 0xFFFF, ver >> 16, size);
    if (size == 0 || size > kRNDiscoveryTmrSize) size = kRNDiscoveryTmrSize;

    // Copy the whole table. Read it back with: ioreg -rn RedNaviProbe -l -x | grep ip-discovery
    OSData *blob = OSData::withCapacity(size);
    if (!blob) return;
    for (uint32_t off = 0; off < size; off += 4) {
        uint32_t v = vramRead(base + off);
        blob->appendBytes(&v, sizeof(v));
    }
    setProperty("ip-discovery", blob);

    // Also log it as hex lines for tools/decode_discovery.py (IOSleep keeps the log buffer from overflowing).
    const uint32_t *d = static_cast<const uint32_t *>(blob->getBytesNoCopy());
    for (uint32_t off = 0; off + 16 <= size; off += 16) {
        const uint32_t *l = d + off / 4;
        IOLog("RedNavi: DISC %04x: %08x %08x %08x %08x\n", off, l[0], l[1], l[2], l[3]);
        if ((off & 0xFF) == 0xF0) IOSleep(5);
    }
    disc = blob;
}

// Walk the discovery table (layout in include/discovery.h) and return the first base address of
// instance 0 of the given hardware ID. Every read is bounds-checked against the copied table.
bool RedNaviProbe::ipBase(uint16_t hwId, uint32_t *base, uint32_t idx) {
    if (!disc) return false;
    const uint8_t *b = static_cast<const uint8_t *>(disc->getBytesNoCopy());
    uint32_t len = disc->getLength();
    auto u16 = [&](uint32_t o) -> uint32_t { return o + 2 <= len ? static_cast<uint32_t>(b[o] | (b[o + 1] << 8)) : 0u; };
    auto u32 = [&](uint32_t o) -> uint32_t { return o + 4 <= len ? u16(o) | (u16(o + 2) << 16) : 0; };

    if (u16(4) >= 2) return false;                       // binary header v2 has another table layout
    uint32_t ipOff = u16(12);                            // table_list[0].offset (IP_DISCOVERY)
    uint32_t numDies = u16(ipOff + 12);
    bool base64 = u16(ipOff + 4) == 4 && (ipOff + 78 < len) && (b[ipOff + 78] & 1);
    if (numDies > 16) numDies = 16;

    for (uint32_t d = 0; d < numDies; d++) {
        uint32_t pos = u16(ipOff + 14 + d * 4 + 2);      // die_info[d].die_offset, absolute
        uint32_t numIps = u16(pos + 2);
        pos += 4;
        for (uint32_t i = 0; i < numIps && pos + 8 <= len; i++) {
            uint32_t nbase = b[pos + 3];
            if (u16(pos) == hwId && b[pos + 2] == 0 && idx < nbase) {
                if (base64 && u32(pos + 8 + idx * 8 + 4) != 0) return false;   // above 4 GB, not a register offset we can use
                *base = u32(pos + 8 + idx * (base64 ? 8 : 4));
                return true;
            }
            pos += 8 + nbase * (base64 ? 8 : 4);
        }
    }
    return false;
}

void RedNaviProbe::dumpRegs(const RNMailboxReg *list, size_t count, const char *property) {
    OSDictionary *out = OSDictionary::withCapacity(16);
    if (!out) return;
    uint32_t regLen = static_cast<uint32_t>(regs->getLength());

    for (size_t i = 0; i < count; i++) {
        const RNMailboxReg &r = list[i];
        uint32_t base = 0;
        if (!ipBase(r.hwId, &base, r.baseIdx)) {
            IOLog("RedNavi: %s: hardware ID %u base %u not in discovery table\n", r.name, r.hwId, r.baseIdx);
            continue;
        }
        uint32_t off = (base + r.dword) * 4;
        if (off + 4 > regLen) {
            IOLog("RedNavi: %s: byte offset 0x%x outside the %u byte register BAR\n", r.name, off, regLen);
            continue;
        }
        uint32_t v = rd(off);
        IOLog("RedNavi: %s @0x%05x = 0x%08x\n", r.name, off, v);
        OSNumber *n = OSNumber::withNumber(static_cast<uint64_t>(v), 64);
        if (n) { out->setObject(r.name, n); n->release(); }
    }
    setProperty(property, out);
    out->release();
}

// Copy one component into the VRAM staging buffer and run the bootloader handshake.
// Read the legacy x86 image of the PCI expansion ROM (amdgpu_read_bios / amdgpu_read_platform_bios).
void RedNaviProbe::dumpVbios() {
    uint32_t romReg = pci->configRead32(0x30);
    IODeviceMemory *rom = pci->getDeviceMemoryWithRegister(0x30);
    IOLog("RedNavi: ROM BAR register = 0x%08x, device memory %s\n", romReg, rom ? "present" : "absent");
    if (!rom) return;
    IOLog("RedNavi: ROM phys 0x%llx size 0x%llx\n", (uint64_t)rom->getPhysicalAddress(), (uint64_t)rom->getLength());

    pci->configWrite32(0x30, romReg | 1);                    // expansion ROM enable
    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(0x30);
    if (!map) { IOLog("RedNavi: could not map the expansion ROM\n"); pci->configWrite32(0x30, romReg); return; }

    const volatile uint8_t *p = reinterpret_cast<const volatile uint8_t *>(map->getVirtualAddress());
    uint32_t mapLen = static_cast<uint32_t>(map->getLength());
    uint32_t sig = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8);
    uint32_t size = static_cast<uint32_t>(p[2]) * 512;       // legacy image size in 512 byte units
    IOLog("RedNavi: ROM signature 0x%04x (expect 0xaa55), first image %u bytes, mapping %u bytes\n", sig, size, mapLen);
    if (sig == 0xAA55 && size && size <= mapLen && size <= (1u << 20)) {
        OSData *blob = OSData::withCapacity(size);
        if (blob) {
            for (uint32_t i = 0; i < size; i++) { uint8_t b = p[i]; blob->appendBytes(&b, 1); }
            setProperty("vbios", blob);
            vbiosCopy = blob;
            IOLog("RedNavi: VBIOS image of %u bytes published\n", size);
        }
    }
    map->release();
    pci->configWrite32(0x30, romReg);                        // leave the ROM decode as we found it
}

// One AUX transaction on engine `eng`, mirroring acquire_engine / submit_channel_request / read_channel_reply in dce_aux.c.
// action is the command nibble already shifted left by 4 (DP read 0x90, I2C write+MOT 0x40, I2C read 0x10, I2C read+MOT 0x50).
bool RedNaviProbe::auxTransfer(uint32_t eng, uint32_t action, uint32_t addr, uint8_t *data, uint32_t len, uint32_t *status, uint32_t *reply) {
    uint32_t dmu = 0;
    if (!ipBase(kRNHwIdDmu, &dmu, 2)) return false;
    uint32_t base = dmu + 0x1f50 + eng * 0x1c;           // DP_AUXn_AUX_CONTROL
    auto R = [&](uint32_t d) { return rd((base + d) * 4); };
    auto W = [&](uint32_t d, uint32_t v) { wr((base + d) * 4, v); };
    const uint32_t CONTROL = 0, SWCTL = 1, ARB = 2, INTC = 3, STAT = 4, DATA = 6;
    bool isWrite = (action == 0x80) || (action == 0x00) || (action == 0x40);
    *status = 0; *reply = 0xFF;

    if (!(R(CONTROL) & 1)) W(CONTROL, R(CONTROL) | 1);                    // AUX_EN
    W(ARB, R(ARB) | (1u << 16));                                           // AUX_SW_USE_AUX_REG_REQ
    if (((R(ARB) >> 2) & 3) != 1) { *status = R(ARB); W(ARB, (R(ARB) & ~(1u << 16)) | (1u << 17)); return false; }   // SW_CAN_ACCESS_AUX

    W(INTC, R(INTC) | (1u << 1));                                          // AUX_SW_DONE_ACK
    for (int i = 0; i < 100 && (R(STAT) & 1); i++) IODelay(100);

    uint32_t wr_bytes = (len ? 4 : 3) + (isWrite ? len : 0);
    W(SWCTL, (R(SWCTL) & ~((0xFu << 4) | (0xFFFFu << 16))) | (0u << 4) | (wr_bytes << 16));
    // AUX_SW_INDEX 0, AUX_SW_DATA_RW 0, autoincrement disabled for the first byte only
    uint32_t v = (1u << 31) | ((action | ((addr >> 16) & 0xF)) << 8);
    W(DATA, v);
    v &= ~(1u << 31);
    W(DATA, (v & 0xFFFF00FFu) | (((addr >> 8) & 0xFF) << 8));
    W(DATA, (v & 0xFFFF00FFu) | ((addr & 0xFF) << 8));
    if (len) W(DATA, (v & 0xFFFF00FFu) | (((len - 1) & 0xFF) << 8));
    if (isWrite) for (uint32_t i = 0; i < len; i++) W(DATA, (v & 0xFFFF00FFu) | (static_cast<uint32_t>(data[i]) << 8));
    W(SWCTL, R(SWCTL) | 1);                                                // AUX_SW_GO

    uint32_t st = 0;
    bool done = false;
    for (int i = 0; i < 2000; i++) { st = R(STAT); if (st & 1) { done = true; break; } IODelay(100); }
    *status = st;
    bool ok = false;
    uint32_t count = (st >> 24) & 0x1F;
    const uint32_t bad = (1u << 7) | (7u << 4) | (1u << 9) | (1u << 14) | (1u << 20) | (1u << 22) | (1u << 23);   // timeouts, HPD disconnect, invalid reply bits
    if (done && !(st & bad) && count) {
        W(DATA, 0);                                                        // AUX_SW_INDEX = 0
        W(DATA, 1u << 31);                                                 // AUX_SW_AUTOINCREMENT_DISABLE
        W(DATA, (1u << 31) | 1u);                                          // AUX_SW_DATA_RW = 1
        uint32_t first = (R(DATA) >> 8) & 0xFF;
        *reply = first >> 4;
        if (*reply == 0) {
            uint32_t n = count - 1;
            for (uint32_t i = 0; i < n && i < len; i++) data[i] = (R(DATA) >> 8) & 0xFF;
            ok = true;
        }
    }
    W(ARB, (R(ARB) & ~(1u << 16)) | (1u << 17));                           // release: DONE_USING_AUX_REG, clear USE request
    return ok;
}

// Queue one 64-byte command in inbox1 and wait for the firmware to consume it (dmub_srv_cmd_queue / _execute).
bool RedNaviProbe::dmubSend(const uint8_t *cmd, uint32_t len, uint32_t reply[16]) {
    if (!dmubUp || len > 64) return false;
    const uint32_t IN_WP = 0x1d6, IN_RP = 0x1d7;
    uint32_t wp = rd((dmubDmu + IN_WP) * 4), slot = wp % kRNDmubRbSize;
    uint64_t slotAddr = dmubMailbox + slot;
    for (uint32_t i = 0; i < 64; i += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4; k++) if (i + k < len) v |= static_cast<uint32_t>(cmd[i + k]) << (8 * k);
        vramWrite(slotAddr + i, v);
    }
    uint32_t newWp = (wp + 64) % kRNDmubRbSize;
    wr((dmubDmu + IN_WP) * 4, newWp);
    uint32_t rp = 0;
    for (int i = 0; i < 1000; i++) { rp = rd((dmubDmu + IN_RP) * 4); if (rp == newWp) break; IOSleep(1); }
    for (uint32_t i = 0; i < 16; i++) reply[i] = vramRead(slotAddr + i * 4);
    return rp == newWp;
}

// VBIOS command tables executed by the DMCUB (DMUB_CMD__VBIOS), the way dc/bios/command_table2.c does when
// dc->debug.dmub_command_table is set. Here: DIG1_TRANSMITTER_CONTROL with action INIT for path 0
// (DisplayPort, UNIPHY2 enum 1 = transmitter E = phy id 4, HPD pin 5).
void RedNaviProbe::dpInit() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_dp_init", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: DP transmitter init not requested (boot-arg rednavi_dp_init=1)\n");
        return;
    }
    if (!dmubUp) { IOLog("RedNavi: DP init: DMCUB not running\n"); return; }
    uint8_t cmd[64] = {};
    // header: type DMUB_CMD__VBIOS (128), sub_type DIG1_TRANSMITTER_CONTROL (1), payload 16 bytes (<< 24)
    cmd[0] = 128; cmd[1] = 1; cmd[3] = 16;
    // struct dig_transmitter_control_parameters_v1_6 at offset 4
    cmd[4 + 0] = 4;       // phyid: UNIPHY E
    cmd[4 + 1] = 7;       // ATOM_TRANSMITTER_ACTION_INIT
    cmd[4 + 2] = 0;       // digmode: DP
    cmd[4 + 3] = 4;       // lanenum
    cmd[4 + 8] = 5;       // hpdsel: HPD5 (pin id 5)
    cmd[4 + 9] = 0;       // digfe_sel
    cmd[4 + 10] = 19;     // connector object id: DisplayPort
    uint32_t reply[16];
    bool ok = dmubSend(cmd, 20, reply);
    IOLog("RedNavi: DP transmitter INIT via DMUB: %s, slot %08x %08x %08x %08x %08x\n", ok ? "consumed" : "NOT consumed", reply[0], reply[1], reply[2], reply[3], reply[4]);
    const char *n[3] = {"consumed", "slot0", "slot1"};
    uint32_t v[3] = {ok, reply[0], reply[1]};
    rnPublish(this, "dp-init", n, v, 3);
}

bool RedNaviProbe::dpcdWrite(uint32_t eng, uint32_t addr, const uint8_t *d, uint32_t n) {
    uint8_t buf[16];
    if (n > 16) return false;
    memcpy(buf, d, n);
    uint32_t st, rep;
    return auxTransfer(eng, 0x80, addr, buf, n, &st, &rep);
}

bool RedNaviProbe::dpcdRead(uint32_t eng, uint32_t addr, uint8_t *d, uint32_t n) {
    uint32_t st, rep;
    return auxTransfer(eng, 0x90, addr, d, n, &st, &rep);
}

// DIG1_TRANSMITTER_CONTROL for path 0 (phy id 4, HPD 5, connector id 19) through the DMCUB.
bool RedNaviProbe::dpTx(uint8_t action, uint8_t modeOrLane, uint8_t lanes, uint32_t symclk10k) {
    uint8_t cmd[64] = {};
    cmd[0] = 128; cmd[1] = 1; cmd[3] = 16;
    cmd[4 + 0] = 4; cmd[4 + 1] = action; cmd[4 + 2] = modeOrLane; cmd[4 + 3] = lanes;
    memcpy(cmd + 4 + 4, &symclk10k, 4);
    cmd[4 + 8] = 5; cmd[4 + 9] = 0; cmd[4 + 10] = 19;
    uint32_t reply[16];
    return dmubSend(cmd, 20, reply);
}

void RedNaviProbe::dpTrain() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_dp_train", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: DP link training not requested (boot-arg rednavi_dp_train=1)\n");
        return;
    }
    if (!dmubUp) { IOLog("RedNavi: DP training: DMCUB not running\n"); return; }
    // Link rate: RBR (0x06, 162000 kHz symbol clock) like Linux uses for 1080p60 8 bpc, which is also the state the replay table
    // was captured in (DP_DTO0, VID_M/N); rednavi_dp_rate=0x14 selects HBR2 as before.
    uint32_t rateArg = selRate;                                             // from the selected mode (RBR for 1080p60)
    PE_parse_boot_argn("rednavi_dp_rate", &rateArg, sizeof(rateArg));
    const uint32_t aux = 4, lanes = 4, rateCode = rateArg, symclk10k = rateCode * 27000 / 10;
    uint32_t dmu = dmubDmu;
    // DP4 back end and DIG4 registers (dcn_3_2_0_offset.h, base index 2)
    const uint32_t DP_CONFIG = 0x250b, DP_LINK_CNTL = 0x2508, DP_DPHY_CNTL = 0x2517, DP_PATTERN_SEL = 0x2518, DP_SCRAM = 0x251e;
    const uint32_t DIG_BE_CNTL = 0x24b1;
    auto R = [&](uint32_t d) { return rd((dmu + d) * 4); };
    auto W = [&](uint32_t d, uint32_t v) { wr((dmu + d) * 4, v); };
    auto U = [&](uint32_t d, uint32_t mask, uint32_t v) { W(d, (R(d) & ~mask) | (v & mask)); };

    uint8_t caps[16] = {};
    if (!dpcdRead(aux, 0, caps, 16)) { IOLog("RedNavi: DP training: no DPCD\n"); return; }
    bool tps3 = (caps[2] & 0x40) != 0 && rateCode >= 0x14, enhanced = (caps[2] & 0x80) != 0;

    // Sink and source setup (dp_enable_link_phy, enc1_configure_encoder, dcn10_link_encoder_setup)
    uint8_t d0 = 1;
    dpcdWrite(aux, 0x600, &d0, 1);                                          // SET_POWER: D0
    U(DIG_BE_CNTL, 0xFu << 16, 0);                                          // DIG_MODE = DP
    U(DP_CONFIG, 3, lanes - 1);                                             // DP_UDI_LANES
    U(DP_SCRAM, 1u << 4, 1u << 4);                                          // DPHY_SCRAMBLER_ADVANCE
    uint8_t link[2] = {static_cast<uint8_t>(rateCode), static_cast<uint8_t>(lanes | (enhanced ? 0x80 : 0))};
    dpcdWrite(aux, 0x100, link, 2);                                         // LINK_BW_SET, LANE_COUNT_SET
    uint8_t ds = 0; dpcdWrite(aux, 0x107, &ds, 1);                          // no downspread
    uint8_t enc = 1; dpcdWrite(aux, 0x108, &enc, 1);                        // 8b/10b
    bool txOk = dpTx(1, 0, static_cast<uint8_t>(lanes), symclk10k);          // ATOM_TRANSMITTER_ACTION_ENABLE
    IOLog("RedNavi: DP training: transmitter ENABLE %s, DIG4 BE_CNTL=0x%08x, link %02x %02x\n", txOk ? "consumed" : "NOT consumed", R(DIG_BE_CNTL), link[0], link[1]);

    uint8_t vs[4] = {0, 0, 0, 0}, pe[4] = {0, 0, 0, 0};
    uint8_t st[6] = {};
    bool crOk = false, eqOk = false;
    uint32_t crTries = 0, eqTries = 0;
    auto laneSet = [&](uint32_t i) -> uint8_t { return static_cast<uint8_t>(vs[i] | (pe[i] << 3) | (vs[i] == 3 ? 4 : 0) | (pe[i] == 3 ? 0x20 : 0)); };
    auto applyDrive = [&](void) {
        for (uint32_t i = 0; i < lanes; i++) dpTx(11, laneSet(i), static_cast<uint8_t>(lanes), symclk10k);   // SETUP_VSEMPH
    };
    auto readStatus = [&]() { dpcdRead(aux, 0x202, st, 6); };
    auto adjust = [&]() {
        for (uint32_t i = 0; i < lanes; i++) {
            uint8_t req = st[4 + i / 2];
            uint8_t sh = (i & 1) ? 4 : 0;
            vs[i] = (req >> sh) & 3;
            pe[i] = (req >> (sh + 2)) & 3;
            if (vs[i] + pe[i] > 3) pe[i] = 3 - vs[i];
        }
    };

    // Clock recovery: TPS1
    U(DP_PATTERN_SEL, 3, 0);
    U(DP_LINK_CNTL, 1u << 4, 0);                                            // training not complete
    U(DP_DPHY_CNTL, 1u << 16, 0);                                           // PHY bypass off: send the training pattern
    for (crTries = 1; crTries <= 8 && !crOk; crTries++) {
        applyDrive();
        uint8_t tr[5] = {0x21, laneSet(0), laneSet(1), laneSet(2), laneSet(3)};   // TPS1, scrambling disabled, lane drive
        dpcdWrite(aux, 0x102, tr, 5);
        IOSleep(1);                                                         // TRAINING_AUX_RD_INTERVAL: 100 us minimum
        readStatus();
        crOk = (st[0] & 1) && (st[0] & 0x10) && (st[1] & 1) && (st[1] & 0x10);
        if (!crOk) adjust();
    }
    if (crOk) {
        // Channel equalisation: TPS3 when the sink has it (HBR2), else TPS2
        U(DP_PATTERN_SEL, 3, tps3 ? 2 : 1);
        uint8_t pat = tps3 ? 0x23 : 0x22;
        for (eqTries = 1; eqTries <= 6 && !eqOk; eqTries++) {
            applyDrive();
            uint8_t tr[5] = {pat, laneSet(0), laneSet(1), laneSet(2), laneSet(3)};
            dpcdWrite(aux, 0x102, tr, 5);
            IOSleep(1);                                                     // 400 us default interval
            readStatus();
            bool cr = (st[0] & 1) && (st[0] & 0x10) && (st[1] & 1) && (st[1] & 0x10);
            bool eq = (st[0] & 2) && (st[0] & 0x20) && (st[1] & 2) && (st[1] & 0x20);
            bool sl = (st[0] & 4) && (st[0] & 0x40) && (st[1] & 4) && (st[1] & 0x40);
            eqOk = cr && eq && sl && (st[2] & 1);
            if (!eqOk) adjust();
        }
    }
    uint8_t off = 0;
    dpcdWrite(aux, 0x102, &off, 1);                                         // training pattern off
    if (crOk && eqOk) U(DP_LINK_CNTL, 1u << 4, 1u << 4);                    // DP_LINK_TRAINING_COMPLETE
    readStatus();
    IOLog("RedNavi: DP training: CR %s after %u tries, EQ %s after %u tries; DPCD 202..207 = %02x %02x %02x %02x %02x %02x; drive vs %u%u%u%u pe %u%u%u%u\n",
          crOk ? "done" : "FAILED", crTries, eqOk ? "done" : "FAILED", eqTries, st[0], st[1], st[2], st[3], st[4], st[5],
          vs[0], vs[1], vs[2], vs[3], pe[0], pe[1], pe[2], pe[3]);
    const char *n[9] = {"tx_ok", "cr_ok", "eq_ok", "cr_tries", "eq_tries", "status_202_203", "status_204_205", "adjust_206_207", "tps3"};
    uint32_t v[9] = {txOk, crOk, eqOk, crTries, eqTries, static_cast<uint32_t>(st[0] | (st[1] << 8)), static_cast<uint32_t>(st[2] | (st[3] << 8)), static_cast<uint32_t>(st[4] | (st[5] << 8)), tps3};
    rnPublish(this, "dp-train", n, v, 9);
}

// A recognisable 1920x1080 XRGB8888 image: red/green gradient, a checker in blue, a white frame and
// eight colour bars along the bottom, so a wrong pitch, offset or format is obvious on screen.
void RedNaviProbe::fillScanout() {
    static const uint32_t bars[8] = {0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00, 0xFF00FF, 0xFF0000, 0x0000FF, 0x000000};
    const uint32_t W = selMode.hactive, H = selMode.vactive;
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            uint32_t v;
            if (x < 8 || x + 8 >= W || y < 8 || y + 8 >= H) v = 0xFFFFFF;
            else if (y >= H - H / 6) v = bars[(x * 8) / W];
            else {
                uint32_t r = (x * 255) / W, g = (y * 255) / H, b = (((x >> 4) + (y >> 4)) & 1) ? 200 : 60;
                v = (r << 16) | (g << 8) | b;
            }
            vramWrite(kRNScanoutVramOff + (static_cast<uint64_t>(y) * selPitch + x) * 4, v);
        }
        if ((y & 63) == 0) IOSleep(1);
    }
}

// Opt-in (rednavi_smu=1): first talk to the SMU through the compat layer. Only read-style messages: the
// test message, the firmware version and the driver interface version. Published as "smu-info".
void RedNaviProbe::smuProbe(const char *property, bool send) {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_smu", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: SMU messages not requested (boot-arg rednavi_smu=1)\n");
        return;
    }
    uint32_t mp1 = 0;
    if (!ipBase(kRNHwIdMp1, &mp1)) { IOLog("RedNavi: SMU: no MP1 base\n"); return; }

    struct rn_device dev;
    struct rn_device *adev = &dev;       // the RREG32 macros expect a pointer called adev
    rn_device_init(adev, "RedNavi SMU", reinterpret_cast<volatile void *>(regs->getVirtualAddress()), regs->getLength());
    adev->reg_offset[RN_MP1_HWIP][0][0] = mp1;

    OSDictionary *out = OSDictionary::withCapacity(12);
    if (!out) return;
    rnPublishOne(out, "mp1_base", mp1);
    // MP1_FIRMWARE_FLAGS (SMN 0x3b10024: MP1_Public | smnMP1_FIRMWARE_FLAGS) through the NBIO RSMU index/data pair;
    // bit 0 is INTERRUPTS_ENABLED, which smu_v13_0_check_fw_status wants set before messages are sent.
    uint32_t nbif = 0;
    if (ipBase(108, &nbif, 1)) {
        wr((nbif + 0) * 4, 0x03b10024u);
        rnPublishOne(out, "fw_flags", rd((nbif + 1) * 4));
        rnPublishOne(out, "nbif_base1", nbif);
    }
    rnPublishOne(out, "c2pmsg_66_before", RREG32_SOC15(RN_MP1, 0, mmMP1_SMN_C2PMSG_66));
    rnPublishOne(out, "c2pmsg_82_before", RREG32_SOC15(RN_MP1, 0, mmMP1_SMN_C2PMSG_82));
    rnPublishOne(out, "c2pmsg_90_before", RREG32_SOC15(RN_MP1, 0, mmMP1_SMN_C2PMSG_90));
    // The whole MP1 mailbox block and the PSP state, to compare with docs/logs/linux-reference/smu_mailbox_normal.txt.
    for (uint32_t n = 66; n <= 90; n++) {
        char key[24];
        snprintf(key, sizeof(key), "m1_%u", n);
        rnPublishOne(out, key, rd((mp1 + 0x282 + (n - 66)) * 4));
    }
    uint32_t mp0 = 0;
    if (ipBase(kRNHwIdMp0, &mp0)) {
        rnPublishOne(out, "m0_33", rd((mp0 + 0x61) * 4));
        rnPublishOne(out, "m0_35", rd((mp0 + 0x63) * 4));
        rnPublishOne(out, "m0_81", rd((mp0 + 0x91) * 4));
    }
    static const struct { const char *name; uint32_t msg; uint32_t arg; } msgs[] = {
        {"smu_version", PPSMC_MSG_GetSmuVersion, 0},
        {"driver_if_version", PPSMC_MSG_GetDriverIfVersion, 0},
        {"test", PPSMC_MSG_TestMessage, 0x12345678},
    };
    for (const auto &m : msgs) {
        if (!send) break;                    // read-only call: only the registers above
        uint32_t result = 0, resp = 0;
        int rc = rn_smu13_send(adev, m.msg, m.arg, &result, &resp, 100000);
        IOLog("RedNavi: SMU %s: rc %d, response 0x%x, argument 0x%08x\n", m.name, rc, resp, result);
        char key[40];
        snprintf(key, sizeof(key), "%s_rc", m.name);
        rnPublishOne(out, key, static_cast<uint64_t>(static_cast<uint32_t>(rc)));
        snprintf(key, sizeof(key), "%s_resp", m.name);
        rnPublishOne(out, key, resp);
        snprintf(key, sizeof(key), "%s_arg", m.name);
        rnPublishOne(out, key, result);
        if (rc == -ETIMEDOUT) WREG32_SOC15(RN_MP1, 0, mmMP1_SMN_C2PMSG_90, 1);   // diagnostic only: let the next message try
    }
    rnPublishOne(out, "c2pmsg_90_after", RREG32_SOC15(RN_MP1, 0, mmMP1_SMN_C2PMSG_90));
    setProperty(property, out);
    out->release();
}

// Opt-in (rednavi_fb=1): tell the RedNaviFB kext (a separate bundle, see RedNaviFB/kern_fb.cpp) where the lit
// scanout surface is. It matches on rednavi-fb-ready, so that property has to be set last.
void RedNaviProbe::publishFramebuffer() {
    uint32_t arg = 0, replay = 0;
    if (!PE_parse_boot_argn("rednavi_fb", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: framebuffer not requested (boot-arg rednavi_fb=1)\n");
        return;
    }
    if (!PE_parse_boot_argn("rednavi_dcn_replay", &replay, sizeof(replay)) || replay < 14) {
        IOLog("RedNavi: framebuffer needs rednavi_dcn_replay=14, skipping\n");
        return;
    }
    IODeviceMemory *bar0 = pci->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0 + kRNVramBar * 4);
    if (!bar0) { IOLog("RedNavi: framebuffer: no BAR0\n"); return; }
    uint64_t phys = static_cast<uint64_t>(bar0->getPhysicalAddress());
    uint64_t len = static_cast<uint64_t>(bar0->getLength());
    IOLog("RedNavi: framebuffer: BAR0 phys 0x%llx, %llu MB\n", phys, len >> 20);
    uint64_t need = static_cast<uint64_t>(selPitch) * 4 * selMode.vactive;
    if (kRNScanoutVramOff + need > len) { IOLog("RedNavi: framebuffer: mode does not fit in BAR0\n"); return; }
    setProperty("fb-width", selMode.hactive, 32);
    setProperty("fb-height", selMode.vactive, 32);
    setProperty("fb-pitch", selPitch * 4, 32);
    setProperty("fb-refresh", selMode.refresh_hz, 32);
    setProperty("fb-scanout-phys", phys + kRNScanoutVramOff, 64);
    setProperty("rednavi-fb-ready", kOSBooleanTrue);
}

void RedNaviProbe::dcnReplay() {
    uint32_t maxGroup = 0;
    if (!PE_parse_boot_argn("rednavi_dcn_replay", &maxGroup, sizeof(maxGroup)) || maxGroup == 0) return;
    if (maxGroup > 14) maxGroup = 14;
    uint32_t base[4] = {};
    for (uint32_t i = 0; i < 4; i++) if (!ipBase(kRNHwIdDmu, &base[i], i)) { IOLog("RedNavi: replay: no DMU base %u\n", i); return; }
    uint32_t regLen = static_cast<uint32_t>(regs->getLength());
    // State found before we touch anything (the VBIOS may have lit the pipe already), then quiesce it:
    // DC assumes the display is disabled before it programs the timing.
    {
        const uint32_t pre[16][2] = {{2, 0x1b41}, {2, 0x1b2a}, {2, 0x1b2f}, {2, 0x250c}, {2, 0x248b}, {2, 0x24b1}, {2, 0x24b2}, {2, 0x2511}, {2, 0x2512}, {1, 0x81}, {1, 0x64}, {1, 0x99}, {1, 0xb6}, {2, 0x475}, {2, 0x476}, {2, 0x46f}};
        const char *pn[16] = {"otg_control", "h_total", "v_total", "vid_stream_cntl", "fe_cntl", "be_cntl", "be_en", "vid_n", "vid_m", "dp_dto0_phase", "dentist_dispclk", "dppclk0_dto", "dppclk_dto_ctrl", "vm_fb_base", "vm_fb_top", "sdpif_cfg0"};
        uint32_t pv[16];
        for (int i = 0; i < 16; i++) pv[i] = rd((base[pre[i][0]] + pre[i][1]) * 4);
        RN_STEP("RedNavi: replay before: OTG_CONTROL=0x%08x H_TOTAL=0x%x V_TOTAL=0x%x VID_STREAM=0x%08x FE=0x%08x BE=0x%08x BE_EN=0x%08x VID_N=0x%x VID_M=0x%x DTO0=0x%08x\n", pv[0], pv[1], pv[2], pv[3], pv[4], pv[5], pv[6], pv[7], pv[8], pv[9]);
        rnPublish(this, "dcn-replay-pre", pn, pv, 16);
        uint32_t vidReg = (base[2] + 0x250c) * 4, otgReg = (base[2] + 0x1b41) * 4;
        if (maxGroup >= 3) {
            if (rd(vidReg) & 1) {                                           // stop the DP video stream first
                wr(vidReg, rd(vidReg) & ~1u);
                for (int i = 0; i < 100 && (rd(vidReg) & (1u << 16)); i++) IOSleep(1);
                RN_STEP("RedNavi: replay: DP video stream stopped, VID_STREAM=0x%08x\n", rd(vidReg));
            }
            if (rd(otgReg) & 1) {                                           // optc1_disable_crtc
                wr(otgReg, rd(otgReg) & ~1u);
                for (int i = 0; i < 200 && (rd(otgReg) & (1u << 16)); i++) IOSleep(1);
                RN_STEP("RedNavi: replay: OTG disabled, OTG_CONTROL=0x%08x\n", rd(otgReg));
            }
        }
    }
    static const char *const names[15] = {"", "clocks", "optc clocks", "otg timing", "opp", "mpc", "dig/dp stream", "otg enable", "test pattern", "dp stream enable", "hubbub", "dpp and clocks", "mpc tree", "hubp surface", "pattern off"};
    uint32_t mismatches = 0;
    OSDictionary *mm = OSDictionary::withCapacity(16);
    const rn_mode_ctx modeCtx = {&selMode, selPitch, 4};
    const rn_mode *baseMode = rn_base_mode();
    const bool baseState = selMode.pclk_khz == baseMode->pclk_khz && selMode.hactive == baseMode->hactive &&
                           selMode.vactive == baseMode->vactive && selMode.htotal == baseMode->htotal && selMode.vtotal == baseMode->vtotal;
    // The value to write for a table entry: the captured one, or the one computed for the selected mode.
    auto valueFor = [&](const RNReplayReg &r) {
        uint32_t v = r.value;
        if (!baseState) rn_mode_register(r.name, r.value, &modeCtx, &v);
        return v;
    };
    if (!baseState && maxGroup >= 1) {                                       // DISPCLK high enough for the pixel clock?
        uint32_t dentOff = (base[1] + 0x64) * 4, cur = rd(dentOff);
        int changed = 0;
        uint32_t nv = rn_dispclk_register(cur, &selMode, &changed);
        if (changed) {
            wr(dentOff, nv);
            IOSleep(5);
            RN_STEP("RedNavi: replay: DENTIST_DISPCLK_CNTL 0x%08x -> 0x%08x (read back 0x%08x) for %u kHz\n", cur, nv, rd(dentOff), selMode.pclk_khz);
        } else {
            RN_STEP("RedNavi: replay: DENTIST_DISPCLK_CNTL 0x%08x is enough for %u kHz\n", cur, selMode.pclk_khz);
        }
        setProperty("dentist-dispclk-cntl", static_cast<uint64_t>(rd(dentOff)), 32);
    }
    for (uint32_t g = 1; g <= maxGroup; g++) {
        RN_STEP("RedNavi: replay group %u: %s\n", g, names[g]);
        if (g == 13) { RN_STEP("RedNavi: replay: writing the scanout image to VRAM\n"); fillScanout(); RN_STEP("RedNavi: replay: scanout image written\n"); }
        for (const RNReplayReg &r : kRNReplay) {
            if (r.group != g) continue;
            uint32_t off = (base[r.baseIdx] + r.dword) * 4;
            if (off + 4 > regLen) { IOLog("RedNavi: replay: %s outside the BAR\n", r.name); continue; }
            wr(off, valueFor(r));
        }
        if (g == 7) {                                                       // wait for the OTG to report enabled
            uint32_t ctl = (base[2] + 0x1b41) * 4, ms = 0;
            for (; ms < 200; ms++) { if (rd(ctl) & (1u << 16)) break; IOSleep(1); }
            RN_STEP("RedNavi: replay: OTG_CONTROL = 0x%08x after %u ms\n", rd(ctl), ms);
        }
        for (const RNReplayReg &r : kRNReplay) {                            // read back, ignoring status bits
            if (r.group != g) continue;
            uint32_t v = rd((base[r.baseIdx] + r.dword) * 4), want = valueFor(r);
            if (v != want) {
                mismatches++;
                if (mismatches <= 24) IOLog("RedNavi: replay: %s wrote 0x%08x read 0x%08x\n", r.name, want, v);
                if (mm && mismatches <= 16) {
                    char txt[40];
                    snprintf(txt, sizeof(txt), "wrote 0x%08x read 0x%08x", want, v);
                    OSString *str = OSString::withCString(txt);
                    if (str) { mm->setObject(r.name, str); str->release(); }
                }
            }
        }
        IOSleep(40);
    }
    // Second pass: a few registers (line buffer partitions) do not take a write made early in the sequence. Write the
    // ones that read back wrong once more, now that the pipe runs.
    uint32_t fixed = 0;
    for (const RNReplayReg &r : kRNReplay) {
        if (r.group > maxGroup) continue;
        uint32_t want = valueFor(r), off = (base[r.baseIdx] + r.dword) * 4;
        if (off + 4 > regLen || rd(off) == want) continue;
        wr(off, want);
        if (rd(off) == want) { fixed++; IOLog("RedNavi: replay: %s took the second write\n", r.name); }
    }
    if (fixed) mismatches -= fixed < mismatches ? fixed : mismatches;
    // Status after the replay: is the OTG counting, is the stream flowing?
    uint32_t pos0 = rd((base[2] + 0x1b4a) * 4);
    IOSleep(2);
    uint32_t pos1 = rd((base[2] + 0x1b4a) * 4);
    uint32_t otgCtl = rd((base[2] + 0x1b41) * 4), otgStatus = rd((base[2] + 0x1b49) * 4);
    uint32_t vid = rd((base[2] + 0x250c) * 4), dpg = rd((base[2] + 0x185b) * 4), link = rd((base[2] + 0x2508) * 4);
    uint32_t beEn = rd((base[2] + 0x24b2) * 4), fe = rd((base[2] + 0x248b) * 4);
    IOLog("RedNavi: replay done: %u mismatches; OTG_CONTROL=0x%08x STATUS=0x%08x POS 0x%08x -> 0x%08x; DP_VID_STREAM_CNTL=0x%08x DPG_STATUS=0x%08x DP_LINK_CNTL=0x%08x BE_EN=0x%08x FE=0x%08x\n",
          mismatches, otgCtl, otgStatus, pos0, pos1, vid, dpg, link, beEn, fe);
    const char *n[12] = {"groups", "mismatches", "otg_control", "otg_status", "pos0", "pos1", "vid_stream_cntl", "dpg_status", "dp_link_cntl", "be_en", "fe_cntl", "otg_moving"};
    uint32_t v[12] = {maxGroup, mismatches, otgCtl, otgStatus, pos0, pos1, vid, dpg, link, beEn, fe, pos0 != pos1};
    rnPublish(this, "dcn-replay", n, v, 12);
    if (mm) { setProperty("dcn-replay-mismatch", mm); mm->release(); }
}

void RedNaviProbe::dcnDump() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_dcn_dump_unsafe", &arg, sizeof(arg)) || arg != 1) return;
    uint32_t dmu = 0;
    if (!ipBase(kRNHwIdDmu, &dmu, 2)) return;
    uint32_t regLen = static_cast<uint32_t>(regs->getLength());
    OSData *blob = OSData::withCapacity(kRNDcnRegCount * 4);
    if (!blob) return;
    for (uint32_t i = 0; i < kRNDcnRegCount; i++) {
        uint32_t off = (dmu + kRNDcnRegs[i]) * 4, v = 0xDEADBEEF;
        if (off + 4 <= regLen) v = rd(off);
        blob->appendBytes(&v, 4);
    }
    setProperty("dcn-dump", blob);
    blob->release();
    IOLog("RedNavi: DCN dump of %u registers published\n", kRNDcnRegCount);
}

void RedNaviProbe::probeAux() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_edid", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: AUX/EDID read not requested (boot-arg rednavi_edid=1)\n");
        return;
    }
    OSDictionary *out = OSDictionary::withCapacity(20);
    if (!out) return;
    int found = -1;
    uint8_t dpcd[16] = {};
    for (uint32_t eng = 0; eng < 5; eng++) {
        uint8_t buf[16] = {};
        uint32_t st = 0, rep = 0;
        bool ok = auxTransfer(eng, 0x90, 0x000, buf, 16, &st, &rep);
        IOLog("RedNavi: AUX%u DPCD read: %s, SW_STATUS=0x%08x, reply=0x%x, first bytes %02x %02x %02x %02x\n", eng, ok ? "ACK" : "no", st, rep, buf[0], buf[1], buf[2], buf[3]);
        char name[24];
        snprintf(name, sizeof(name), "aux%u_status", eng);
        OSNumber *n = OSNumber::withNumber(static_cast<uint64_t>(st), 64);
        if (n) { out->setObject(name, n); n->release(); }
        snprintf(name, sizeof(name), "aux%u_reply", eng);
        n = OSNumber::withNumber(static_cast<uint64_t>(rep), 64);
        if (n) { out->setObject(name, n); n->release(); }
        if (ok && found < 0) { found = static_cast<int>(eng); memcpy(dpcd, buf, 16); }
    }
    OSNumber *fn = OSNumber::withNumber(static_cast<uint64_t>(found < 0 ? 0xFF : found), 64);
    if (fn) { out->setObject("engine", fn); fn->release(); }
    setProperty("aux-probe", out);
    out->release();
    if (found < 0) { IOLog("RedNavi: no AUX engine answered\n"); return; }
    OSData *d = OSData::withBytes(dpcd, 16);
    if (d) { setProperty("dpcd", d); d->release(); }

    // EDID: set the offset to 0 (I2C write with MOT), then read 16 byte chunks (MOT until the last one).
    uint8_t edid[256] = {};
    uint32_t total = 128, got = 0;
    uint8_t zero = 0;
    uint32_t st = 0, rep = 0;
    if (!auxTransfer(static_cast<uint32_t>(found), 0x40, 0x50, &zero, 1, &st, &rep)) {
        IOLog("RedNavi: EDID: offset write failed, SW_STATUS=0x%08x reply=0x%x\n", st, rep);
        return;
    }
    while (got < total) {
        bool last = got + 16 >= total;
        if (!auxTransfer(static_cast<uint32_t>(found), last ? 0x10 : 0x50, 0x50, edid + got, 16, &st, &rep)) {
            IOLog("RedNavi: EDID: read at %u failed, SW_STATUS=0x%08x reply=0x%x\n", got, st, rep);
            break;
        }
        got += 16;
        if (got == 128 && edid[126] && total == 128) total = 256;            // one extension block
    }
    IOLog("RedNavi: EDID: %u bytes read from AUX%d\n", got, found);
    if (got) {
        OSData *e = OSData::withBytes(edid, got);
        if (e) { setProperty("edid", e); e->release(); }
        publishModes(edid, got);
    }
}

// Pick the mode to light: boot-arg rednavi_mode=WxH@R if the monitor lists it, else the preferred mode of the EDID, else the
// 1080p60 the replay table was captured at. Modes the link cannot carry (above HBR2 x4) are skipped.
void RedNaviProbe::selectMode() {
    selMode = *rn_base_mode();
    uint32_t w = 0, h = 0, r = 0;
    char arg[32] = {};
    if (PE_parse_boot_argn("rednavi_mode", arg, sizeof(arg))) {
        const char *p = arg;
        auto num = [&](uint32_t *out) { *out = 0; while (*p >= '0' && *p <= '9') *out = *out * 10 + static_cast<uint32_t>(*p++ - '0'); };
        num(&w); if (*p == 'x') { p++; num(&h); } if (*p == '@') { p++; num(&r); }
    }
    const rn_mode *pick = nullptr;
    for (unsigned i = 0; i < edidInfo.mode_count && !pick; i++) {
        const rn_mode &m = edidInfo.modes[i];
        int rate = rn_min_dp_link_rate(&m, 4);
        if (rate < 0 || rate > 0x14) continue;
        if (w) { if (m.hactive == w && m.vactive == h && (!r || m.refresh_hz == r)) pick = &m; }
        else if (m.preferred) pick = &m;
    }
    if (w && !pick) IOLog("RedNavi: mode %ux%u@%u is not usable on this monitor, falling back\n", w, h, r);
    if (!pick) {
        for (unsigned i = 0; i < edidInfo.mode_count && !pick; i++) {            // preferred mode, then the first one the link carries
            int rate = rn_min_dp_link_rate(&edidInfo.modes[i], 4);
            if (rate >= 0 && rate <= 0x14 && (edidInfo.modes[i].preferred || !w)) pick = &edidInfo.modes[i];
        }
    }
    if (pick) selMode = *pick;
    int rate = rn_min_dp_link_rate(&selMode, 4);
    const rn_mode *base = rn_base_mode();
    bool isBase = selMode.pclk_khz == base->pclk_khz && selMode.hactive == base->hactive && selMode.vactive == base->vactive && selMode.htotal == base->htotal;
    selRate = isBase ? 0x06 : static_cast<uint32_t>(rate < 0x06 ? 0x06 : rate);    // the base state was captured at RBR
    selPitch = rn_pitch_pixels(selMode.hactive);
    IOLog("RedNavi: mode %ux%u@%u pclk %u kHz, link rate 0x%02x, pitch %u pixels\n", selMode.hactive, selMode.vactive, selMode.refresh_hz, selMode.pclk_khz, selRate, selPitch);
    uint32_t vals[6] = {selMode.hactive, selMode.vactive, selMode.refresh_hz, selMode.pclk_khz, selRate, selPitch};
    const char *names[6] = {"width", "height", "refresh", "pclk_khz", "link_rate", "pitch_pixels"};
    rnPublish(this, "selected-mode", names, vals, 6);
}

// Interpret the EDID and publish the modes as "modes" (strings like "1920x1080@60 148500 kHz"), plus the monitor name.
void RedNaviProbe::publishModes(const uint8_t *edid, size_t len) {
    if (rn_edid_parse(edid, len, &edidInfo) != 0) { IOLog("RedNavi: EDID: header or checksum wrong, no modes\n"); return; }
    OSArray *list = OSArray::withCapacity(edidInfo.mode_count);
    if (!list) return;
    for (unsigned i = 0; i < edidInfo.mode_count; i++) {
        const rn_mode &m = edidInfo.modes[i];
        char line[64];
        snprintf(line, sizeof(line), "%ux%u@%u %u kHz%s%s", m.hactive, m.vactive, m.refresh_hz, m.pclk_khz,
                 m.preferred ? " preferred" : "", m.vic ? " (CTA)" : "");
        OSString *str = OSString::withCString(line);
        if (str) { list->setObject(str); str->release(); }
        IOLog("RedNavi: EDID mode %u: %s\n", i, line);
    }
    setProperty("modes", list);
    list->release();
    OSString *name = OSString::withCString(edidInfo.name);
    if (name) { setProperty("monitor-name", name); name->release(); }
}

bool RedNaviProbe::loadComponent(uint32_t mp0, const char *name, uint32_t cmd, const uint8_t *data, uint32_t len, uint32_t version) {
    uint32_t r35 = (mp0 + kRNMp0C2pmsg35) * 4, r36 = (mp0 + kRNMp0C2pmsg36) * 4, r58 = (mp0 + kRNMp0C2pmsg58) * 4, r81 = (mp0 + kRNMp0C2pmsg81) * 4;

    uint32_t st = 0;
    for (int i = 0; i < 2000; i++) { st = rd(r35); if (st == kRNPspBlReady) break; IOSleep(1); }
    if (st != kRNPspBlReady) { IOLog("RedNavi: PSP %s: bootloader not ready (C2PMSG_35 = 0x%08x)\n", name, st); return false; }

    // psp_copy_fw: zero the whole 1 MB buffer, then copy the image.
    if (len > kRNPspBufSize) { IOLog("RedNavi: PSP %s: image larger than the staging buffer\n", name); return false; }
    for (uint32_t off = 0; off < kRNPspBufSize; off += 4) vramWrite(kRNPspBufVramOffset + off, 0);
    uint32_t bad = 0;
    for (uint32_t off = 0; off < len; off += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4 && off + k < len; k++) v |= static_cast<uint32_t>(data[off + k]) << (8 * k);
        vramWrite(kRNPspBufVramOffset + off, v);
        if (vramRead(kRNPspBufVramOffset + off) != v) bad++;
    }
    IOLog("RedNavi: PSP %s: %u bytes (version 0x%x) in VRAM +0x%llx, %u mismatching dwords\n", name, len, version, kRNPspBufVramOffset, bad);
    if (bad) return false;

    uint32_t before81 = rd(r81);
    wr(r36, static_cast<uint32_t>((kRNVramMcBase + kRNPspBufVramOffset) >> 20));
    wr(r35, cmd);

    if (cmd == kRNPspBlLoadSos) {
        // amdgpu waits 20 ms, then for C2PMSG_81 (sOS sign of life) to change.
        IOSleep(20);
        for (int i = 0; i < 2000; i++) { if (rd(r81) != before81) break; IOSleep(1); }
        IOLog("RedNavi: PSP %s: C2PMSG_81 = 0x%08x (was 0x%08x), C2PMSG_58 (sOS version) = 0x%08x, C2PMSG_35 = 0x%08x\n", name, rd(r81), before81, rd(r58), rd(r35));
        return rd(r81) != before81;
    }

    IOSleep(5);
    for (int i = 0; i < 2000; i++) { st = rd(r35); if (st == kRNPspBlReady) break; IOSleep(1); }
    IOLog("RedNavi: PSP %s: after command C2PMSG_35 = 0x%08x, C2PMSG_36 = 0x%08x, C2PMSG_81 = 0x%08x\n", name, st, rd(r36), rd(r81));
    return st == kRNPspBlReady;
}

void RedNaviProbe::loadPspChain() {
#ifdef RN_HAVE_PSP_FW
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_psp_load", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: PSP load not requested (boot-arg rednavi_psp_load=1)\n");
        return;
    }
    uint32_t mp0 = 0;
    if (!ipBase(kRNHwIdMp0, &mp0)) { IOLog("RedNavi: PSP: no MP0 base\n"); return; }
    if (((mp0 + kRNMp0C2pmsg81) * 4) + 4 > regs->getLength()) { IOLog("RedNavi: PSP: registers outside BAR\n"); return; }
    if (rd((mp0 + kRNMp0C2pmsg81) * 4) != 0) { IOLog("RedNavi: PSP: sOS already alive, nothing to do\n"); return; }

    uint32_t done = 0;
    for (const auto &c : kRNPspChain) {
        if (!loadComponent(mp0, c.name, c.cmd, c.data, c.size, c.version)) { IOLog("RedNavi: PSP chain stopped at %s\n", c.name); break; }
        done++;
    }
    IOLog("RedNavi: PSP chain: %u of %u components accepted\n", done, (uint32_t)(sizeof(kRNPspChain) / sizeof(kRNPspChain[0])));
    OSNumber *n = OSNumber::withNumber(static_cast<uint64_t>(done), 64);
    if (n) { setProperty("psp-chain-done", n); n->release(); }
    OSNumber *s81 = OSNumber::withNumber(static_cast<uint64_t>(rd((mp0 + kRNMp0C2pmsg81) * 4)), 64);
    if (s81) { setProperty("psp-c2pmsg81-after", s81); s81->release(); }
#else
    IOLog("RedNavi: built without psp_fw.h, PSP load not available\n");
#endif
}

bool RedNaviProbe::createPspRing() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_psp_ring", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: PSP ring not requested (boot-arg rednavi_psp_ring=1)\n");
        return false;
    }
    uint32_t mp0 = 0;
    if (!ipBase(kRNHwIdMp0, &mp0)) { IOLog("RedNavi: PSP ring: no MP0 base\n"); return false; }
    uint32_t r64 = (mp0 + kRNMp0C2pmsg64) * 4, r69 = (mp0 + kRNMp0C2pmsg69) * 4, r70 = (mp0 + kRNMp0C2pmsg70) * 4, r71 = (mp0 + kRNMp0C2pmsg71) * 4;
    uint32_t r81 = (mp0 + kRNMp0C2pmsg81) * 4;
    if (r81 + 4 > regs->getLength()) { IOLog("RedNavi: PSP ring: registers outside BAR\n"); return false; }
    if (rd(r81) == 0) { IOLog("RedNavi: PSP ring: sOS not alive (C2PMSG_81 = 0), skipping\n"); return false; }

    // Wait for the sOS to be ready for ring creation.
    uint32_t st = 0;
    for (int i = 0; i < 2000; i++) { st = rd(r64); if ((st & kRNPspMboxMask) == 0x80000000u) break; IOSleep(1); }
    IOLog("RedNavi: PSP ring: C2PMSG_64 before = 0x%08x\n", st);
    if ((st & kRNPspMboxMask) != 0x80000000u) { IOLog("RedNavi: PSP ring: sOS not ready for ring creation\n"); return false; }

    for (uint32_t off = 0; off < kRNPspRingSize; off += 4) vramWrite(kRNPspRingVramOffset + off, 0);
    uint64_t mc = kRNVramMcBase + kRNPspRingVramOffset;
    wr(r69, static_cast<uint32_t>(mc));
    wr(r70, static_cast<uint32_t>(mc >> 32));
    wr(r71, kRNPspRingSize);
    wr(r64, kRNPspRingTypeKm << 16);
    IOSleep(20);
    for (int i = 0; i < 2000; i++) { st = rd(r64); if ((st & kRNPspMboxMask) == 0x80000000u) break; IOSleep(1); }
    IOLog("RedNavi: PSP ring: C2PMSG_64 after = 0x%08x (0x80000000 = created), ring at MC 0x%llx\n", st, mc);
    OSNumber *n = OSNumber::withNumber(static_cast<uint64_t>(st), 64);
    if (n) { setProperty("psp-ring-status", n); n->release(); }
    return (st & kRNPspMboxMask) == 0x80000000u;
}

// Submit one command through the kernel ring and wait for its fence (psp_ring_cmd_submit and
// psp_cmd_submit_buf in amdgpu_psp.c). args go to psp_gfx_cmd_resp.cmd (+28); nresp dwords of
// psp_gfx_cmd_resp.resp (+864) are returned.
bool RedNaviProbe::pspSubmit(uint32_t cmdId, const uint32_t *args, uint32_t nargs, uint32_t *resp, uint32_t nresp) {
    uint32_t mp0 = 0;
    if (!ipBase(kRNHwIdMp0, &mp0)) return false;
    uint32_t r67 = (mp0 + kRNMp0C2pmsg67) * 4;

    for (uint32_t off = 0; off < 0x1000; off += 4) {
        vramWrite(kRNPspCmdVramOffset + off, 0);
        vramWrite(kRNPspFenceVramOffset + off, 0);
    }
    vramWrite(kRNPspCmdVramOffset + 8, cmdId);               // psp_gfx_cmd_resp.cmd_id
    for (uint32_t i = 0; i < nargs; i++) vramWrite(kRNPspCmdVramOffset + 28 + i * 4, args[i]);

    uint32_t wptr = rd(r67);                                  // in dwords; a frame is 16 dwords
    uint32_t ringDw = kRNPspRingSize / 4;
    if (wptr % 16 != 0 || wptr >= ringDw) { IOLog("RedNavi: PSP cmd: odd write pointer 0x%x, not sending\n", wptr); return false; }
    uint64_t frame = kRNPspRingVramOffset + static_cast<uint64_t>(wptr) * 4;
    uint64_t cmdMc = kRNVramMcBase + kRNPspCmdVramOffset, fenceMc = kRNVramMcBase + kRNPspFenceVramOffset;
    uint32_t index = ++pspFence;
    for (uint32_t off = 0; off < 64; off += 4) vramWrite(frame + off, 0);
    vramWrite(frame + 0, static_cast<uint32_t>(cmdMc));
    vramWrite(frame + 4, static_cast<uint32_t>(cmdMc >> 32));
    vramWrite(frame + 12, static_cast<uint32_t>(fenceMc));
    vramWrite(frame + 16, static_cast<uint32_t>(fenceMc >> 32));
    vramWrite(frame + 20, index);
    wr(r67, (wptr + 16) % ringDw);

    uint32_t fence = 0;
    for (int i = 0; i < 2000; i++) { fence = vramRead(kRNPspFenceVramOffset); if (fence == index) break; IOSleep(1); }
    for (uint32_t i = 0; i < nresp; i++) resp[i] = vramRead(kRNPspCmdVramOffset + 864 + i * 4);
    if (fence != index) IOLog("RedNavi: PSP cmd 0x%x: fence %u, expected %u\n", cmdId, fence, index);
    return fence == index;
}

void RedNaviProbe::sendPspBootCfgGet() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_psp_cmd", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: PSP command not requested (boot-arg rednavi_psp_cmd=1)\n");
        return;
    }
    // cmd.boot_cfg: timestamp, sub_cmd, boot_config, boot_config_valid
    uint32_t args[4] = {0, kRNPspBootCfgGet, 0, 0};
    uint32_t resp[17] = {};                                   // resp.status ... uresp.boot_cfg at dword 16
    bool ok = pspSubmit(kRNPspCmdBootCfg, args, 4, resp, 17);
    IOLog("RedNavi: PSP cmd BOOT_CFG_GET: fence %s, status = 0x%08x, boot_cfg = 0x%08x\n", ok ? "ok" : "missing", resp[0], resp[16]);
    const char *names[3] = {"fence_ok", "status", "boot_cfg"};
    uint32_t vals[3] = {ok, resp[0], resp[16]};
    rnPublish(this, "psp-cmd-boot-cfg", names, vals, 3);
}

void RedNaviProbe::sendPspSetupTmr() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_psp_tmr", &arg, sizeof(arg)) || (arg != 1 && arg != 2)) {
        IOLog("RedNavi: PSP SETUP_TMR not requested (boot-arg rednavi_psp_tmr=1, or 2 for the top of VRAM as Linux does)\n");
        return;
    }
    if (!pspTmrSize) { IOLog("RedNavi: PSP SETUP_TMR: no TMR size from LOAD_TOC\n"); return; }
    IODeviceMemory *bar0 = pci->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0 + kRNVramBar * 4);
    if (!bar0) { IOLog("RedNavi: PSP SETUP_TMR: no BAR0\n"); return; }
    uint64_t tmrOff = arg == 2 ? 0x3e0000000ull : kRNPspTmrVramOffset;       // 2: where the Linux reference run put it
    uint64_t mc = kRNVramMcBase + tmrOff;                                      // GPU address
    uint64_t pa = static_cast<uint64_t>(bar0->getPhysicalAddress()) + tmrOff;  // CPU physical address of the same bytes (amdgpu_gmc_vram_pa)
    // cmd.cmd_setup_tmr: buf_phy_addr_lo/hi, buf_size, flags (bit 1 = virt_phy_addr), system_phy_addr_lo/hi
    uint32_t args[6] = {static_cast<uint32_t>(mc), static_cast<uint32_t>(mc >> 32), pspTmrSize, 2u,
                        static_cast<uint32_t>(pa), static_cast<uint32_t>(pa >> 32)};
    uint32_t resp[1] = {};
    bool ok = pspSubmit(kRNPspCmdSetupTmr, args, 6, resp, 1);
    IOLog("RedNavi: PSP cmd SETUP_TMR (0x%x bytes at MC 0x%llx, PA 0x%llx): fence %s, status = 0x%08x\n", pspTmrSize, mc, pa, ok ? "ok" : "missing", resp[0]);
    const char *names[3] = {"fence_ok", "status", "pa_lo"};
    uint32_t vals[3] = {ok, resp[0], static_cast<uint32_t>(pa)};
    rnPublish(this, "psp-cmd-setup-tmr", names, vals, 3);
}

void RedNaviProbe::sendPspLoadToc() {
#ifdef RN_HAVE_PSP_FW
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_psp_toc", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: PSP LOAD_TOC not requested (boot-arg rednavi_psp_toc=1)\n");
        return;
    }
    // psp_copy_fw: zero the staging buffer, copy the TOC, then point the command at it.
    uint32_t len = sizeof(kRNPspToc);
    for (uint32_t off = 0; off < kRNPspBufSize; off += 4) vramWrite(kRNPspBufVramOffset + off, 0);
    for (uint32_t off = 0; off < len; off += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4 && off + k < len; k++) v |= static_cast<uint32_t>(kRNPspToc[off + k]) << (8 * k);
        vramWrite(kRNPspBufVramOffset + off, v);
    }
    uint64_t mc = kRNVramMcBase + kRNPspBufVramOffset;
    // cmd.cmd_load_toc: toc_phy_addr_lo, toc_phy_addr_hi, toc_size
    uint32_t args[3] = {static_cast<uint32_t>(mc), static_cast<uint32_t>(mc >> 32), len};
    uint32_t resp[5] = {};                                    // status, session_id, fw_addr_lo, fw_addr_hi, tmr_size
    bool ok = pspSubmit(kRNPspCmdLoadToc, args, 3, resp, 5);
    IOLog("RedNavi: PSP cmd LOAD_TOC (%u bytes): fence %s, status = 0x%08x, tmr_size = 0x%08x\n", len, ok ? "ok" : "missing", resp[0], resp[4]);
    const char *names[3] = {"fence_ok", "status", "tmr_size"};
    uint32_t vals[3] = {ok, resp[0], resp[4]};
    rnPublish(this, "psp-cmd-load-toc", names, vals, 3);
    if (ok && resp[0] == 0) pspTmrSize = resp[4];
#else
    IOLog("RedNavi: built without psp_fw.h, LOAD_TOC not available\n");
#endif
}

// Dump the DMCUB window, security and control registers into one property (all at DMU base index 2).
void RedNaviProbe::dmcubSnapshot(const char *property) {
    uint32_t dmu = 0;
    if (!ipBase(kRNHwIdDmu, &dmu, 2)) return;
    if ((dmu + 0x21a) * 4 + 4 > regs->getLength()) return;
    OSDictionary *out = OSDictionary::withCapacity(64);
    if (!out) return;
    auto put = [&](const char *name, uint32_t dword) {
        OSNumber *n = OSNumber::withNumber(static_cast<uint64_t>(rd((dmu + dword) * 4)), 64);
        if (n) { out->setObject(name, n); n->release(); }
    };
    static const char *const base[8] = {"CW0_BASE", "CW1_BASE", "CW2_BASE", "CW3_BASE", "CW4_BASE", "CW5_BASE", "CW6_BASE", "CW7_BASE"};
    static const char *const top[8] = {"CW0_TOP", "CW1_TOP", "CW2_TOP", "CW3_TOP", "CW4_TOP", "CW5_TOP", "CW6_TOP", "CW7_TOP"};
    static const char *const off[8] = {"CW0_OFFSET", "CW1_OFFSET", "CW2_OFFSET", "CW3_OFFSET", "CW4_OFFSET", "CW5_OFFSET", "CW6_OFFSET", "CW7_OFFSET"};
    static const char *const offHi[8] = {"CW0_OFFSET_HI", "CW1_OFFSET_HI", "CW2_OFFSET_HI", "CW3_OFFSET_HI", "CW4_OFFSET_HI", "CW5_OFFSET_HI", "CW6_OFFSET_HI", "CW7_OFFSET_HI"};
    for (uint32_t n = 0; n < 8; n++) {
        put(base[n], 0x1a5 + n);
        put(top[n], 0x1ad + n);
        put(off[n], 0x1b5 + 2 * n);
        put(offHi[n], 0x1b6 + 2 * n);
    }
    put("REGION5_OFFSET", 0x198); put("REGION5_OFFSET_HI", 0x199); put("REGION5_TOP", 0x1a2);
    put("TMR_AXI_SPACE", 0x21a); put("SEC_CNTL", 0x1ce); put("MEM_CNTL", 0x1cf);
    put("CNTL", 0x1f6); put("CNTL2", 0x200); put("SCRATCH0", 0x1e3); put("SCRATCH1", 0x1e4);
    put("FAULT_ADDR", 0x1fa); put("TIMER", 0x1fd);
    setProperty(property, out);
    out->release();
}

void RedNaviProbe::loadDmcub() {
#ifdef RN_HAVE_DMUB_FW
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_dmub_load", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: DMUB load not requested (boot-arg rednavi_dmub_load=1)\n");
        return;
    }
    uint32_t mode = 0;
    PE_parse_boot_argn("rednavi_dmub_mode", &mode, sizeof(mode));
    uint32_t viaPsp = 0;
    PE_parse_boot_argn("rednavi_dmub_psp", &viaPsp, sizeof(viaPsp));
    if (viaPsp) {
        // psp_load_non_psp_fw: stage the signed image in the 1 MB buffer and ask the sOS to load it.
        for (uint32_t off = 0; off < kRNPspBufSize; off += 4) vramWrite(kRNPspBufVramOffset + off, 0);
        for (uint32_t off = 0; off < kRNDmubSignedSize; off += 4) {
            uint32_t v = 0;
            for (uint32_t k = 0; k < 4 && off + k < kRNDmubSignedSize; k++) v |= static_cast<uint32_t>(kRNDmubSigned[off + k]) << (8 * k);
            vramWrite(kRNPspBufVramOffset + off, v);
        }
        uint64_t mc = kRNVramMcBase + kRNPspBufVramOffset;
        uint32_t args[4] = {static_cast<uint32_t>(mc), static_cast<uint32_t>(mc >> 32), kRNDmubSignedSize, kRNPspFwTypeDmub};
        uint32_t resp[4] = {};
        bool ok = pspSubmit(kRNPspCmdLoadIpFw, args, 4, resp, 4);
        IOLog("RedNavi: PSP LOAD_IP_FW DMUB (%u bytes): fence %s, status = 0x%08x, fw_addr = 0x%08x%08x\n", kRNDmubSignedSize, ok ? "ok" : "missing", resp[0], resp[3], resp[2]);
        const char *pn[4] = {"fence_ok", "status", "fw_addr_lo", "fw_addr_hi"};
        uint32_t pv[4] = {ok, resp[0], resp[2], resp[3]};
        rnPublish(this, "psp-load-dmub", pn, pv, 4);
        dmcubSnapshot("dmcub-snap-after-psp");
        if (!ok || resp[0] != 0) return;
    }
    uint32_t dmu = 0;
    if (!ipBase(kRNHwIdDmu, &dmu, 2)) { IOLog("RedNavi: DMUB: no DMU base 2\n"); return; }
    if (!vbiosCopy || vbiosCopy->getLength() != kRNDmubVbiosSize) { IOLog("RedNavi: DMUB: VBIOS size does not match the firmware header\n"); return; }
    uint32_t regLen = static_cast<uint32_t>(regs->getLength());
    if ((dmu + 0x21a) * 4 + 4 > regLen) { IOLog("RedNavi: DMUB: registers outside BAR\n"); return; }

    auto R = [&](uint32_t dword) { return rd((dmu + dword) * 4); };
    auto W = [&](uint32_t dword, uint32_t v) { wr((dmu + dword) * 4, v); };
    auto U = [&](uint32_t dword, uint32_t mask, uint32_t v) { W(dword, (R(dword) & ~mask) | (v & mask)); };
    // dword offsets from dcn_3_2_0_offset.h, all at DMU base index 2
    const uint32_t CNTL = 0x1f6, CNTL2 = 0x200, SEC = 0x1ce, S0 = 0x1e3, S7 = 0x1ea, S14 = 0x1f1, S15 = 0x1f2;
    const uint32_t GPINT1 = 0x1f8, RST = 0x342;
    const uint32_t IN_BASE = 0x1d4, IN_SIZE = 0x1d5, IN_WP = 0x1d6, IN_RP = 0x1d7;
    const uint32_t O0_BASE = 0x1d8, O0_SIZE = 0x1d9, O0_WP = 0x1da, O0_RP = 0x1db;
    const uint32_t O1_BASE = 0x1dc, O1_SIZE = 0x1dd, O1_WP = 0x1de, O1_RP = 0x1df;
    auto CWOFF = [](uint32_t n) { return 0x1b5 + 2 * n; };   // REGIONx_CWn_OFFSET (+1 is _HIGH)
    auto CWBASE = [](uint32_t n) { return 0x1a5 + n; };
    auto CWTOP = [](uint32_t n) { return 0x1ad + n; };
    const uint32_t EN = 0x80000000u, ADDR_MASK = 0x1FFFFFFFu;

    IOLog("RedNavi: DMUB: before: CNTL=0x%08x CNTL2=0x%08x SEC=0x%08x SCRATCH0=0x%08x\n", R(CNTL), R(CNTL2), R(SEC), R(S0));

    uint32_t minimal = 0;                       // rednavi_dmub_min=1: after the PSP load, only release the reset
    PE_parse_boot_argn("rednavi_dmub_min", &minimal, sizeof(minimal));
    if (viaPsp && minimal) {
        U(RST, 1u << 8, 0);
        U(CNTL, (1u << 16) | (1u << 19), (1u << 16) | (1u << 19));
        U(CNTL2, 1, 0);
        uint32_t st = 0, ms = 0;
        for (; ms < 3000; ms++) { st = R(S0); if ((st & 3) == 3) break; IOSleep(1); }
        IOLog("RedNavi: DMUB minimal release: after %u ms SCRATCH0=0x%08x\n", ms, st);
        dmcubSnapshot("dmcub-snap-end");
        return;
    }

    // dmub_dcn32_reset: ask the running firmware to stop, then hold the DMCUB in reset.
    bool enabled = (R(CNTL) >> 16) & 1, inReset = R(CNTL2) & 1;
    if (enabled && !inReset) {
        W(GPINT1, kRNDmubGpintStopFw);
        uint32_t i = 0;
        for (; i < 1000; i++) { if (R(S7) == kRNDmubStopResponse) break; IOSleep(1); }
        IOLog("RedNavi: DMUB: STOP_FW response %s after %u ms (SCRATCH7=0x%08x)\n", R(S7) == kRNDmubStopResponse ? "received" : "NOT received", i, R(S7));
        for (; i < 1000; i++) { if ((R(CNTL) >> 20) & 1) break; IOSleep(1); }
    }
    if (enabled) {
        U(CNTL2, 1, 1);
        IODelay(1);
        U(CNTL, 1u << 16, 0);
    }
    RN_STEP("RedNavi: DMUB step: clearing windows CW2-CW7\n");
    for (uint32_t n = 2; n <= 7; n++) U(CWTOP(n), EN, 0);
    RN_STEP("RedNavi: DMUB step: clearing ring pointers\n");
    W(IN_RP, 0); W(IN_WP, 0); W(O1_RP, 0); W(O1_WP, 0); W(O0_RP, 0); W(O0_WP, 0);
    W(S0, 0);
    W(GPINT1, 0);
    RN_STEP("RedNavi: DMUB step: building the block in VRAM (zeroing %u bytes)\n", kRNDmubBlockSize);

    // Build the memory block (the windows of tools/dmub_layout.py) in VRAM.
    const uint64_t blockOff = kRNDmubBlockVramOff;
    for (uint32_t off = 0; off < kRNDmubBlockSize; off += 4) vramWrite(blockOff + off, 0);
    for (uint32_t off = 0; off < kRNDmubInstSize; off += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4 && off + k < kRNDmubInstSize; k++) v |= static_cast<uint32_t>(kRNDmubInst[off + k]) << (8 * k);
        vramWrite(blockOff + kRNDmubWin_INST_CONST_Off + off, v);
    }
    RN_STEP("RedNavi: DMUB step: zeroed, copying code (%u bytes)\n", kRNDmubInstSize);
    const uint8_t *vb = static_cast<const uint8_t *>(vbiosCopy->getBytesNoCopy());
    for (uint32_t off = 0; off < kRNDmubVbiosSize; off += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4 && off + k < kRNDmubVbiosSize; k++) v |= static_cast<uint32_t>(vb[off + k]) << (8 * k);
        vramWrite(blockOff + kRNDmubWin_VBIOS_Off + off, v);
    }
    RN_STEP("RedNavi: DMUB step: copied VBIOS, verifying code\n");
    uint32_t bad = 0;
    for (uint32_t off = 0; off < kRNDmubInstSize; off += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4 && off + k < kRNDmubInstSize; k++) v |= static_cast<uint32_t>(kRNDmubInst[off + k]) << (8 * k);
        if (vramRead(blockOff + kRNDmubWin_INST_CONST_Off + off) != v) bad++;
    }
    IOLog("RedNavi: DMUB: block of %u bytes built at VRAM +0x%llx, %u mismatching code dwords\n", kRNDmubBlockSize, blockOff, bad);
    if (bad) return;

    auto addr = [&](uint32_t winOff) -> uint64_t { return mode ? blockOff + winOff : kRNVramMcBase + blockOff + winOff; };
    auto window = [&](uint32_t n, uint64_t offset, uint32_t base, uint32_t top) {
        W(CWOFF(n), static_cast<uint32_t>(offset));
        W(CWOFF(n) + 1, static_cast<uint32_t>(offset >> 32));
        W(CWBASE(n), base & ADDR_MASK);
        W(CWTOP(n), (top & ADDR_MASK) | EN);
    };

    // dmub_dcn32_backdoor_load: code and stack windows, addressed as VRAM offsets (fb_base removed).
    if (!viaPsp) {
    U(SEC, 1u << 16, 1u << 16);                 // DMCUB_SEC_RESET = 1
    U(CNTL, 1u << 16, 0);
    window(0, blockOff + kRNDmubWin_INST_CONST_Off, kRNDmubCw0Base, kRNDmubCw0Base + kRNDmubWin_INST_CONST_Size - 1);
    window(1, blockOff + kRNDmubWin_STACK_Off, kRNDmubCw1Base, kRNDmubCw1Base + kRNDmubWin_STACK_Size - 1);
    U(SEC, (1u << 16) | (0xFFu << 8), 0x20u << 8);   // DMCUB_SEC_RESET = 0, DMCUB_MEM_UNIT_ID = 0x20
    }

    RN_STEP("RedNavi: DMUB step: block verified, programming windows\n");
    // dmub_dcn32_enable_dmub_boot_options and setup_windows: VBIOS, mailbox, trace, firmware state.
    W(S14, 0);
    window(3, addr(kRNDmubWin_VBIOS_Off), kRNDmubCw3Base, kRNDmubCw3Base + kRNDmubWin_VBIOS_Size);
    window(4, addr(kRNDmubWin_MAILBOX_Off), kRNDmubCw4Base, kRNDmubCw4Base + kRNDmubWin_MAILBOX_Size);
    window(5, addr(kRNDmubWin_TRACEBUFF_Off), kRNDmubCw5Base, kRNDmubCw5Base + kRNDmubWin_TRACEBUFF_Size);
    uint64_t t = addr(kRNDmubWin_TRACEBUFF_Off);   // REGION5 aliases the trace buffer
    W(0x198, static_cast<uint32_t>(t));
    W(0x199, static_cast<uint32_t>(t >> 32));
    W(0x1a2, ((kRNDmubWin_TRACEBUFF_Size - 1) & ADDR_MASK) | EN);
    window(6, addr(kRNDmubWin_FW_STATE_Off), kRNDmubCw6Base, kRNDmubCw6Base + kRNDmubWin_FW_STATE_Size);
    W(O0_BASE, kRNDmubRegion5Base + kRNDmubTraceEntryOff);
    W(O0_SIZE, kRNDmubWin_TRACEBUFF_Size - kRNDmubTraceEntryOff);
    W(IN_BASE, kRNDmubCw4Base);
    W(IN_SIZE, kRNDmubRbSize);
    W(O1_BASE, kRNDmubCw4Base + kRNDmubRbSize);
    W(O1_SIZE, kRNDmubRbSize);

    RN_STEP("RedNavi: DMUB step: windows set, releasing reset\n");
    // dmub_dcn32_reset_release
    U(RST, 1u << 8, 0);                         // DMUIF_SOFT_RESET = 0
    W(S15, 0);
    U(CNTL, (1u << 16) | (1u << 19), (1u << 16) | (1u << 19));   // DMCUB_ENABLE, DMCUB_TRACEPORT_EN
    U(CNTL2, 1, 0);                             // DMCUB_SOFT_RESET = 0

    uint32_t st = 0, ms = 0;
    for (; ms < 3000; ms++) { st = R(S0); if ((st & 3) == 3) break; IOSleep(1); }
    IOLog("RedNavi: DMUB: after %u ms SCRATCH0=0x%08x (bit0 dal_fw, bit1 mailbox_rdy) CNTL=0x%08x CNTL2=0x%08x SEC=0x%08x\n", ms, st, R(CNTL), R(CNTL2), R(SEC));
    if ((st & 3) == 3) {
        dmubUp = true; dmubDmu = dmu; dmubMailbox = blockOff + kRNDmubWin_MAILBOX_Off;
        uint32_t cmdArg = 0;
        PE_parse_boot_argn("rednavi_dmub_cmd", &cmdArg, sizeof(cmdArg));
        if (cmdArg == 1) {
            // dmub_srv_fb_cmd_queue + dmub_srv_fb_cmd_execute: copy the command into the ring at the
            // write pointer, then move the write pointer; the firmware advances the read pointer.
            uint32_t wp = R(IN_WP), slot = wp % kRNDmubRbSize;
            uint64_t slotAddr = blockOff + kRNDmubWin_MAILBOX_Off + slot;
            for (uint32_t i = 0; i < 64; i += 4) vramWrite(slotAddr + i, 0);
            vramWrite(slotAddr, 6u | (14u << 24));    // header: type QUERY_FEATURE_CAPS (6), payload 14 bytes
            uint32_t newWp = (wp + 64) % kRNDmubRbSize;
            W(IN_WP, newWp);
            uint32_t rp = 0, waited = 0;
            for (; waited < 1000; waited++) { rp = R(IN_RP); if (rp == newWp) break; IOSleep(1); }
            uint32_t reply[5];
            for (uint32_t i = 0; i < 5; i++) reply[i] = vramRead(slotAddr + i * 4);
            IOLog("RedNavi: DMUB cmd QUERY_FEATURE_CAPS: wptr 0x%x -> 0x%x, rptr 0x%x after %u ms, slot %08x %08x %08x %08x %08x\n", wp, newWp, rp, waited, reply[0], reply[1], reply[2], reply[3], reply[4]);
            const char *cn[9] = {"rptr_reached", "wptr", "rptr", "slot0", "slot1", "slot2", "slot3", "slot4", "outbox1_wptr"};
            uint32_t cv[9] = {rp == newWp, newWp, rp, reply[0], reply[1], reply[2], reply[3], reply[4], R(O1_WP)};
            rnPublish(this, "dmub-cmd", cn, cv, 9);
        }
    }
    const char *names[12] = {"scratch0", "scratch1", "scratch2", "cntl", "cntl2", "inbox_wptr", "inbox_rptr", "mode", "sec_cntl", "fault_addr", "timer", "scratch14"};
    uint32_t vals[12] = {st, R(0x1e4), R(0x1e5), R(CNTL), R(CNTL2), R(IN_WP), R(IN_RP), mode, R(SEC), R(0x1fa), R(0x1fd), R(S14)};
    rnPublish(this, "dmub-load", names, vals, 12);
    dmcubSnapshot("dmcub-snap-end");
#else
    IOLog("RedNavi: built without dmub_fw.h, DMUB load not available\n");
#endif
}

bool RedNaviProbe::start(IOService *provider) {
    if (!IOService::start(provider)) return false;
    pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) return false;

    pci->setMemoryEnable(true);
    dumpPci();

    regs = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0 + kRNRegBar * 4);
    if (!regs) {
        IOLog("RedNavi: could not map register BAR%u\n", kRNRegBar);
        return false;
    }
    IOLog("RedNavi: registers mapped, %llu KB\n", (uint64_t)regs->getLength() >> 10);

    // System Information reads the GPU name and VRAM size from these properties of the PCI node.
    {
        static const char kName[] = "AMD Radeon RX 7800 XT";
        uint32_t vramMb = 16384;
        OSData *m = OSData::withBytes(kName, sizeof(kName));
        OSData *v = OSData::withBytes(&vramMb, sizeof(vramMb));
        if (m) { pci->setProperty("model", m); m->release(); }
        if (v) { pci->setProperty("VRAM,totalMB", v); v->release(); }
    }

    dumpDiscovery();
    dumpVbios();
    dumpRegs(kRNMailboxRegs, sizeof(kRNMailboxRegs) / sizeof(kRNMailboxRegs[0]), "mailbox-regs");
    dumpRegs(kRNMmhubRegs, sizeof(kRNMmhubRegs) / sizeof(kRNMmhubRegs[0]), "mmhub-regs");
    dumpRegs(kRNDmcubRegs, sizeof(kRNDmcubRegs) / sizeof(kRNDmcubRegs[0]), "dmcub-regs");
    dumpRegs(kRNHpdRegs, sizeof(kRNHpdRegs) / sizeof(kRNHpdRegs[0]), "hpd-regs");
    smuProbe("smu-info-early", false);   // registers only, before any stage that writes to the card
    dmcubSnapshot("dmcub-snap-boot");
    loadPspChain();
    if (createPspRing()) {
        sendPspBootCfgGet();
        sendPspLoadToc();
        sendPspSetupTmr();
    }
    loadDmcub();
    dpInit();
    probeAux();
    selectMode();
    dpTrain();
    dcnReplay();
    dcnDump();
    smuProbe("smu-info", true);          // messages, once the PSP and the display are up
    publishFramebuffer();
    registerService();
    return true;
}

void RedNaviProbe::stop(IOService *provider) {
    if (disc) { disc->release(); disc = nullptr; }
    if (vbiosCopy) { vbiosCopy->release(); vbiosCopy = nullptr; }
    if (regs) { regs->release(); regs = nullptr; }
    IOService::stop(provider);
}
