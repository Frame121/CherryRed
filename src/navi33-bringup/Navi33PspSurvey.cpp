//
//  Navi33PspSurvey.cpp — read-only PSP/SMU mailbox survey (boot-arg navi33psp=1).
//
//  Maps the MMIO register BAR (BAR5) and READS PSP/SMU mailbox registers so
//  the next bring-up decisions rest on live hardware state instead of the
//  captured Linux evidence alone. Zero writes of any kind: the PCI command
//  register is read, never written — if PCI memory decode is off, the survey
//  aborts instead of enabling it. No firmware is loaded; the scaffold's
//  fail-closed markers stay in force.
//
#include "Navi33PspSurvey.hpp"
#include "Navi33PspMailbox.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>

#define N33PSPLOG(fmt, ...) IOLog("Navi33Bringup/PSP: " fmt "\n", ##__VA_ARGS__)

// Log pacing: give the console time to print, so a hang shows the last step.
#define N33PSP_STEP(...) do { N33PSPLOG(__VA_ARGS__); IOSleep(40); } while (0)

using cobalt64::navi33::psp::kMailboxSpecs;
using cobalt64::navi33::psp::kMailboxSpecCount;
using cobalt64::navi33::psp::kPciCommandMemorySpace;
using cobalt64::navi33::psp::kRegBarIndex;
using cobalt64::navi33::psp::kSpecBootloader;
using cobalt64::navi33::psp::kSpecIfwiInit;
using cobalt64::navi33::psp::kSpecSmuResponse;
using cobalt64::navi33::psp::kSpecSosAlive;
using cobalt64::navi33::psp::MailboxSpec;
using cobalt64::navi33::psp::dwordInBar;
using cobalt64::navi33::psp::ifwiInitDone;
using cobalt64::navi33::psp::mailboxByteOffset;
using cobalt64::navi33::psp::pspBootloaderReady;
using cobalt64::navi33::psp::secureOsAlive;
using cobalt64::navi33::psp::smuResponded;
using cobalt64::navi33::psp::smuResponseOk;
using cobalt64::navi33::psp::smuResponseStatus;

namespace {

// One register read: bounds-checked against the mapped BAR length, then read
// from the dword-indexed window. There is deliberately no write helper here.
inline bool readMailboxDword(const volatile uint32_t *bar,
                             uint64_t barLength,
                             const MailboxSpec &spec,
                             uint32_t *outValue)
{
	const uint64_t off = mailboxByteOffset(spec.baseDwords, spec.dword);
	if (!dwordInBar(off, barLength))
		return false;
	*outValue = bar[off / 4];
	return true;
}

}  // namespace

void SurveyPspMailbox(IOPCIDevice *pci, IOService *publisher)
{
	if (!pci || !publisher)
		return;

	N33PSP_STEP("PSP/SMU mailbox survey starting (read-only, zero writes)");

	// Read-only check of the PCI command register. The display stack already
	// runs on this card, so memory decode must be on; if it is not, abort
	// rather than writing the command register (zero-write contract).
	const uint16_t command = pci->configRead16(kIOPCIConfigCommand);
	publisher->setProperty("Navi33Bringup,PspStage,PciCommandRaw",
	                       static_cast<uint64_t>(command), 16);
	if (!(command & kPciCommandMemorySpace)) {
		publisher->setProperty("Navi33Bringup,PspStage",
		                       "aborted: PCI memory decode is off; refusing to enable it (zero-write contract)");
		N33PSPLOG("aborted: PCI command 0x%04x has no memory-space bit", command);
		return;
	}

	IOMemoryMap *regs = pci->mapDeviceMemoryWithRegister(
	    static_cast<UInt8>(kIOPCIConfigBaseAddress0 + kRegBarIndex * 4));
	if (!regs) {
		publisher->setProperty("Navi33Bringup,PspStage",
		                       "aborted: register BAR5 map failed");
		N33PSPLOG("could not map register BAR%u", kRegBarIndex);
		return;
	}
	const uint64_t barLength = static_cast<uint64_t>(regs->getLength());
	publisher->setProperty("Navi33Bringup,PspStage,RegisterBARIndex",
	                       static_cast<uint64_t>(kRegBarIndex), 8);
	publisher->setProperty("Navi33Bringup,PspStage,RegisterBARLength", barLength, 64);
	publisher->setProperty("Navi33Bringup,PspStage,RegisterBARMapped", true);
	N33PSP_STEP("register BAR%u mapped, %llu KB (reads only)", kRegBarIndex,
	            (unsigned long long)(barLength >> 10));

	const volatile uint32_t *bar =
	    reinterpret_cast<volatile const uint32_t *>(regs->getVirtualAddress());
	if (!bar) {
		publisher->setProperty("Navi33Bringup,PspStage",
		                       "aborted: register BAR5 map has no virtual address");
		regs->release();
		return;
	}

	// Bounded wait for IFWI init (same 2 s bound RedNavi verified on Navi 32).
	// The mailbox values themselves are this stage's evidence: they stay
	// UNVERIFIED for Navi 33 until this readback is collected.
	uint32_t values[kMailboxSpecCount] = {};
	bool readOk[kMailboxSpecCount] = {};
	uint32_t msg33 = 0;
	unsigned waitedMs = 0;
	for (; waitedMs < 2000; ++waitedMs) {
		if (readMailboxDword(bar, barLength, kMailboxSpecs[kSpecIfwiInit], &msg33) &&
		    ifwiInitDone(msg33))
			break;
		IOSleep(1);
	}
	publisher->setProperty("Navi33Bringup,PspStage,IfwiInitWaitMs",
	                       static_cast<uint64_t>(waitedMs), 32);
	publisher->setProperty("Navi33Bringup,PspStage,IfwiInitDone",
	                       ifwiInitDone(msg33));
	N33PSP_STEP("MP0_C2PMSG_33 = 0x%08x (IFWI init done: %u, waited %u ms)",
	            msg33, ifwiInitDone(msg33) ? 1u : 0u, waitedMs);

	// Collect and publish every mailbox register of the pinned table. A
	// register whose byte offset falls outside the mapped window is reported
	// as skipped — never read through a hole.
	OSDictionary *published = OSDictionary::withCapacity(kMailboxSpecCount);
	for (unsigned i = 0; i < kMailboxSpecCount; ++i) {
		const MailboxSpec &spec = kMailboxSpecs[i];
		readOk[i] = readMailboxDword(bar, barLength, spec, &values[i]);
		if (!readOk[i]) {
			const uint64_t off = mailboxByteOffset(spec.baseDwords, spec.dword);
			N33PSPLOG("%s: byte offset 0x%llx outside the %llu-byte BAR — skipped",
			          spec.name, (unsigned long long)off,
			          (unsigned long long)barLength);
			continue;
		}
		N33PSPLOG("%s = 0x%08x (%s)", spec.name, values[i], spec.note);
		IOSleep(20);
		if (published) {
			OSNumber *value = OSNumber::withNumber(
			    static_cast<uint64_t>(values[i]), 64);
			if (value) {
				published->setObject(spec.name, value);
				value->release();
			}
		}
	}
	if (published) {
		publisher->setProperty("Navi33Bringup,PspMailboxRegs", published);
		published->release();
	}

	// Decoded status, mirroring RedNavi's verified Navi 32 M1.5 decodes.
	const bool decoderInputsOk = readOk[kSpecBootloader] && readOk[kSpecSosAlive] &&
	                             readOk[kSpecSmuResponse];
	publisher->setProperty("Navi33Bringup,PspStage,DecoderInputsRead", decoderInputsOk);
	publisher->setProperty("Navi33Bringup,PspStage,PspBootloaderReady",
	                       readOk[kSpecBootloader] &&
	                           pspBootloaderReady(values[kSpecBootloader]));
	publisher->setProperty("Navi33Bringup,PspStage,SecureOsAlive",
	                       readOk[kSpecSosAlive] &&
	                           secureOsAlive(values[kSpecSosAlive]));
	publisher->setProperty("Navi33Bringup,PspStage,SmuResponded",
	                       readOk[kSpecSmuResponse] &&
	                           smuResponded(values[kSpecSmuResponse]));
	publisher->setProperty("Navi33Bringup,PspStage,SmuResponseOk",
	                       readOk[kSpecSmuResponse] &&
	                           smuResponseOk(values[kSpecSmuResponse]));
	if (readOk[kSpecSmuResponse]) {
		publisher->setProperty("Navi33Bringup,PspStage,SmuResponseStatus",
		                       static_cast<uint64_t>(
		                           smuResponseStatus(values[kSpecSmuResponse])), 16);
	}

	// The zero-write contract holds in this stage too: registers were only read.
	publisher->setProperty("Navi33Bringup,PspStage,HardwareWrites", false);
	publisher->setProperty("Navi33Bringup,PspStage,RegisterWrites", false);
	publisher->setProperty("Navi33Bringup,PspStage,FirmwareLoaded", false);
	publisher->setProperty("Navi33Bringup,PspStage",
	                       ifwiInitDone(msg33)
	                           ? "mailbox-read-only complete: PSP/SMU mailbox state published"
	                           : "mailbox-read-only complete: IFWI init bit NOT observed within 2 s");

	regs->release();
	N33PSPLOG("mailbox survey done (read-only, no writes, no firmware load)");
}
