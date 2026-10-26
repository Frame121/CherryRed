// RedNavi probe (milestone M1): read-only. Matches the RX 7800 XT (Navi 32, 1002:747E),
// maps the register BAR, and logs what the GPU tells us about itself: PCI config, BAR sizes
// and the IP discovery table (the authoritative list of IP block versions).
//
// Nothing is written to the GPU except the MM_INDEX/MM_DATA window used to read VRAM.

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

#define kRNRegBar          5          // MMIO registers (512 KB on Navi 3x)
#define kRNVramBar         0          // VRAM aperture (size depends on Resizable BAR)

// SOC15 MMIO window: byte offsets inside the register BAR (dword 0x0, 0x1, 0x6 in amdgpu).
#define kRNMmIndex         0x00
#define kRNMmData          0x04
#define kRNMmIndexHi       0x18

// IP discovery lives in the top 64 KB of VRAM (DISCOVERY_TMR_OFFSET in amdgpu_discovery.c).
#define kRNDiscoveryTmrSize   (64 * 1024)
#define kRNBinarySignature    0x28211407u   // "BINARY_SIGNATURE" in amdgpu_discovery.h

class RedNaviProbe : public IOService {
    OSDeclareDefaultStructors(RedNaviProbe)

    IOPCIDevice *pci {nullptr};
    IOMemoryMap *regs {nullptr};

    uint32_t rd(uint32_t off) { return *reinterpret_cast<volatile uint32_t *>(regs->getVirtualAddress() + off); }
    void wr(uint32_t off, uint32_t v) { *reinterpret_cast<volatile uint32_t *>(regs->getVirtualAddress() + off) = v; }

    // Read one dword of VRAM through the index/data pair (works regardless of BAR0 size).
    uint32_t vramRead(uint64_t pos) {
        wr(kRNMmIndex, static_cast<uint32_t>(pos) | 0x80000000u);
        wr(kRNMmIndexHi, static_cast<uint32_t>(pos >> 31));
        uint32_t v = rd(kRNMmData);
        return v;
    }

    void dumpPci();
    void dumpDiscovery();

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
    // Try the plausible VRAM sizes of Navi 32 boards (16 GB for the 7800 XT; 12 GB for the 7700 XT).
    static const uint64_t sizes[] = {16ull << 30, 12ull << 30, 8ull << 30};
    for (uint64_t vram : sizes) {
        uint64_t base = vram - kRNDiscoveryTmrSize;
        uint32_t sig = vramRead(base);
        IOLog("RedNavi: discovery probe vram=%llu MB base=0x%llx sig=0x%08x\n", vram >> 20, base, sig);
        if (sig != kRNBinarySignature) continue;

        // struct binary_header: u32 signature, u16 ver_major, u16 ver_minor, u16 checksum, u16 size, then table list.
        uint32_t ver = vramRead(base + 4);
        uint32_t csumSize = vramRead(base + 8);
        IOLog("RedNavi: IP discovery found: version %u.%u size %u bytes\n", ver & 0xFFFF, ver >> 16, csumSize >> 16);
        // Dump the first 512 bytes raw so the table can be decoded offline (tools/decode_discovery.py).
        for (uint32_t off = 0; off < 512; off += 16) {
            IOLog("RedNavi: DISC %03x: %08x %08x %08x %08x\n", off,
                  vramRead(base + off), vramRead(base + off + 4), vramRead(base + off + 8), vramRead(base + off + 12));
        }
        return;
    }
    IOLog("RedNavi: IP discovery signature not found (VRAM size guess wrong or index window unavailable)\n");
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
    registerService();
    return true;
}

void RedNaviProbe::stop(IOService *provider) {
    if (regs) { regs->release(); regs = nullptr; }
    IOService::stop(provider);
}
