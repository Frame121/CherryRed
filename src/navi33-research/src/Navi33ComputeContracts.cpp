#include "Navi33ComputeContracts.hpp"
#include <limits>
namespace cobalt64::research {
namespace {
bool powerOfTwo(std::uint64_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}
bool targetEvidence(ComputeEvidenceState evidence) noexcept {
    return evidence == ComputeEvidenceState::BoardAttributed ||
           evidence == ComputeEvidenceState::HardwareVerified;
}
bool rangeContains(const ComputeAperture& aperture,
                   std::uint64_t address, std::uint64_t bytes) noexcept {
    if (aperture.size == 0 || bytes == 0) return false;
    if (aperture.base > std::numeric_limits<std::uint64_t>::max() - aperture.size ||
        address > std::numeric_limits<std::uint64_t>::max() - bytes) return false;
    return address >= aperture.base &&
           address + bytes <= aperture.base + aperture.size;
}
}
ComputeContractResult validateComputeQueue(
    const ComputeQueueGeometry& geometry,
    ComputeEvidenceState evidence) noexcept {
    if (!targetEvidence(evidence))
        return {false, ComputeContractError::EvidenceNotTargeted};
    if (geometry.ringBytes == 0 || geometry.entryBytes == 0 ||
        geometry.ringBytes % geometry.entryBytes != 0 ||
        !powerOfTwo(geometry.alignment) ||
        geometry.alignment < geometry.entryBytes ||
        geometry.maxInFlight == 0 ||
        geometry.maxInFlight > geometry.ringBytes / geometry.entryBytes)
        return {false, ComputeContractError::InvalidQueueGeometry};
    return {true, ComputeContractError::None};
}
ComputeContractResult validateComputeSubmission(
    const ComputeAperture& aperture, const ComputeQueueGeometry& queue,
    const ComputeIbPolicy& policy, const ComputeSubmission& submission,
    std::uint64_t lastSubmittedSequence,
    ComputeEvidenceState evidence) noexcept {
    if (!targetEvidence(evidence))
        return {false, ComputeContractError::EvidenceNotTargeted};
    if (!validateComputeQueue(queue, evidence).valid)
        return {false, ComputeContractError::InvalidQueueGeometry};
    if (!powerOfTwo(policy.addressAlignment) ||
        !powerOfTwo(policy.sizeAlignment) ||
        policy.minimumBytes == 0 ||
        policy.maximumBytes < policy.minimumBytes)
        return {false, ComputeContractError::InvalidIbPolicy};
    if (submission.ibBytes == 0)
        return {false, ComputeContractError::EmptyIb};
    if (submission.ibBytes < policy.minimumBytes ||
        submission.ibBytes > policy.maximumBytes)
        return {false, ComputeContractError::IbTooLarge};
    if ((submission.ibAddress & (policy.addressAlignment - 1)) != 0)
        return {false, ComputeContractError::MisalignedIb};
    if ((submission.ibBytes & (policy.sizeAlignment - 1)) != 0)
        return {false, ComputeContractError::InvalidIbSize};
    if (submission.sequence == 0 || submission.sequence <= lastSubmittedSequence)
        return {false, ComputeContractError::InvalidSequence};
    if (aperture.base > std::numeric_limits<std::uint64_t>::max() - aperture.size ||
        submission.ibAddress > std::numeric_limits<std::uint64_t>::max() - submission.ibBytes)
        return {false, ComputeContractError::AddressOverflow};
    if (!rangeContains(aperture, submission.ibAddress, submission.ibBytes))
        return {false, ComputeContractError::OutsideAperture};
    return {true, ComputeContractError::None};
}
}  // namespace cobalt64::research
