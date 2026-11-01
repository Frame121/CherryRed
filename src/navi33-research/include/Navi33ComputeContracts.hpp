#ifndef COBALT64_NAVI33_COMPUTE_CONTRACTS_HPP
#define COBALT64_NAVI33_COMPUTE_CONTRACTS_HPP
#include <cstdint>
namespace cobalt64::research {
enum class ComputeEvidenceState : std::uint8_t {
    Unknown, GenericUpstream, BoardAttributed, HardwareVerified
};
enum class ComputeEngine : std::uint8_t { Gfx, Sdma };
struct ComputeAperture { std::uint64_t base = 0; std::uint64_t size = 0; };
struct ComputeQueueGeometry {
    std::uint32_t ringBytes = 0;
    std::uint32_t entryBytes = 0;
    std::uint32_t alignment = 0;
    std::uint32_t maxInFlight = 0;
};
struct ComputeIbPolicy {
    std::uint32_t addressAlignment = 0;
    std::uint32_t sizeAlignment = 0;
    std::uint32_t minimumBytes = 0;
    std::uint32_t maximumBytes = 0;
};
struct ComputeSubmission {
    ComputeEngine engine = ComputeEngine::Gfx;
    std::uint64_t ibAddress = 0;
    std::uint32_t ibBytes = 0;
    std::uint64_t sequence = 0;
};
enum class ComputeContractError : std::uint8_t {
    None, EvidenceNotTargeted, InvalidQueueGeometry, InvalidIbPolicy,
    EmptyIb, IbTooLarge, MisalignedIb, InvalidIbSize,
    AddressOverflow, OutsideAperture, InvalidSequence
};
struct ComputeContractResult {
    bool valid = false;
    ComputeContractError error = ComputeContractError::None;
};
ComputeContractResult validateComputeQueue(
    const ComputeQueueGeometry& geometry,
    ComputeEvidenceState evidence) noexcept;
ComputeContractResult validateComputeSubmission(
    const ComputeAperture& aperture, const ComputeQueueGeometry& queue,
    const ComputeIbPolicy& policy, const ComputeSubmission& submission,
    std::uint64_t lastSubmittedSequence,
    ComputeEvidenceState evidence) noexcept;
// Host-only validation. This never writes a ring, rings a doorbell, maps GPU
// memory, or submits an IB to a physical GFX/SDMA engine.
}
#endif
