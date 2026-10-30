//
//  Navi33Survey.cpp — read-only PCI survey; build-validated, not loaded.
//
#include "Navi33Survey.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>

#define N33LOG(fmt, ...) IOLog("Navi33Survey: " fmt "\n", ##__VA_ARGS__)

static constexpr UInt16 kAmdVendorId = 0x1002;
static constexpr UInt16 kNavi33Device = 0x7480; // Radeon RX 7600
static constexpr UInt16 kBoardVendorId = 0x148c;
static constexpr UInt16 kBoardDeviceId = 0x2421;
static constexpr UInt8 kPcieCapId = 0x10; // PCI Express capability (standard list)
static constexpr UInt32 kReBarExtCapId =
	static_cast<UInt32>(-0x0015); // IOPCIDevice expects negated PCIe extended-capability IDs

OSDefineMetaClassAndStructors(Navi33Survey, IOService)

IOService *Navi33Survey::probe(IOService *provider, SInt32 *score)
{
	uint32_t enabled = 0;
	if (!PE_parse_boot_argn("navi33survey", &enabled, sizeof(enabled)) || enabled != 1)
		return nullptr;                                  // opt-in only

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci)
		return nullptr;

	if (pci->configRead16(kIOPCIConfigVendorID) != kAmdVendorId ||
	    pci->configRead16(kIOPCIConfigDeviceID) != kNavi33Device ||
	    pci->configRead16(kIOPCIConfigSubSystemVendorID) != kBoardVendorId ||
	    pci->configRead16(kIOPCIConfigSubSystemID) != kBoardDeviceId)
		return nullptr; // Match only the captured RX 7600 board identity.

	return super::probe(provider, score);
}

bool Navi33Survey::start(IOService *provider)
{
	if (!super::start(provider))
		return false;

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci)
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
	if (!pci->findPCICapability(kPcieCapId, &off) || off == 0) {
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
	if (!pci->extendedFindPCICapability(kReBarExtCapId, &off) || off == 0) {
		N33LOG("Resizable BAR capability not found");
		return;
	}
	uint32_t cap = pci->configRead32(off + 4);   // supported sizes (bit n => 2^(n+20) bytes)
	uint32_t ctl = pci->configRead32(off + 8);   // current size field in bits 13:8
	unsigned curShift = ((ctl >> 8) & 0x3F) + 20;
	if (curShift > 62) {                         // bogus/unreadable value: do not shift
		N33LOG("ReBAR cap 0x%08x ctl 0x%08x (size field out of range)", cap, ctl);
		return;
	}
	N33LOG("ReBAR cap 0x%08x ctl 0x%08x (current BAR size 2^%u = %llu MiB)",
	       cap, ctl, curShift, (unsigned long long)(1ULL << curShift) >> 20);
}
