//
//  Navi33Bringup.hpp — Navi 33 (RX 7600) bring-up kext on x86 macOS.
//
//  This is a staged bring-up driver for the AMD Radeon RX 7600 (1002:7480).
//  It enforces:
//    - recognition of RX 7600 by PCI vendor/device ID (1002:7480),
//    - opt-in ladder via boot-args (navi33bringup=1, navi33psp=1, etc.),
//    - staged hardware bring-up (PSP -> SMU -> GC -> Memory -> Queue),
//    - evidence publication for the Navi33Accelerator compute gate.
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
	bool pspStageEnabled();
	bool pspLoadStageEnabled();
	bool pspChainStageEnabled();
	bool pspRingStageEnabled();
	bool smuStageEnabled();
	bool gcStageEnabled();
	bool memoryStageEnabled();
	bool queueStageEnabled();
	uint32_t pspTmrMode();
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
