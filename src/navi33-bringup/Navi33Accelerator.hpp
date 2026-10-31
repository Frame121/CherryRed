#ifndef COBALT64_NAVI33_ACCELERATOR_HPP
#define COBALT64_NAVI33_ACCELERATOR_HPP

#include <IOKit/IOService.h>

// Navi33Accelerator — the C6/IOAccelerator-direction service skeleton.
//
// It attaches to the Navi33Bringup service (no PCI matching of its own) and
// evaluates the compute readiness gate from the evidence properties the
// bring-up stages publish. It performs NO hardware access of any kind: no
// BAR mapping, no config writes, no firmware. While the gate is closed it
// stays alive and publishes exactly why Metal dispatch is unavailable; the
// real accelerator work (command submission backend, user client, Metal
// device descriptor) only attaches behind those same evidence properties.
//
// This skeleton exists so the Metal layer's contract and its refusal
// behavior are built and reviewed now — not so that Metal works today.

class Navi33Accelerator : public IOService {
	OSDeclareDefaultStructors(Navi33Accelerator)
	using super = IOService;

public:
	IOService *probe(IOService *provider, SInt32 *score) override;
	bool       start(IOService *provider) override;
	void       stop(IOService *provider) override;

private:
	void publishBlockedState(const char *reasonName);
};

#endif
