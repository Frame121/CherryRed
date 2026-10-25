//
//  Navi33Survey.cpp — see Navi33Survey.hpp. READ-ONLY. Hardware loading remains unverified.
//
#include "Navi33Survey.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>

#define N33LOG(fmt, ...) IOLog("Navi33Survey: " fmt "\n", ##__VA_ARGS__)

static constexpr uint16_t kAmdVendorId   = 0x1002;
static constexpr uint16_t kNavi33Device  = 0x7480;   // Radeon RX 7600
static constexpr uint32_t kPcieCapId     = 0x10;     // PCI Express capability (standard list)
static constexpr uint32_t kReBarExtCapId = 0x0015;   // Resizable BAR (extended list)

OSDefineMetaClassAndStructors(Navi33Survey, IOService)

static IOPCIDevice *eligible(IOService *provider)
{
	uint32_t enabled = 0;
	if (!PE_parse_boot_argn("navi33survey", &enabled, sizeof(enabled)) || enabled != 1)
		return nullptr;                                  // opt-in only

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci)
		return nullptr;

	if (pci->configRead16(kIOPCIConfigVendorID) != kAmdVendorId ||
	    pci->configRead16(kIOPCIConfigDeviceID) != kNavi33Device ||
	    pci->configRead8(0x0b) != 0x03)
		return nullptr;                                  // never touch another device

	return pci;
}

IOService *Navi33Survey::probe(IOService *provider, SInt32 *score)
{
	return eligible(provider) ? super::probe(provider, score) : nullptr;
}

bool Navi33Survey::start(IOService *provider)
{
	IOPCIDevice *pci = eligible(provider);
	if (!pci || !super::start(provider))
		return false;

	N33LOG("start (read-only survey, no register BAR mapped, no writes)");
	surveyIdentity(pci);
	surveyBars(pci);
	surveyPcieLink(pci);
	surveyResizableBar(pci);
	N33LOG("survey done");

	registerService();
	return true;
}

void Navi33Survey::stop(IOService *provider)
{
	super::stop(provider);
}

void Navi33Survey::surveyIdentity(IOPCIDevice *pci)
{
	uint16_t vendor    = pci->configRead16(kIOPCIConfigVendorID);
	uint16_t device    = pci->configRead16(kIOPCIConfigDeviceID);
	uint8_t  revision  = pci->configRead8(kIOPCIConfigRevisionID);
	uint16_t subVendor = pci->configRead16(kIOPCIConfigSubSystemVendorID);
	uint16_t subDevice = pci->configRead16(kIOPCIConfigSubSystemID);
	N33LOG("id %04x:%04x rev %02x subsystem %04x:%04x",
	       vendor, device, revision, subVendor, subDevice);
}

void Navi33Survey::surveyBars(IOPCIDevice *pci)
{
	// Six BAR registers. The upper half of a 64-bit BAR has no descriptor of its
	// own, so a NULL here is normal and skipped. Nothing is mapped or touched.
	for (UInt8 i = 0; i < 6; i++) {
		UInt8 reg = (UInt8)(kIOPCIConfigBaseAddress0 + 4 * i);
		IOMemoryDescriptor *mem = pci->getDeviceMemoryWithRegister(reg);
		if (!mem)
			continue;
		N33LOG("BAR%u reg 0x%02x phys 0x%llx len 0x%llx (%llu MiB)", i, reg,
		       (unsigned long long)mem->getPhysicalAddress(),
		       (unsigned long long)mem->getLength(),
		       (unsigned long long)(mem->getLength() >> 20));
	}
}

void Navi33Survey::surveyPcieLink(IOPCIDevice *pci)
{
	UInt8 off = 0;
	if (!pci->findPCICapability(kPcieCapId, &off) || off < 0x40 || off > 0xec || (off & 3)) {
		N33LOG("PCIe capability not found");
		return;
	}
	uint16_t linkStatus = pci->configRead16(off + 0x12);   // Link Status
	N33LOG("PCIe link status 0x%04x (speed code %u, width x%u)", linkStatus,
	       linkStatus & 0xF, (linkStatus >> 4) & 0x3F);
}

void Navi33Survey::surveyResizableBar(IOPCIDevice *pci)
{
	IOByteCount off = 0;
	// IOPCIFamily requires the negated ID for the extended capability list.
	uint32_t header = pci->extendedFindPCICapability(0u - kReBarExtCapId, &off);
	if (!header || header == 0xffffffffu || (header & 0xffff) != kReBarExtCapId ||
	    off < 0x100 || off > 0xff4 || (off & 3)) {
		N33LOG("Resizable BAR capability not found or invalid");
		return;
	}
	const unsigned next = header >> 20;
	const unsigned limit = next ? next : 0x1000;
	if (limit <= off || limit > 0x1000 || (limit & 3) || limit - off < 12) {
		N33LOG("Invalid ReBAR capability boundary");
		return;
	}
	uint32_t first = pci->extendedConfigRead32(off + 8);
	unsigned count = (first >> 5) & 7;
	if (first == 0xffffffffu || count == 0 || count > 6 ||
	    4 + count * 8 > limit - off) {
		N33LOG("Invalid ReBAR entry count");
		return;
	}
	unsigned seen = 0;
	for (unsigned i = 0; i < count; ++i) {
		uint32_t cap = pci->extendedConfigRead32(off + 4 + i * 8);
		uint32_t ctl = i ? pci->extendedConfigRead32(off + 8 + i * 8) : first;
		unsigned bar = ctl & 7;
		unsigned size = (ctl >> 8) & 0x3f;
		// This decoder supports sizes represented by the 28-bit capability bitmap.
		if (cap == 0xffffffffu || ctl == 0xffffffffu || bar > 5 ||
		    (seen & (1u << bar)) || size >= 28 || !(cap & (1u << (size + 4)))) {
			N33LOG("Invalid or unsupported ReBAR entry %u", i);
			return;
		}
		seen |= 1u << bar;
		N33LOG("ReBAR BAR%u supported 0x%08x current %llu MiB", bar, cap >> 4,
		       (unsigned long long)(1ULL << size));
	}
}
