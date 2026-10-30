#include "Navi33ComputeContracts.hpp"
#include <cstdio>
#include <limits>
using namespace cobalt64::research;
namespace {
int checks = 0;
int failures = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
void queueTests() {
    const ComputeQueueGeometry q{65536, 4, 256, 1024};
    check(validateComputeQueue(q, ComputeEvidenceState::BoardAttributed).valid,
          "board-attributed queue geometry accepted");
    check(validateComputeQueue(q, ComputeEvidenceState::GenericUpstream).error ==
          ComputeContractError::EvidenceNotTargeted,
          "generic upstream evidence cannot authorize target queue");
    auto bad = q; bad.ringBytes = 0;
    check(validateComputeQueue(bad, ComputeEvidenceState::HardwareVerified).error ==
          ComputeContractError::InvalidQueueGeometry, "zero ring rejected");
    bad = q; bad.ringBytes = 65535;
    check(!validateComputeQueue(bad, ComputeEvidenceState::HardwareVerified).valid,
          "non-divisible ring rejected");
    bad = q; bad.alignment = 3;
    check(!validateComputeQueue(bad, ComputeEvidenceState::HardwareVerified).valid,
          "non-power-of-two alignment rejected");
    bad = q; bad.maxInFlight = 20000;
    check(!validateComputeQueue(bad, ComputeEvidenceState::HardwareVerified).valid,
          "in-flight limit beyond ring capacity rejected");
}
void submissionTests() {
    const ComputeAperture aperture{0x100000, 0x100000};
    const ComputeQueueGeometry queue{65536, 4, 256, 1024};
    // Synthetic policy values for contract tests only; not asserted as GFX11
    // hardware alignment or packet limits.
    const ComputeIbPolicy policy{256, 4, 16, 65536};
    const ComputeSubmission ib{ComputeEngine::Gfx, 0x110000, 256, 1};
    check(validateComputeSubmission(aperture, queue, policy, ib, 0,
          ComputeEvidenceState::BoardAttributed).valid, "valid synthetic IB accepted");
    check(validateComputeSubmission(aperture, queue, policy, ib, 0,
          ComputeEvidenceState::GenericUpstream).error ==
          ComputeContractError::EvidenceNotTargeted, "unattributed evidence rejected");
    auto bad = ib; bad.ibBytes = 0;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::EmptyIb,
          "empty IB rejected");
    bad = ib; bad.ibBytes = 8;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::IbTooLarge,
          "IB below policy minimum rejected");
    bad = ib; bad.ibBytes = 65540;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::IbTooLarge,
          "IB above policy maximum rejected");
    bad = ib; bad.ibAddress += 4;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::MisalignedIb,
          "misaligned IB address rejected");
    bad = ib; bad.ibBytes = 258;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::InvalidIbSize,
          "misaligned IB byte length rejected");
    bad = ib; bad.sequence = 0;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::InvalidSequence,
          "zero sequence rejected");
    check(validateComputeSubmission(aperture, queue, policy, ib, 1,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::InvalidSequence,
          "replayed sequence rejected");
    bad = ib; bad.ibAddress = 0x1fff00; bad.ibBytes = 512;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::OutsideAperture,
          "IB outside aperture rejected");
    bad = ib; bad.ibAddress = std::numeric_limits<std::uint64_t>::max() - 255;
    check(validateComputeSubmission(aperture, queue, policy, bad, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::AddressOverflow,
          "IB address overflow rejected");
    const ComputeIbPolicy invalid{0, 4, 16, 65536};
    check(validateComputeSubmission(aperture, queue, invalid, ib, 0,
          ComputeEvidenceState::HardwareVerified).error == ComputeContractError::InvalidIbPolicy,
          "missing target policy rejected");
}
}
int main() {
    queueTests(); submissionTests();
    if (failures) {
        std::fprintf(stderr, "%d/%d compute contract checks failed\n", failures, checks);
        return 1;
    }
    std::printf("Navi33 compute contract checks: %d passed, 0 failed\n", checks);
    return 0;
}
