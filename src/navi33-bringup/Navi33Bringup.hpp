//
//  Navi33Bringup.hpp — Navi 33 (RX 7600) bring-up kext SCAFFOLD on x86 macOS.
//
//  This is a fail-closed bring-up skeleton, NOT a working GPU driver. At this
//  stage it:
//    - recognizes RX 7600 by PCI vendor/device ID (1002:7480), independent
//      of board-vendor subsystem IDs; reports whether the subsystem matches
//      the one captured in the Linux evidence (148c:2421),
//    - is opt-in (navi33bringup=1) and refuses to start otherwise,
//    - performs READ-ONLY PCI identity / BAR / PCIe / ReBAR discovery,
//    - REFUSES to load firmware and REFUSES to write any register.
//
//  It never maps a register BAR and never writes to the device. The staged
//  hardware bring-up (firmware -> memory -> interrupts -> command -> DCN 3.2.1
//  display -> Metal) is future work gated on the evidence milestones in
//  RX7600-Linux-Evidence/ and the Mac-day runbook.
//
#ifndef Navi33Bringup_hpp
#define Navi33Bringup_hpp

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

class Navi33Bringup : public IOService {
	OSDeclareDefaultStructors(Navi33Bringup)
	using super = IOService;

public:
	IOService *probe(IOService *provider, SInt32 *score) override;
	bool       start(IOService *provider) override;
	void       stop(IOService *provider) override;

private:
	bool optInEnabled();
	bool identityMatchesGpu(IOPCIDevice *pci);
	bool identityMatchesEvidenceBoard(IOPCIDevice *pci);
	void publishPreflightState();
	void surveyIdentity(IOPCIDevice *pci);
	void surveyBars(IOPCIDevice *pci);
	void surveyPcieLink(IOPCIDevice *pci);
	void surveyResizableBar(IOPCIDevice *pci);
	void refuseFirmwareAndWrites();
};

#endif
