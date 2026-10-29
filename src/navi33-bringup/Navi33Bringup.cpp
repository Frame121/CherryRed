//
//  Navi33Bringup.cpp — see Navi33Bringup.hpp. FAIL-CLOSED SCAFFOLD.
//  READ-ONLY discovery only; no register BAR mapped, no writes, no firmware load.
//
#include "Navi33Bringup.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>

#define N33LOG(fmt, ...) IOLog("Navi33Bringup: " fmt "\n", ##__VA_ARGS__)

static constexpr uint16_t kAmdVendorId   = 0x1002;
static constexpr uint16_t kNavi33Device  = 0x7480;   // Radeon RX 7600
// Restrict this bring-up to the exact board captured in the Linux evidence.
static constexpr uint16_t kTargetSubsystemVendorId = 0x148C;
static constexpr uint16_t kTargetSubsystemDeviceId  = 0x2421;
static constexpr uint32_t kPcieCapId     = 0x10;     // PCI Express capability (standard list)
static constexpr uint32_t kReBarExtCapId = 0x0015;   // Resizable BAR (extended list)
static const char *const kBarRegisterKeys[6] = {
	"Navi33Bringup,BAR0,Register", "Navi33Bringup,BAR1,Register",
	"Navi33Bringup,BAR2,Register", "Navi33Bringup,BAR3,Register",
	"Navi33Bringup,BAR4,Register", "Navi33Bringup,BAR5,Register",
};
static const char *const kBarAddressKeys[6] = {
	"Navi33Bringup,BAR0,PhysicalAddress", "Navi33Bringup,BAR1,PhysicalAddress",
	"Navi33Bringup,BAR2,PhysicalAddress", "Navi33Bringup,BAR3,PhysicalAddress",
	"Navi33Bringup,BAR4,PhysicalAddress", "Navi33Bringup,BAR5,PhysicalAddress",
};
static const char *const kBarLengthKeys[6] = {
	"Navi33Bringup,BAR0,Length", "Navi33Bringup,BAR1,Length",
	"Navi33Bringup,BAR2,Length", "Navi33Bringup,BAR3,Length",
	"Navi33Bringup,BAR4,Length", "Navi33Bringup,BAR5,Length",
};
static const char *const kBarRangeValidKeys[6] = {
	"Navi33Bringup,BAR0,RangeValid", "Navi33Bringup,BAR1,RangeValid",
	"Navi33Bringup,BAR2,RangeValid", "Navi33Bringup,BAR3,RangeValid",
	"Navi33Bringup,BAR4,RangeValid", "Navi33Bringup,BAR5,RangeValid",
};

OSDefineMetaClassAndStructors(Navi33Bringup, IOService)

bool Navi33Bringup::optInEnabled()
{
	char enabled[8] = {};
	if (!PE_parse_boot_argn("navi33bringup", enabled, sizeof(enabled)) ||
	    enabled[0] != '1' || enabled[1] != '\0')
		return false;                                  // exact opt-in only
	return true;
}

bool Navi33Bringup::identityMatchesBoard(IOPCIDevice *pci)
{
	if (!pci)
		return false;
	return pci->configRead16(kIOPCIConfigVendorID) == kAmdVendorId &&
	       pci->configRead16(kIOPCIConfigDeviceID) == kNavi33Device &&
	       pci->configRead16(kIOPCIConfigSubSystemVendorID) == kTargetSubsystemVendorId &&
	       pci->configRead16(kIOPCIConfigSubSystemID) == kTargetSubsystemDeviceId;
}

IOService *Navi33Bringup::probe(IOService *provider, SInt32 *score)
{
	if (!optInEnabled())
		return nullptr;                                  // exact opt-in only

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci || !identityMatchesBoard(pci))
		return nullptr;                                  // exact evidenced board only

	return super::probe(provider, score);
}

bool Navi33Bringup::start(IOService *provider)
{
	if (!optInEnabled()) {
		N33LOG("start refused: exact opt-in boot argument is absent");
		return false;
	}

	if (!super::start(provider))
		return false;

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!identityMatchesBoard(pci)) {
		N33LOG("start refused: PCI identity/subsystem is outside the evidenced RX 7600 board");
		super::stop(provider);
		return false;
	}

	N33LOG("start (bring-up SCAFFOLD: read-only discovery, no register BAR mapped, no writes, no firmware)");
	publishPreflightState();
	surveyIdentity(pci);
	surveyBars(pci);
	surveyPcieLink(pci);
	surveyResizableBar(pci);
	refuseFirmwareAndWrites();
	N33LOG("bring-up scaffold done (staged hardware bring-up is future, evidence-gated work)");

	registerService();
	return true;
}

void Navi33Bringup::stop(IOService *provider)
{
	super::stop(provider);
}

void Navi33Bringup::publishPreflightState()
{
	// Status only. None of these enable BAR access, firmware load, or GPU init.
	setProperty("Navi33Bringup,Stage", "bringup-scaffold");
	setProperty("Navi33Bringup,OptInRequired", true);
	setProperty("Navi33Bringup,RegisterBARMapped", false);
	setProperty("Navi33Bringup,HardwareWrites", false);
	setProperty("Navi33Bringup,FirmwareLoaded", false);
	setProperty("Navi33Bringup,GPUInitialized", false);
	setProperty("Navi33Bringup,Acceleration", false);
	setProperty("Navi33Bringup,GfxTarget", "gfx1102");
	setProperty("Navi33Bringup,DcnVersion", "3.2.1");
}

void Navi33Bringup::refuseFirmwareAndWrites()
{
	// Explicit fail-closed marker for this scaffold stage: no firmware is loaded and
	// no register write path exists. The staged bring-up (firmware -> memory ->
	// interrupts -> command -> DCN 3.2.1 display -> Metal) is future, evidence-gated
	// work and must not be enabled from this stage.
	setProperty("Navi33Bringup,FirmwareRefused", true);
	setProperty("Navi33Bringup,WritePathPresent", false);
	N33LOG("firmware load and register writes refused at scaffold stage (fail-closed)");
}

void Navi33Bringup::surveyIdentity(IOPCIDevice *pci)
{
	uint16_t vendor    = pci->configRead16(kIOPCIConfigVendorID);
	uint16_t device    = pci->configRead16(kIOPCIConfigDeviceID);
	uint8_t  revision  = pci->configRead8(kIOPCIConfigRevisionID);
	uint16_t subVendor = pci->configRead16(kIOPCIConfigSubSystemVendorID);
	uint16_t subDevice = pci->configRead16(kIOPCIConfigSubSystemID);
	setProperty("Navi33Bringup,VendorID", static_cast<uint64_t>(vendor), 16);
	setProperty("Navi33Bringup,DeviceID", static_cast<uint64_t>(device), 16);
	setProperty("Navi33Bringup,RevisionID", static_cast<uint64_t>(revision), 8);
	setProperty("Navi33Bringup,SubsystemVendorID", static_cast<uint64_t>(subVendor), 16);
	setProperty("Navi33Bringup,SubsystemID", static_cast<uint64_t>(subDevice), 16);
	setProperty("Navi33Bringup,ExactEvidenceBoardMatch", true);
	N33LOG("id %04x:%04x rev %02x subsystem %04x:%04x",
	       vendor, device, revision, subVendor, subDevice);
}

void Navi33Bringup::surveyBars(IOPCIDevice *pci)
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
	setProperty("Navi33Bringup,ResourceCount", static_cast<uint64_t>(resourceCount), 32);
	setProperty("Navi33Bringup,ResourceMetadataValid",
	            resourceCount != 0 && resourceMetadataValid);
}

void Navi33Bringup::surveyPcieLink(IOPCIDevice *pci)
{
	IOByteCount off = 0;
	if (!pci->findPCICapability(kPcieCapId, &off) || off == 0) {
		setProperty("Navi33Bringup,PCIeCapabilityFound", false);
		N33LOG("PCIe capability not found");
		return;
	}
	uint16_t linkStatus = pci->configRead16(off + 0x12);   // Link Status
	setProperty("Navi33Bringup,PCIeCapabilityFound", true);
	setProperty("Navi33Bringup,PCIeLinkStatus", static_cast<uint64_t>(linkStatus), 16);
	setProperty("Navi33Bringup,PCIeLinkSpeedCode",
	             static_cast<uint64_t>(linkStatus & 0xF), 4);
	setProperty("Navi33Bringup,PCIeLinkWidth",
	             static_cast<uint64_t>((linkStatus >> 4) & 0x3F), 6);
	N33LOG("PCIe link status 0x%04x (speed code %u, width x%u)", linkStatus,
	       linkStatus & 0xF, (linkStatus >> 4) & 0x3F);
}

void Navi33Bringup::surveyResizableBar(IOPCIDevice *pci)
{
	IOByteCount off = 0;
	if (!pci->extendedFindPCICapability(kReBarExtCapId, &off) || off == 0) {
		setProperty("Navi33Bringup,ResizableBARFound", false);
		N33LOG("Resizable BAR capability not found");
		return;
	}
	uint32_t cap = pci->configRead32(off + 4);   // supported sizes (bit n => 2^(n+20) bytes)
	uint32_t ctl = pci->configRead32(off + 8);   // current size field in bits 13:8
	setProperty("Navi33Bringup,ResizableBARFound", true);
	setProperty("Navi33Bringup,ResizableBARSizes", static_cast<uint64_t>(cap), 32);
	setProperty("Navi33Bringup,ResizableBARControl", static_cast<uint64_t>(ctl), 32);
	unsigned curShift = ((ctl >> 8) & 0x3F) + 20;
	if (curShift > 62) {                         // bogus/unreadable value: do not shift
		N33LOG("ReBAR cap 0x%08x ctl 0x%08x (size field out of range)", cap, ctl);
		return;
	}
	setProperty("Navi33Bringup,ResizableBARCurrentSizeMiB",
	             static_cast<uint64_t>((1ULL << curShift) >> 20), 32);
	N33LOG("ReBAR cap 0x%08x ctl 0x%08x (current BAR size 2^%u = %llu MiB)",
	       cap, ctl, curShift, (unsigned long long)(1ULL << curShift) >> 20);
}
