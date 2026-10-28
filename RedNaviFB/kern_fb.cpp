// RedNaviFB: IOFramebuffer on top of the DCN scanout the probe brings up.
//
// The display hardware is already lit by RedNaviProbe::dcnReplay() (fixed 1920x1080 at 60 Hz, linear XRGB8888,
// pitch 7680). This class only tells WindowServer where that surface is: an aperture over BAR0, which macOS
// maps as 256 MB, so the surface lives in the first 256 MB of VRAM. There is no mode setting, no cursor, no
// gamma and no vertical blank interrupt yet.
//
// This is a separate kext because IOGraphicsFamily sits in the system kernel collection and OpenCore can only
// link injected kexts against the boot collection. It matches the property RedNaviProbe sets after the replay
// (rednavi-fb-ready, only with the boot-arg rednavi_fb=1) and reads the surface location from the same node.

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOFramebuffer.h>

#define kRNFbWidth    1920u
#define kRNFbHeight   1080u
#define kRNFbPitch    7680u
#define kRNFbModeId   1u

class RedNaviFB : public IOFramebuffer {
    OSDeclareDefaultStructors(RedNaviFB)

    uint64_t scanoutPhys {0};          // CPU physical address of the surface (BAR0 + offset)

    // Diagnostics, published as the fb-diag property: what WindowServer asks and whether the surface changes.
    IOTimerEventSource *diagTimer {nullptr};
    IOMemoryMap *diagMap {nullptr};
    uint32_t calls[8] {};              // 0 aperture, 1 setAttr, 2 getAttr, 3 setConnAttr, 4 getConnAttr, 5 setMode, 6 cursor, 7 other
    uint32_t attrLog[16] {};           // ring of "kind << 28 | low 28 bits of selector"; selectors are fourcc, see attrRaw
    uint32_t attrRaw[16] {};
    uint32_t attrPos {0};

    void note(unsigned kind, uint32_t sel) {
        calls[kind]++;
        attrLog[attrPos & 15] = kind;
        attrRaw[attrPos & 15] = sel;
        attrPos++;
        publishDiag();
    }

    void publishDiag() {
        uint32_t px[16] = {};
        uint32_t sum = 0;
        if (diagMap) {
            volatile uint32_t *fb = reinterpret_cast<volatile uint32_t *>(diagMap->getVirtualAddress());
            for (unsigned i = 0; i < 16; i++) {
                unsigned y = 8 + i * 64, x = 100 + i * 100;
                px[i] = fb[y * (kRNFbPitch / 4) + x];
                sum = sum * 31 + px[i];
            }
        }
        OSDictionary *d = OSDictionary::withCapacity(8);
        if (!d) return;
        OSData *c = OSData::withBytes(calls, sizeof(calls));
        OSData *a = OSData::withBytes(attrRaw, sizeof(attrRaw));
        OSData *k = OSData::withBytes(attrLog, sizeof(attrLog));
        OSData *p = OSData::withBytes(px, sizeof(px));
        OSNumber *h = OSNumber::withNumber(sum, 32);
        OSNumber *n = OSNumber::withNumber(attrPos, 32);
        if (c && a && k && p && h && n) {
            d->setObject("calls", c); d->setObject("attr-sel", a); d->setObject("attr-kind", k);
            d->setObject("pixels", p); d->setObject("hash", h); d->setObject("count", n);
            setProperty("fb-diag", d);
        }
        if (c) c->release();
        if (a) a->release();
        if (k) k->release();
        if (p) p->release();
        if (h) h->release();
        if (n) n->release();
        d->release();
    }

    static bool readNumber(IOService *svc, const char *key, uint64_t *out) {
        OSNumber *n = OSDynamicCast(OSNumber, svc->getProperty(key));
        if (!n) return false;
        *out = n->unsigned64BitValue();
        return true;
    }

public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE {
        if (!readNumber(provider, "fb-scanout-phys", &scanoutPhys)) {
            IOLog("RedNaviFB: provider has no fb-scanout-phys\n");
            return false;
        }
        if (!IOFramebuffer::start(provider)) return false;
        IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(scanoutPhys, kRNFbPitch * kRNFbHeight, kIODirectionInOut);
        if (md) { diagMap = md->map(kIOMapInhibitCache); md->release(); }
        return true;
    }

    void stop(IOService *provider) APPLE_KEXT_OVERRIDE {
        if (diagTimer) { diagTimer->cancelTimeout(); getWorkLoop()->removeEventSource(diagTimer); diagTimer->release(); diagTimer = nullptr; }
        if (diagMap) { diagMap->release(); diagMap = nullptr; }
        IOFramebuffer::stop(provider);
    }

    IOReturn setAttribute(IOSelect attr, uintptr_t value) APPLE_KEXT_OVERRIDE {
        note(1, attr);
        return IOFramebuffer::setAttribute(attr, value);
    }
    IOReturn getAttribute(IOSelect attr, uintptr_t *value) APPLE_KEXT_OVERRIDE {
        note(2, attr);
        return IOFramebuffer::getAttribute(attr, value);
    }
    IOReturn setAttributeForConnection(IOIndex c, IOSelect attr, uintptr_t value) APPLE_KEXT_OVERRIDE {
        note(3, attr);
        return IOFramebuffer::setAttributeForConnection(c, attr, value);
    }
    IOReturn getAttributeForConnection(IOIndex c, IOSelect attr, uintptr_t *value) APPLE_KEXT_OVERRIDE {
        note(4, attr);
        // IODisplay::readFramebufferEDID passes a null value pointer to ask only whether the attribute is supported.
        uintptr_t scratch;
        if (!value) value = &scratch;
        switch (attr) {
        case kConnectionFlags:             *value = 0; return kIOReturnSuccess;
        case kConnectionEnable:            *value = 1; return kIOReturnSuccess;
        case kConnectionSupportsHLDDCSense:
        case kConnectionSupportsLLDDCSense: *value = 1; return kIOReturnSuccess;
        case kConnectionSupportsAppleSense: return kIOReturnUnsupported;
        default: return IOFramebuffer::getAttributeForConnection(c, attr, value);
        }
    }

    // The EDID the probe read through AUX is a property of the probe node ("edid", 128-byte blocks).
    bool hasDDCConnect(IOIndex) APPLE_KEXT_OVERRIDE {
        OSData *e = OSDynamicCast(OSData, getProvider() ? getProvider()->getProperty("edid") : nullptr);
        return e && e->getLength() >= 128;
    }

    IOReturn getDDCBlock(IOIndex, UInt32 block, IOSelect type, IOOptionBits, UInt8 *data, IOByteCount *length) APPLE_KEXT_OVERRIDE {
        OSData *e = OSDynamicCast(OSData, getProvider() ? getProvider()->getProperty("edid") : nullptr);
        if (!e || !data || !length || type != kIODDCBlockTypeEDID || block == 0 || e->getLength() < block * 128u) return kIOReturnUnsupported;
        if (*length > 128) *length = 128;
        memcpy(data, static_cast<const uint8_t *>(e->getBytesNoCopy()) + (block - 1) * 128, *length);
        return kIOReturnSuccess;
    }

    IOReturn enableController() APPLE_KEXT_OVERRIDE {
        calls[7]++;
        IOWorkLoop *wl = getWorkLoop();
        if (wl) {
            diagTimer = IOTimerEventSource::timerEventSource(this, [](OSObject *o, IOTimerEventSource *t) {
                RedNaviFB *self = static_cast<RedNaviFB *>(o);
                self->publishDiag();
                t->setTimeoutMS(1000);
            });
            if (diagTimer && wl->addEventSource(diagTimer) == kIOReturnSuccess) diagTimer->setTimeoutMS(1000);
        }
        publishDiag();
        IOLog("RedNaviFB: enabled, %ux%u at physical 0x%llx\n", kRNFbWidth, kRNFbHeight, scanoutPhys);
        return kIOReturnSuccess;
    }

    IODeviceMemory *getApertureRange(IOPixelAperture aperture) APPLE_KEXT_OVERRIDE {
        calls[0]++;
        publishDiag();
        if (aperture != kIOFBSystemAperture) return nullptr;
        IODeviceMemory *m = IODeviceMemory::withRange(scanoutPhys, static_cast<IOPhysicalLength>(kRNFbPitch) * kRNFbHeight);
        if (m) m->setTag(kIOMapWriteCombineCache);    // VRAM through the BAR: write-combined, never plain cached
        return m;
    }

    IODeviceMemory *getVRAMRange() APPLE_KEXT_OVERRIDE {
        return getApertureRange(kIOFBSystemAperture);
    }

    const char *getPixelFormats() APPLE_KEXT_OVERRIDE { return IO32BitDirectPixels; }

    IOItemCount getDisplayModeCount() APPLE_KEXT_OVERRIDE { return 1; }

    IOReturn getDisplayModes(IODisplayModeID *modes) APPLE_KEXT_OVERRIDE {
        modes[0] = kRNFbModeId;
        return kIOReturnSuccess;
    }

    IOReturn getInformationForDisplayMode(IODisplayModeID mode, IODisplayModeInformation *info) APPLE_KEXT_OVERRIDE {
        if (mode != kRNFbModeId) return kIOReturnUnsupportedMode;
        bzero(info, sizeof(*info));
        info->nominalWidth = kRNFbWidth;
        info->nominalHeight = kRNFbHeight;
        info->refreshRate = 60 << 16;
        info->maxDepthIndex = 0;
        info->flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;
        return kIOReturnSuccess;
    }

    UInt64 getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex) APPLE_KEXT_OVERRIDE { return 0; }

    IOReturn getPixelInformation(IODisplayModeID mode, IOIndex depth, IOPixelAperture aperture,
                                 IOPixelInformation *pi) APPLE_KEXT_OVERRIDE {
        if (mode != kRNFbModeId || depth != 0 || aperture != kIOFBSystemAperture) return kIOReturnUnsupportedMode;
        bzero(pi, sizeof(*pi));
        strlcpy(pi->pixelFormat, IO32BitDirectPixels, sizeof(pi->pixelFormat));
        pi->flags = 0;
        pi->pixelType = kIORGBDirectPixels;
        pi->componentCount = 3;
        pi->bitsPerComponent = 8;
        pi->bitsPerPixel = 32;
        pi->bytesPerRow = kRNFbPitch;
        pi->activeWidth = kRNFbWidth;
        pi->activeHeight = kRNFbHeight;
        pi->componentMasks[0] = 0x00ff0000;
        pi->componentMasks[1] = 0x0000ff00;
        pi->componentMasks[2] = 0x000000ff;
        return kIOReturnSuccess;
    }

    IOReturn getCurrentDisplayMode(IODisplayModeID *mode, IOIndex *depth) APPLE_KEXT_OVERRIDE {
        if (mode) *mode = kRNFbModeId;
        if (depth) *depth = 0;
        return kIOReturnSuccess;
    }

    // The only mode is already on the wire, so accepting it again is all a mode set can mean for now.
    IOReturn setDisplayMode(IODisplayModeID mode, IOIndex depth) APPLE_KEXT_OVERRIDE {
        calls[5]++;
        return (mode == kRNFbModeId && depth == 0) ? kIOReturnSuccess : kIOReturnUnsupportedMode;
    }
};

OSDefineMetaClassAndStructors(RedNaviFB, IOFramebuffer)
