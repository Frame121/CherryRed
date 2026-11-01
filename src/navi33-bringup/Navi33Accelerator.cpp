//
//  Navi33Accelerator.cpp — compute readiness gate + accelerator skeleton.
//
//  Attached to the Navi33Bringup service node. Evaluates the compute-first
//  gate from the bring-up evidence properties and publishes the result.
//  ZERO hardware access in every state: this file must never contain a BAR
//  mapping, a config write, or a register write — the safety contract test
//  enforces that at source level.
//
#include "Navi33Accelerator.hpp"
#include "Navi33ClientContracts.hpp"
#include "Navi33UserClient.hpp"

#include <IOKit/IOLib.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSDictionary.h>

#define N33ACCLOG(fmt, ...) IOLog("Navi33Accelerator: " fmt "\n", ##__VA_ARGS__)

using cobalt64::research::client::BringupEvidenceFlags;
using cobalt64::research::client::ClientError;
using cobalt64::research::client::clientErrorName;
using cobalt64::research::client::computeGate;
using cobalt64::research::client::readinessFromBringupEvidence;
using cobalt64::research::IntegrationReadiness;

OSDefineMetaClassAndStructors(Navi33Accelerator, IOService)

static bool boolProperty(IOService *provider, const char *key)
{
	OSBoolean *value = OSDynamicCast(OSBoolean, provider->getProperty(key));
	return value != nullptr && value->getValue();
}

static BringupEvidenceFlags evidenceFrom(IOService *provider)
{
	BringupEvidenceFlags flags;
	// The bring-up node publishes "Navi33Bringup,Stage" before anything else;
	// its presence is the PCI-transport evidence (the PCI survey ran).
	flags.bringupStarted =
	    provider->getProperty("Navi33Bringup,Stage") != nullptr;
	flags.firmwareVerified =
	    boolProperty(provider, "Navi33Bringup,FirmwareVerified");
	flags.memoryVerified =
	    boolProperty(provider, "Navi33Bringup,MemoryVerified");
	flags.queueVerified =
	    boolProperty(provider, "Navi33Bringup,QueueVerified");
	return flags;
}

IOService *Navi33Accelerator::probe(IOService *provider, SInt32 *score)
{
	// Attach only to a live bring-up node; there is nothing to accelerate
	// otherwise.
	if (provider == nullptr || provider->getProperty("Navi33Bringup,Stage") == nullptr)
		return nullptr;
	return super::probe(provider, score);
}

void Navi33Accelerator::publishBlockedState(const char *reasonName)
{
	setProperty("Navi33Accelerator,State", "blocked");
	setProperty("Navi33Accelerator,BlockedReason", reasonName);
	setProperty("Navi33Accelerator,MetalDispatchAvailable", false);
	setProperty("Navi33Accelerator,HardwareAccess", false);
	setProperty("Navi33Accelerator,FirmwareLoaded", false);
}

bool Navi33Accelerator::start(IOService *provider)
{
	if (!super::start(provider))
		return false;

	N33ACCLOG("start: evaluating the compute readiness gate (no hardware access)");
	const BringupEvidenceFlags flags = evidenceFrom(provider);
	const IntegrationReadiness readiness = readinessFromBringupEvidence(flags);
	const cobalt64::research::client::ClientError gate = computeGate(readiness);

	if (gate != ClientError::None) {
		gateOpen_ = false;
		publishBlockedState(clientErrorName(gate));
		N33ACCLOG("blocked: %s — Metal dispatch stays unavailable until the "
		          "bring-up stages publish verified evidence",
		          clientErrorName(gate));
		registerService();
		return true;
	}

	// The compute-first gate is OPEN: PCI transport, firmware, memory, and queue
	// submission are all verified on the live card!
	gateOpen_ = true;
	setProperty("Navi33Accelerator,State", "ready");
	setProperty("Navi33Accelerator,MetalDispatchAvailable", true);
	setProperty("Navi33Accelerator,HardwareAccess", false);
	N33ACCLOG("gate open — compute engine verified; user-client bridge available");
	registerService();
	return true;
}

void Navi33Accelerator::stop(IOService *provider)
{
	super::stop(provider);
}

IOReturn Navi33Accelerator::newUserClient(task_t owningTask, void *securityID,
                                         UInt32 type, IOUserClient **handler)
{
	if (!gateOpen_) {
		N33ACCLOG("newUserClient refused: compute gate is closed");
		return kIOReturnNotPermitted;
	}

	Navi33UserClient *client = OSTypeAlloc(Navi33UserClient);
	if (!client)
		return kIOReturnNoMemory;

	if (!client->initWithTask(owningTask, securityID, type)) {
		client->release();
		return kIOReturnBadArgument;
	}

	if (!client->attach(this)) {
		client->release();
		return kIOReturnError;
	}

	if (!client->start(this)) {
		client->detach(this);
		client->release();
		return kIOReturnError;
	}

	*handler = client;
	return kIOReturnSuccess;
}
