//
//  Navi33Survey.hpp — READ-ONLY PCI survey service for the AMD Radeon RX 7600
//  (Navi 33, PCI 1002:7480, subsystem 148c:2421) on x86 macOS.
//
//  This kext does NOT initialise the GPU. It writes nothing to the device and
//  does not map any register BAR. It only reads PCI config space and the BAR
//  sizes/addresses that macOS has already assigned, then logs/publishes metadata.
//
//  Opt-in: it only matches when navi33survey=1 is present and the PCI identity
//  and subsystem IDs match the exact board in the Linux evidence capture.
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
	void publishPreflightState();
	void surveyIdentity(IOPCIDevice *pci);
	void surveyBars(IOPCIDevice *pci);
	void surveyPcieLink(IOPCIDevice *pci);
	void surveyResizableBar(IOPCIDevice *pci);
};

#endif
