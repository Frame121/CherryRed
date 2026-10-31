// Host unit tests for the C4 user-client contracts and the compute
// readiness gate (Navi33ClientContracts.hpp). No IOKit, no hardware: every
// policy value here is explicitly synthetic and claims no GFX11 constant.
#include "Navi33ClientContracts.hpp"

#include <stdio.h>

namespace {

int failures = 0;

void check(bool condition, const char *label) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}

using cobalt64::research::client::BringupEvidenceFlags;
using cobalt64::research::client::ClientBudget;
using cobalt64::research::client::ClientError;
using cobalt64::research::client::ClientPolicy;
using cobalt64::research::client::computeGate;
using cobalt64::research::client::kClientAbiVersion;
using cobalt64::research::client::kHandleCapacity;
using cobalt64::research::client::policyValid;
using cobalt64::research::client::readinessFromBringupEvidence;
using cobalt64::research::IntegrationReadiness;
using cobalt64::research::IntegrationState;

ClientPolicy syntheticPolicy() {
    ClientPolicy policy;
    policy.abiVersion = kClientAbiVersion;
    policy.maxBufferBytes = 1ull << 30;   // 1 GiB per buffer (synthetic)
    policy.maxClientBytes = 3ull << 30;   // 3 GiB budget (synthetic)
    policy.minAlignment = 256;
    policy.maxAlignment = 4096;
    policy.maxOpenHandles = 8;
    return policy;
}

void testPolicyValidation() {
    ClientPolicy policy = syntheticPolicy();
    check(policyValid(policy), "synthetic policy is valid");

    policy.maxBufferBytes = 0;
    check(!policyValid(policy), "zero buffer cap rejected");
    policy = syntheticPolicy();

    policy.maxClientBytes = (1ull << 30) - 1;
    check(!policyValid(policy), "budget below buffer cap rejected");
    policy = syntheticPolicy();

    policy.minAlignment = 255;
    check(!policyValid(policy), "non-power-of-two min alignment rejected");
    policy = syntheticPolicy();

    policy.maxAlignment = 128;
    check(!policyValid(policy), "max alignment below min rejected");
    policy = syntheticPolicy();

    policy.maxOpenHandles = kHandleCapacity + 1;
    check(!policyValid(policy), "handle cap above table capacity rejected");
}

void testComputeGate() {
    IntegrationReadiness allMissing;
    check(computeGate(allMissing) == ClientError::GateClosedPciTransport,
          "gate names the first missing layer");

    IntegrationReadiness noFirmware;
    noFirmware.pciTransport = IntegrationState::HardwareVerified;
    check(computeGate(noFirmware) == ClientError::GateClosedFirmware,
          "gate requires the firmware layer");

    IntegrationReadiness noMemory = noFirmware;
    noMemory.firmware = IntegrationState::HardwareVerified;
    check(computeGate(noMemory) == ClientError::GateClosedMemory,
          "gate requires the memory layer");

    IntegrationReadiness noQueue = noMemory;
    noQueue.memory = IntegrationState::HardwareVerified;
    check(computeGate(noQueue) == ClientError::GateClosedQueueSubmission,
          "gate requires the queue-submission layer");

    IntegrationReadiness computeReady = noQueue;
    computeReady.queueSubmission = IntegrationState::HardwareVerified;
    // The display, IOAccelerator, and Metal layers stay Missing: the
    // compute-first gate must not demand them.
    check(computeReady.ioAccelerator == IntegrationState::Missing &&
              computeReady.metal == IntegrationState::Missing &&
              computeReady.display == IntegrationState::Missing,
          "upper layers stay unset in the compute model");
    check(computeGate(computeReady) == ClientError::None,
          "compute gate opens without display/Metal layers");

    // Prototype states never satisfy the gate.
    computeReady.queueSubmission = IntegrationState::Prototype;
    check(computeGate(computeReady) == ClientError::GateClosedQueueSubmission,
          "prototype states never satisfy the gate");
}

void testEvidenceMapping() {
    BringupEvidenceFlags flags;
    IntegrationReadiness readiness = readinessFromBringupEvidence(flags);
    check(computeGate(readiness) == ClientError::GateClosedPciTransport,
          "no bring-up evidence: gate closed at the transport layer");

    flags.bringupStarted = true;
    readiness = readinessFromBringupEvidence(flags);
    check(computeGate(readiness) == ClientError::GateClosedFirmware,
          "bring-up started but no firmware evidence: gate closed at firmware");

    flags.firmwareVerified = true;
    flags.memoryVerified = true;
    flags.queueVerified = true;
    readiness = readinessFromBringupEvidence(flags);
    check(computeGate(readiness) == ClientError::None,
          "full bring-up evidence opens the compute gate");
}

void testBudgetLifecycle() {
    ClientBudget budget(syntheticPolicy());

    uint32_t handleA = 0;
    check(budget.allocate(1ull << 20, 256, &handleA) == ClientError::None,
          "first allocation succeeds");
    check(handleA != ClientBudget::kInvalidHandle, "allocation returns a handle");
    check(budget.isOpen(handleA), "new handle is open");
    check(budget.allocatedBytes() == 1ull << 20, "budget accounts the allocation");

    check(budget.allocate(0, 256, &handleA) == ClientError::ByteCountTooLarge,
          "zero-byte allocation rejected");
    check(budget.allocate((1ull << 30) + 1, 256, &handleA) ==
              ClientError::ByteCountTooLarge,
          "allocation above the buffer cap rejected");
    check(budget.allocate(1ull << 20, 128, &handleA) == ClientError::InvalidAlignment,
          "alignment below the policy minimum rejected");
    check(budget.allocate(1ull << 20, 8192, &handleA) == ClientError::InvalidAlignment,
          "alignment above the policy maximum rejected");
    check(budget.allocate(1ull << 20, 300, &handleA) == ClientError::InvalidAlignment,
          "non-power-of-two alignment rejected");

    check(budget.release(handleA) == ClientError::None, "release succeeds");
    check(!budget.isOpen(handleA), "released handle is closed");
    check(budget.release(handleA) == ClientError::HandleNotOpen,
          "double release rejected");
    check(budget.allocatedBytes() == 0, "budget released");

    check(budget.release(ClientBudget::kInvalidHandle) == ClientError::HandleNotOpen,
          "invalid sentinel handle rejected");
}

void testStaleHandleAfterRecycle() {
    ClientBudget budget(syntheticPolicy());
    uint32_t first = 0;
    check(budget.allocate(1ull << 20, 256, &first) == ClientError::None, "alloc #1");
    const uint32_t stale = first;
    check(budget.release(stale) == ClientError::None, "release #1");

    // The next allocation reuses the same slot with a bumped generation: the
    // stale handle must not address it.
    uint32_t second = 0;
    check(budget.allocate(2ull << 20, 256, &second) == ClientError::None, "alloc #2");
    check(ClientBudget::slotOf(second) == ClientBudget::slotOf(stale),
          "the slot was recycled");
    check(second != stale, "the handle changed");
    check(!budget.isOpen(stale), "stale handle is not open");
    check(budget.isOpen(second), "the recycled slot is reachable via the new handle");
    check(budget.release(stale) == ClientError::HandleNotOpen,
          "releasing a stale handle is rejected");
    check(budget.release(second) == ClientError::None,
          "releasing the current handle works");
}

void testBudgetAndTableLimits() {
    ClientPolicy policy = syntheticPolicy();
    policy.maxBufferBytes = 2ull << 20;
    policy.maxClientBytes = 2ull << 20;
    ClientBudget budget(policy);

    uint32_t handle = 0;
    check(budget.allocate(1ull << 20, 256, &handle) == ClientError::None, "alloc 1 MiB");
    check(budget.allocate(2ull << 20, 256, &handle) ==
              ClientError::ClientBudgetExhausted,
          "allocation above the remaining budget rejected");

    ClientPolicy tiny = syntheticPolicy();
    tiny.maxOpenHandles = 2;
    ClientBudget fewHandles(tiny);
    uint32_t h1 = 0, h2 = 0, h3 = 0;
    check(fewHandles.allocate(256, 256, &h1) == ClientError::None, "handle 1");
    check(fewHandles.allocate(256, 256, &h2) == ClientError::None, "handle 2");
    check(fewHandles.allocate(256, 256, &h3) == ClientError::HandleTableFull,
          "third allocation rejected at the handle cap");
    check(fewHandles.release(h1) == ClientError::None, "release handle 1");
    check(fewHandles.allocate(256, 256, &h3) == ClientError::None,
          "a freed slot becomes allocatable again");
}

}  // namespace

int main() {
    testPolicyValidation();
    testComputeGate();
    testEvidenceMapping();
    testBudgetLifecycle();
    testStaleHandleAfterRecycle();
    testBudgetAndTableLimits();

    if (failures != 0) {
        fprintf(stderr, "client contract tests: %d failure(s)\n", failures);
        return 1;
    }
    puts("Client contract tests: PASS (policy, gate, evidence mapping, budget, "
         "handle lifecycle, stale handles, limits)");
    return 0;
}
