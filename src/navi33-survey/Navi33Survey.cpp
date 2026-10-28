//
//  Navi33Survey.cpp — see Navi33Survey.hpp. READ-ONLY. UNTESTED (never built or loaded).
//
#include "Navi33Survey.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>

#define N33LOG(fmt, ...) IOLog("Navi33Survey: " fmt "\n", ##__VA_ARGS__)

static constexpr uint16_t kAmdVendorId   = 0x1002;
static constexpr uint16_t kNavi33Device  = 0x7480;   // Radeon RX 7600
// Restrict this survey scaffold to the exact board captured in Linux evidence.
static constexpr uint16_t kTargetSubsystemVendorId = 0x148C;
static constexpr uint16_t kTargetSubsystemDeviceId  = 0x2421;
static constexpr uint32_t kPcieCapId     = 0x10;     // PCI Express capability (standard list)
static constexpr uint32_t kReBarExtCapId = 0x0015;   // Resizable BAR (extended list)
static const char *const kBarRegisterKeys[6] = {
	"Navi33Survey,BAR0,Register", "Navi33Survey,BAR1,Register",
	"Navi33Survey,BAR2,Register", "Navi33Survey,BAR3,Register",
	"Navi33Survey,BAR4,Register", "Navi33Survey,BAR5,Register",
};
static const char *const kBarAddressKeys[6] = {
	"Navi33Survey,BAR0,PhysicalAddress", "Navi33Survey,BAR1,PhysicalAddress",
	"Navi33Survey,BAR2,PhysicalAddress", "Navi33Survey,BAR3,PhysicalAddress",
	"Navi33Survey,BAR4,PhysicalAddress", "Navi33Survey,BAR5,PhysicalAddress",
};
static const char *const kBarLengthKeys[6] = {
	"Navi33Survey,BAR0,Length", "Navi33Survey,BAR1,Length",
	"Navi33Survey,BAR2,Length", "Navi33Survey,BAR3,Length",
	"Navi33Survey,BAR4,Length", "Navi33Survey,BAR5,Length",
};
static const char *const kBarRangeValidKeys[6] = {
	"Navi33Survey,BAR0,RangeValid", "Navi33Survey,BAR1,RangeValid",
	"Navi33Survey,BAR2,RangeValid", "Navi33Survey,BAR3,RangeValid",
	"Navi33Survey,BAR4,RangeValid", "Navi33Survey,BAR5,RangeValid",
};

OSDefineMetaClassAndStructors(Navi33Survey, IOService)

IOService *Navi33Survey::probe(IOService *provider, SInt32 *score)
{
	char enabled[8] = {};
	if (!PE_parse_boot_argn("navi33survey", enabled, sizeof(enabled)) ||
	    enabled[0] != '1' || enabled[1] != '\0')
		return nullptr;                                  // exact opt-in only

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci)
		return nullptr;

	if (pci->configRead16(kIOPCIConfigVendorID) != kAmdVendorId ||
	    pci->configRead16(kIOPCIConfigDeviceID) != kNavi33Device ||
	    pci->configRead16(kIOPCIConfigSubSystemVendorID) != kTargetSubsystemVendorId ||
	    pci->configRead16(kIOPCIConfigSubSystemID) != kTargetSubsystemDeviceId)
		return nullptr;                      // exact evidenced board only

	return super::probe(provider, score);
}

bool Navi33Survey::start(IOService *provider)
{
	char enabled[8] = {};
	if (!PE_parse_boot_argn("navi33survey", enabled, sizeof(enabled)) ||
	    enabled[0] != '1' || enabled[1] != '\0') {
		N33LOG("start refused: exact opt-in boot argument is absent");
		return false;
	}

	if (!super::start(provider))
		return false;

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci || pci->configRead16(kIOPCIConfigVendorID) != kAmdVendorId ||
	    pci->configRead16(kIOPCIConfigDeviceID) != kNavi33Device ||
	    pci->configRead16(kIOPCIConfigSubSystemVendorID) != kTargetSubsystemVendorId ||
	    pci->configRead16(kIOPCIConfigSubSystemID) != kTargetSubsystemDeviceId) {
		N33LOG("start refused: PCI identity/subsystem is outside the evidenced RX 7600 board");
		super::stop(provider);
		return false;
	}

	N33LOG("start (read-only survey, no register BAR mapped, no writes)");
	publishPreflightState();
	surveyIdentity(pci);
	surveyBars(pci);
	surveyPcieLink(pci);
	surveyResizableBar(pci);
	N33LOG("survey done");

	registerService();
	return true;
}

void Navi33Survey::publishPreflightState()
{
	// Expose the service's limits to IORegistry consumers. These properties are
	// status only: they do not enable BAR access or imply GPU initialization.
	setProperty("Navi33Survey,Stage", "pci-preflight-only");
	setProperty("Navi33Survey,OptInRequired", true);
	setProperty("Navi33Survey,RegisterBARMapped", false);
	setProperty("Navi33Survey,HardwareWrites", false);
	setProperty("Navi33Survey,GPUInitialized", false);
	setProperty("Navi33Survey,Acceleration", false);
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
	setProperty("Navi33Survey,VendorID", static_cast<uint64_t>(vendor), 16);
	setProperty("Navi33Survey,DeviceID", static_cast<uint64_t>(device), 16);
	setProperty("Navi33Survey,RevisionID", static_cast<uint64_t>(revision), 8);
	setProperty("Navi33Survey,SubsystemVendorID", static_cast<uint64_t>(subVendor), 16);
	setProperty("Navi33Survey,SubsystemID", static_cast<uint64_t>(subDevice), 16);
	setProperty("Navi33Survey,ExactEvidenceBoardMatch", true);
	N33LOG("id %04x:%04x rev %02x subsystem %04x:%04x",
	       vendor, device, revision, subVendor, subDevice);
}

void Navi33Survey::surveyBars(IOPCIDevice *pci)
{
	// Six BAR registers. The upper half of a 64-bit BAR has no descriptor of its
	// own, so a NULL here is normal and skipped. Nothing is mapped or touched.
	uint32_t resourceCount = 0;
	bool resourceMetadataValid = true;
	const uint64_t maxAddress = static_cast<uint64_t>(-1);
	for (UInt8 i = 0; i < 6; i++) {
		UInt8 reg = (UInt8)(kIOPCIConfigBaseAddress0 + 4 * i);
		IOMemoryDescriptor *mem = pci->getDeviceMemoryWithRegister(reg);
		if (!mem)
			continue;
		uint64_t address = static_cast<uint64_t>(mem->getPhysicalAddress());
		uint64_t length = static_cast<uint64_t>(mem->getLength());
		bool rangeValid = length != 0 && address <= maxAddress - (length - 1);
		resourceCount++;
		resourceMetadataValid = resourceMetadataValid && rangeValid;
		setProperty(kBarRegisterKeys[i], static_cast<uint64_t>(reg), 8);
		setProperty(kBarAddressKeys[i], address, 64);
		setProperty(kBarLengthKeys[i], length, 64);
		setProperty(kBarRangeValidKeys[i], rangeValid);
		N33LOG("BAR%u reg 0x%02x phys 0x%llx len 0x%llx (%llu MiB)", i, reg,
		       (unsigned long long)address,
		       (unsigned long long)length,
		       (unsigned long long)(length >> 20));
	}
	// An empty inventory is incomplete, not a valid resource survey.
	setProperty("Navi33Survey,ResourceCount", static_cast<uint64_t>(resourceCount), 32);
	setProperty("Navi33Survey,ResourceMetadataValid",
	            resourceCount != 0 && resourceMetadataValid);
}

void Navi33Survey::surveyPcieLink(IOPCIDevice *pci)
{
	IOByteCount off = 0;
	if (!pci->findPCICapability(kPcieCapId, &off) || off == 0) {
		setProperty("Navi33Survey,PCIeCapabilityFound", false);
		N33LOG("PCIe capability not found");
		return;
	}
	uint16_t linkStatus = pci->configRead16(off + 0x12);   // Link Status
	setProperty("Navi33Survey,PCIeCapabilityFound", true);
	setProperty("Navi33Survey,PCIeLinkStatus", static_cast<uint64_t>(linkStatus), 16);
	setProperty("Navi33Survey,PCIeLinkSpeedCode",
	             static_cast<uint64_t>(linkStatus & 0xF), 4);
	setProperty("Navi33Survey,PCIeLinkWidth",
	             static_cast<uint64_t>((linkStatus >> 4) & 0x3F), 6);
	N33LOG("PCIe link status 0x%04x (speed code %u, width x%u)", linkStatus,
	       linkStatus & 0xF, (linkStatus >> 4) & 0x3F);
}

void Navi33Survey::surveyResizableBar(IOPCIDevice *pci)
{
	IOByteCount off = 0;
	if (!pci->extendedFindPCICapability(kReBarExtCapId, &off) || off == 0) {
		setProperty("Navi33Survey,ResizableBARFound", false);
		N33LOG("Resizable BAR capability not found");
		return;
	}
	uint32_t cap = pci->configRead32(off + 4);   // supported sizes (bit n => 2^(n+20) bytes)
	uint32_t ctl = pci->configRead32(off + 8);   // current size field in bits 13:8
	setProperty("Navi33Survey,ResizableBARFound", true);
	setProperty("Navi33Survey,ResizableBARSizes", static_cast<uint64_t>(cap), 32);
	setProperty("Navi33Survey,ResizableBARControl", static_cast<uint64_t>(ctl), 32);
	unsigned curShift = ((ctl >> 8) & 0x3F) + 20;
	if (curShift > 62) {                         // bogus/unreadable value: do not shift
		N33LOG("ReBAR cap 0x%08x ctl 0x%08x (size field out of range)", cap, ctl);
		return;
	}
	setProperty("Navi33Survey,ResizableBARCurrentSizeMiB",
	             static_cast<uint64_t>((1ULL << curShift) >> 20), 32);
	N33LOG("ReBAR cap 0x%08x ctl 0x%08x (current BAR size 2^%u = %llu MiB)",
	       cap, ctl, curShift, (unsigned long long)(1ULL << curShift) >> 20);
}
