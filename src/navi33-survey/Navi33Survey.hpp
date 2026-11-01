//
//  Navi33Survey.hpp — READ-ONLY PCI survey service for the AMD Radeon RX 7600
//  (Navi 33, RDNA 3, PCI 1002:7480) on x86 macOS (Hackintosh).
//
//  This kext does NOT initialise the GPU. It writes nothing to the device and
//  does not map any register BAR. It only reads PCI config space and the BAR
//  sizes/addresses that macOS has already assigned, and logs them.
//
//  Opt-in: it only matches when the boot-arg  navi33survey=1  is present.
//
#ifndef Navi33Survey_hpp
#define Navi33Survey_hpp

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

class Navi33Survey : public IOService {
	OSDeclareDefaultStructors(Navi33Survey)
	using super = IOService;

public:
	IOService *probe(IOService *provider, SInt32 *score) override;
	bool       start(IOService *provider) override;
	void       stop(IOService *provider) override;

private:
	void surveyIdentity(IOPCIDevice *pci);
	void surveyBars(IOPCIDevice *pci);
	void surveyPcieLink(IOPCIDevice *pci);
	void surveyResizableBar(IOPCIDevice *pci);
};

#endif
