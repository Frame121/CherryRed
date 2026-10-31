#ifndef COBALT64_NAVI33_ACCELERATOR_HPP
#define COBALT64_NAVI33_ACCELERATOR_HPP

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>

// Navi33Accelerator — the C6/IOAccelerator-direction service.
//
// It attaches to the Navi33Bringup service (no PCI matching of its own) and
// evaluates the compute readiness gate from the evidence properties the
// bring-up stages publish.
//
// When the gate opens (pciTransport, firmware, memory, queue all verified),
// it publishes "Navi33Accelerator,State" = "ready" and admits user client
// connections (Navi33UserClient) for userspace compute submission.

class Navi33Accelerator : public IOService {
	OSDeclareDefaultStructors(Navi33Accelerator)
	using super = IOService;

public:
	IOService *probe(IOService *provider, SInt32 *score) override;
	bool       start(IOService *provider) override;
	void       stop(IOService *provider) override;

	IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	                       IOUserClient **handler) override;

private:
	void publishBlockedState(const char *reasonName);
	bool gateOpen_ {false};
};

#endif
