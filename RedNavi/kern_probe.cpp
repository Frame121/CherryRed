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
#define kRNHwIdMp0            255

// Mailbox registers: dword offsets from the IP base address, taken from mp_13_0_0_offset.h
// (PSP, used by psp_v13_0.c) and smu_v13_0.c (SMU message, argument and response).
struct RNMailboxReg {
    const char *name;
    uint16_t hwId;
    uint32_t dword;
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

class RedNaviProbe : public IOService {
    OSDeclareDefaultStructors(RedNaviProbe)

    IOPCIDevice *pci {nullptr};
    IOMemoryMap *regs {nullptr};
    OSData *disc {nullptr};   // copy of the IP discovery table

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
    bool ipBase(uint16_t hwId, uint32_t *base);
    bool loadComponent(uint32_t mp0, const char *name, uint32_t cmd, const uint8_t *data, uint32_t len, uint32_t version);
    void loadPspChain();
    void createPspRing();
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
bool RedNaviProbe::ipBase(uint16_t hwId, uint32_t *base) {
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
            if (u16(pos) == hwId && b[pos + 2] == 0 && nbase > 0) {
                *base = u32(pos + 8);
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
        if (!ipBase(r.hwId, &base)) {
            IOLog("RedNavi: %s: hardware ID %u not in discovery table\n", r.name, r.hwId);
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

void RedNaviProbe::createPspRing() {
    uint32_t arg = 0;
    if (!PE_parse_boot_argn("rednavi_psp_ring", &arg, sizeof(arg)) || arg != 1) {
        IOLog("RedNavi: PSP ring not requested (boot-arg rednavi_psp_ring=1)\n");
        return;
    }
    uint32_t mp0 = 0;
    if (!ipBase(kRNHwIdMp0, &mp0)) { IOLog("RedNavi: PSP ring: no MP0 base\n"); return; }
    uint32_t r64 = (mp0 + kRNMp0C2pmsg64) * 4, r69 = (mp0 + kRNMp0C2pmsg69) * 4, r70 = (mp0 + kRNMp0C2pmsg70) * 4, r71 = (mp0 + kRNMp0C2pmsg71) * 4;
    uint32_t r81 = (mp0 + kRNMp0C2pmsg81) * 4;
    if (r81 + 4 > regs->getLength()) { IOLog("RedNavi: PSP ring: registers outside BAR\n"); return; }
    if (rd(r81) == 0) { IOLog("RedNavi: PSP ring: sOS not alive (C2PMSG_81 = 0), skipping\n"); return; }

    // Wait for the sOS to be ready for ring creation.
    uint32_t st = 0;
    for (int i = 0; i < 2000; i++) { st = rd(r64); if ((st & kRNPspMboxMask) == 0x80000000u) break; IOSleep(1); }
    IOLog("RedNavi: PSP ring: C2PMSG_64 before = 0x%08x\n", st);
    if ((st & kRNPspMboxMask) != 0x80000000u) { IOLog("RedNavi: PSP ring: sOS not ready for ring creation\n"); return; }

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
    dumpRegs(kRNMailboxRegs, sizeof(kRNMailboxRegs) / sizeof(kRNMailboxRegs[0]), "mailbox-regs");
    dumpRegs(kRNMmhubRegs, sizeof(kRNMmhubRegs) / sizeof(kRNMmhubRegs[0]), "mmhub-regs");
    loadPspChain();
    createPspRing();
    registerService();
    return true;
}

void RedNaviProbe::stop(IOService *provider) {
    if (disc) { disc->release(); disc = nullptr; }
    if (regs) { regs->release(); regs = nullptr; }
    IOService::stop(provider);
}
