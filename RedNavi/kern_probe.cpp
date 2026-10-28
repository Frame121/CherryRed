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
// Tenth stage (opt-in, writes to the GPU, rednavi_dmub_load=1 and a build with RedNavi/dmub_fw.h):
// stops the DMCUB the VBIOS left running, builds the DMUB memory block in VRAM, backdoor-loads the
// DAL firmware and releases the reset, mirroring dmub_srv_hw_init and the dmub_dcn32_* hooks in amdgpu.
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

#ifdef RN_HAVE_PSP_FW
#include "psp_fw.h"
#endif
#ifdef RN_HAVE_DMUB_FW
#include "dmub_fw.h"
#endif

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

class RedNaviProbe : public IOService {
    OSDeclareDefaultStructors(RedNaviProbe)

    IOPCIDevice *pci {nullptr};
    IOMemoryMap *regs {nullptr};
    OSData *disc {nullptr};   // copy of the IP discovery table
    OSData *vbiosCopy {nullptr};   // VBIOS image read from the PCI ROM

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
    void dmcubSnapshot(const char *property);
    bool ipBase(uint16_t hwId, uint32_t *base, uint32_t idx = 0);
    bool loadComponent(uint32_t mp0, const char *name, uint32_t cmd, const uint8_t *data, uint32_t len, uint32_t version);
    void loadPspChain();
    bool createPspRing();
    bool pspSubmit(uint32_t cmdId, const uint32_t *args, uint32_t nargs, uint32_t *resp, uint32_t nresp);
    void sendPspBootCfgGet();
    void sendPspLoadToc();
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
    for (uint32_t n = 2; n <= 7; n++) U(CWTOP(n), EN, 0);
    W(IN_RP, 0); W(IN_WP, 0); W(O1_RP, 0); W(O1_WP, 0); W(O0_RP, 0); W(O0_WP, 0);
    W(S0, 0);
    W(GPINT1, 0);

    // Build the memory block (the windows of tools/dmub_layout.py) in VRAM.
    const uint64_t blockOff = kRNDmubBlockVramOff;
    for (uint32_t off = 0; off < kRNDmubBlockSize; off += 4) vramWrite(blockOff + off, 0);
    for (uint32_t off = 0; off < kRNDmubInstSize; off += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4 && off + k < kRNDmubInstSize; k++) v |= static_cast<uint32_t>(kRNDmubInst[off + k]) << (8 * k);
        vramWrite(blockOff + kRNDmubWin_INST_CONST_Off + off, v);
    }
    const uint8_t *vb = static_cast<const uint8_t *>(vbiosCopy->getBytesNoCopy());
    for (uint32_t off = 0; off < kRNDmubVbiosSize; off += 4) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < 4 && off + k < kRNDmubVbiosSize; k++) v |= static_cast<uint32_t>(vb[off + k]) << (8 * k);
        vramWrite(blockOff + kRNDmubWin_VBIOS_Off + off, v);
    }
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

    // dmub_dcn32_reset_release
    U(RST, 1u << 8, 0);                         // DMUIF_SOFT_RESET = 0
    W(S15, 0);
    U(CNTL, (1u << 16) | (1u << 19), (1u << 16) | (1u << 19));   // DMCUB_ENABLE, DMCUB_TRACEPORT_EN
    U(CNTL2, 1, 0);                             // DMCUB_SOFT_RESET = 0

    uint32_t st = 0, ms = 0;
    for (; ms < 3000; ms++) { st = R(S0); if ((st & 3) == 3) break; IOSleep(1); }
    IOLog("RedNavi: DMUB: after %u ms SCRATCH0=0x%08x (bit0 dal_fw, bit1 mailbox_rdy) CNTL=0x%08x CNTL2=0x%08x SEC=0x%08x\n", ms, st, R(CNTL), R(CNTL2), R(SEC));
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

    dumpDiscovery();
    dumpVbios();
    dumpRegs(kRNMailboxRegs, sizeof(kRNMailboxRegs) / sizeof(kRNMailboxRegs[0]), "mailbox-regs");
    dumpRegs(kRNMmhubRegs, sizeof(kRNMmhubRegs) / sizeof(kRNMmhubRegs[0]), "mmhub-regs");
    dumpRegs(kRNDmcubRegs, sizeof(kRNDmcubRegs) / sizeof(kRNDmcubRegs[0]), "dmcub-regs");
    dmcubSnapshot("dmcub-snap-boot");
    loadPspChain();
    if (createPspRing()) {
        sendPspBootCfgGet();
        sendPspLoadToc();
    }
    loadDmcub();
    registerService();
    return true;
}

void RedNaviProbe::stop(IOService *provider) {
    if (disc) { disc->release(); disc = nullptr; }
    if (vbiosCopy) { vbiosCopy->release(); vbiosCopy = nullptr; }
    if (regs) { regs->release(); regs = nullptr; }
    IOService::stop(provider);
}
