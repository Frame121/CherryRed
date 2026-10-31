//
//  Navi33Bringup.cpp — see Navi33Bringup.hpp. FAIL-CLOSED SCAFFOLD.
//  READ-ONLY discovery only; no register BAR mapped, no writes, no firmware load.
//
#include "Navi33Bringup.hpp"
#include "Navi33PciMatch.hpp"
#include "Navi33PspSurvey.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSIterator.h>
#include <string.h>

#define N33LOG(fmt, ...) IOLog("Navi33Bringup: " fmt "\n", ##__VA_ARGS__)

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

static bool bootArgTokenPresent(const char *args, const char *token)
{
	// Whole-token match: "navi33bringup=1" matches, "xnavi33bringup=1" does not.
	const size_t tlen = strlen(token);
	for (const char *p = args; *p != '\0'; p++) {
		if (strncmp(p, token, tlen) != 0)
			continue;
		const bool leftBounded = (p == args) || p[-1] == ' ' || p[-1] == '\t';
		const char after = p[tlen];
		const bool rightBounded = after == '\0' || after == ' ' || after == '\t';
		if (leftBounded && rightBounded)
			return true;
	}
	return false;
}

static bool readChosenBootArgs(char (&args)[256])
{
	// Read the effective boot-args from the device-tree "chosen" node instead of
	// the pexpert boot-argument API: com.apple.kpi.pexpert no longer exists as a
	// linkable library on macOS 26, and kxld refuses to link a kext whose
	// symbols cannot be resolved from the declared OSBundleLibraries. gIODTPlane
	// and IORegistryEntry::fromPath come from the declared com.apple.kpi.iokit.
	IORegistryEntry *root = IORegistryEntry::getRegistryRoot();
	if (!root)
		return false;

	IORegistryEntry *chosen = IORegistryEntry::fromPath("/chosen", gIODTPlane);
	OSIterator *children = nullptr;
	if (!chosen) {
		children = root->getChildIterator(gIODTPlane);
		if (!children)
			return false;
		while ((chosen = OSDynamicCast(IORegistryEntry, children->getNextObject())) != nullptr) {
			if (chosen->getName() && !strcmp(chosen->getName(), "chosen"))
				break;
		}
		if (chosen && (!chosen->getName() || strcmp(chosen->getName(), "chosen") != 0))
			chosen = nullptr;
		children->release();
	}
	if (!chosen)
		return false;

	OSData *data = OSDynamicCast(OSData, chosen->getProperty("boot-args"));
	if (!data)
		return false;
	size_t len = data->getLength();
	const char *bytes = static_cast<const char *>(data->getBytesNoCopy());
	if (!bytes || len == 0)
		return false;

	if (len >= sizeof(args))
		len = sizeof(args) - 1;
	memcpy(args, bytes, len);
	args[len] = '\0';
	return true;
}

bool Navi33Bringup::optInEnabled()
{
	char args[256];
	if (!readChosenBootArgs(args))
		return false;
	return bootArgTokenPresent(args, "navi33bringup=1");
}

bool Navi33Bringup::pspStageEnabled()
{
	// The PSP/SMU mailbox survey maps the register BAR (reads only), so it
	// needs its own explicit opt-in on top of the bring-up opt-in.
	char args[256];
	if (!readChosenBootArgs(args))
		return false;
	return bootArgTokenPresent(args, "navi33bringup=1") &&
	       bootArgTokenPresent(args, "navi33psp=1");
}

bool Navi33Bringup::identityMatchesGpu(IOPCIDevice *pci)
{
	if (!pci)
		return false;
	const cobalt64::navi33::PciIdentity identity = {
		pci->configRead16(kIOPCIConfigVendorID),
		pci->configRead16(kIOPCIConfigDeviceID),
	};
	return cobalt64::navi33::isRx7600(identity);
}

bool Navi33Bringup::identityMatchesEvidenceBoard(IOPCIDevice *pci)
{
	// This is a provenance match only; it is deliberately not the GPU match gate.
	if (!pci)
		return false;
	const cobalt64::navi33::PciIdentity identity = {
		pci->configRead16(kIOPCIConfigVendorID),
		pci->configRead16(kIOPCIConfigDeviceID),
		pci->configRead16(kIOPCIConfigSubSystemVendorID),
		pci->configRead16(kIOPCIConfigSubSystemID),
	};
	return cobalt64::navi33::matchesCapturedEvidenceBoard(identity);
}

IOService *Navi33Bringup::probe(IOService *provider, SInt32 *score)
{
	if (!optInEnabled())
		return nullptr;                                  // exact opt-in only

	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	if (!pci || !identityMatchesGpu(pci))
		return nullptr;                                  // RX 7600 PCI ID only

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
	if (!identityMatchesGpu(pci)) {
		N33LOG("start refused: PCI identity is not AMD Radeon RX 7600 (1002:7480)");
		super::stop(provider);
		return false;
	}

	N33LOG("start (bring-up SCAFFOLD: read-only discovery, no register BAR mapped, no writes, no firmware)");
	publishPreflightState();
	surveyIdentity(pci);
	surveyBars(pci);
	surveyPcieLink(pci);
	surveyResizableBar(pci);
	if (pspStageEnabled()) {
		// Second opt-in (navi33psp=1): maps the MMIO register BAR (BAR5) and
		// reads PSP/SMU mailbox registers. Reads only — the zero-write
		// contract still holds, and no firmware is loaded.
		SurveyPspMailbox(pci, this);
	} else {
		setProperty("Navi33Bringup,PspStage",
		            "disabled (boot-arg navi33psp=1 absent)");
	}
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
	setProperty("Navi33Bringup,Stage", "gpu-identified-read-only");
	setProperty("Navi33Bringup,RecognitionStatus", "PCI-ID matched: AMD Radeon RX 7600");
	setProperty("Navi33Bringup,GPUName", "AMD Radeon RX 7600");
	setProperty("Navi33Bringup,Architecture", "Navi 33");
	setProperty("Navi33Bringup,GfxTarget", "gfx1102");
	setProperty("Navi33Bringup,PCIIdentifier", "1002:7480");
	setProperty("Navi33Bringup,OptInRequired", true);
	setProperty("Navi33Bringup,RegisterBARMapped", false);
	setProperty("Navi33Bringup,HardwareWrites", false);
	setProperty("Navi33Bringup,FirmwareLoaded", false);
	setProperty("Navi33Bringup,GPUInitialized", false);
	setProperty("Navi33Bringup,Acceleration", false);
	setProperty("Navi33Bringup,DcnVersion", "3.2.1");
	setProperty("Navi33Bringup,IPDiscoveryLive", false);
	setProperty("Navi33Bringup,ProfileSource", "offline RX7600 evidence; runtime IP Discovery not yet connected");
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
	bool evidenceBoardMatch = identityMatchesEvidenceBoard(pci);
	setProperty("Navi33Bringup,SubsystemVendorID", static_cast<uint64_t>(subVendor), 16);
	setProperty("Navi33Bringup,SubsystemID", static_cast<uint64_t>(subDevice), 16);
	setProperty("Navi33Bringup,ExactEvidenceBoardMatch", evidenceBoardMatch);
	setProperty("Navi33Bringup,BoardSupportStatus",
	            evidenceBoardMatch ? "matches captured subsystem 148c:2421"
	                               : "RX 7600 PCI ID recognized; subsystem not evidence-matched");
	setProperty("Navi33Bringup,FutureHardwareInitAllowed", false);
	N33LOG("GPU recognized: AMD Radeon RX 7600 (1002:7480), Navi 33 gfx1102");
	N33LOG("revision %02x subsystem %04x:%04x (evidence board match=%u)",
	       revision, subVendor, subDevice, evidenceBoardMatch);
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
	// findPCICapability returns a UInt8 config-space offset (IOPCIDevice.h).
	UInt8 off = 0;
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

	// PCIe Resizable BAR Capability: BAR Index in bits 2:0, Supported Sizes in
	// bits 31:4 (encoding N is bit N+4; size = 2^(N+20) bytes).
	// Control: BAR Index in bits 2:0, Number of Resizable BARs in bits 7:5,
	// and the selected size encoding in bits 13:8. This stage only reads config.
	uint32_t cap = pci->configRead32(off + 4);
	uint32_t ctl = pci->configRead32(off + 8);
	uint32_t capBarIndex = cap & 0x7;
	uint32_t ctlBarIndex = ctl & 0x7;
	uint32_t supportedSizes = cap >> 4;          // encodings 0..27 (1 MiB..128 TiB)
	uint32_t extendedSupportedSizes = ctl >> 16; // encodings 28..43 (256 TiB..8 EiB)
	uint32_t count = (ctl >> 5) & 0x7;
	uint32_t sizeEncoding = (ctl >> 8) & 0x3F;

	setProperty("Navi33Bringup,ResizableBARFound", true);
	setProperty("Navi33Bringup,ResizableBARCapabilityRaw", static_cast<uint64_t>(cap), 32);
	setProperty("Navi33Bringup,ResizableBARControlRaw", static_cast<uint64_t>(ctl), 32);
	setProperty("Navi33Bringup,ResizableBARIndex", static_cast<uint64_t>(capBarIndex), 3);
	setProperty("Navi33Bringup,ResizableBARControlIndex", static_cast<uint64_t>(ctlBarIndex), 3);
	setProperty("Navi33Bringup,ResizableBARCount", static_cast<uint64_t>(count), 3);
	setProperty("Navi33Bringup,ResizableBARSizes", static_cast<uint64_t>(supportedSizes), 28);
	setProperty("Navi33Bringup,ResizableBARLargeSizes", static_cast<uint64_t>(extendedSupportedSizes), 16);
	setProperty("Navi33Bringup,ResizableBARSizeEncoding", static_cast<uint64_t>(sizeEncoding), 6);

	// PCIe 4.0+: encodings 0..27 live in Capability bits 31:4; encodings
	// 28..43 live in Control bits 31:16. Do not reject valid large BAR sizes.
	bool encodingValid = sizeEncoding <= 43;
	bool encodingSupported = false;
	if (encodingValid) {
		if (sizeEncoding <= 27)
			encodingSupported = (supportedSizes & (1U << sizeEncoding)) != 0;
		else
			encodingSupported = (extendedSupportedSizes & (1U << (sizeEncoding - 28))) != 0;
	}
	bool indicesMatch = capBarIndex == ctlBarIndex;
	setProperty("Navi33Bringup,ResizableBARSizeEncodingValid", encodingValid);
	setProperty("Navi33Bringup,ResizableBARCurrentSizeSupported", encodingSupported);
	setProperty("Navi33Bringup,ResizableBARIndicesMatch", indicesMatch);

	if (!encodingValid || !encodingSupported || !indicesMatch) {
		N33LOG("ReBAR metadata rejected: cap 0x%08x ctl 0x%08x (encoding valid=%u supported=%u indices-match=%u)",
		       cap, ctl, encodingValid, encodingSupported, indicesMatch);
		return;
	}

	uint64_t currentSizeMiB = 1ULL << sizeEncoding;
	setProperty("Navi33Bringup,ResizableBARCurrentSizeMiB", currentSizeMiB, 64);
	N33LOG("ReBAR cap 0x%08x ctl 0x%08x (BAR%u current size encoding %u = %llu MiB; supported)",
	       cap, ctl, capBarIndex, sizeEncoding, (unsigned long long)currentSizeMiB);
}
