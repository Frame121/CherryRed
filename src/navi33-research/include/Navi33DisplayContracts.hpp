#ifndef COBALT64_NAVI33_DISPLAY_CONTRACTS_HPP
#define COBALT64_NAVI33_DISPLAY_CONTRACTS_HPP

#include <cstddef>
#include <cstdint>

namespace cobalt64::research {

constexpr std::size_t kEdidBlockBytes = 128;

enum class DisplayEvidenceState : std::uint8_t {
    Unknown, GenericUpstream, BoardAttributed, HardwareVerified
};

struct EdidValidation {
    bool valid = false;
    bool hasBaseHeader = false;
    bool checksumValid = false;
};

EdidValidation validateEdidBaseBlock(
    const std::uint8_t* bytes, std::size_t length) noexcept;

struct ScanoutAperture {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
};

struct ScanoutBuffer {
    std::uint64_t address = 0;
    std::uint64_t length = 0;
    std::uint32_t pitchBytes = 0;
    std::uint32_t height = 0;
};

enum class ScanoutError : std::uint8_t {
    None, UnknownEvidence, EmptyAperture, EmptyBuffer, InvalidPitch,
    InvalidHeight, ArithmeticOverflow, OutsideAperture
};

struct ScanoutValidation {
    bool valid = false;
    ScanoutError error = ScanoutError::None;
};

ScanoutValidation validateScanoutBuffer(
    const ScanoutAperture& aperture,
    const ScanoutBuffer& buffer,
    DisplayEvidenceState evidence) noexcept;

enum class FlipError : std::uint8_t {
    None, AlreadyPending, InvalidSequence, InvalidBuffer, NoPendingFlip,
    StaleCompletion, OutOfOrderCompletion
};

struct FlipResult {
    bool accepted = false;
    FlipError error = FlipError::None;
};

// Host-side state model only. It does not program a display plane or wait for
// a hardware vblank interrupt.
class DisplayFlipTimelineModel {
public:
    FlipResult submit(std::uint64_t sequence,
                      const ScanoutBuffer& buffer,
                      const ScanoutAperture& aperture,
                      DisplayEvidenceState evidence) noexcept;
    FlipResult completeAtVblank(std::uint64_t sequence) noexcept;
    bool hasPendingFlip() const noexcept;
    std::uint64_t completedSequence() const noexcept;
    std::uint64_t activeAddress() const noexcept;

private:
    bool pending_ = false;
    std::uint64_t pendingSequence_ = 0;
    std::uint64_t pendingAddress_ = 0;
    std::uint64_t completedSequence_ = 0;
    std::uint64_t activeAddress_ = 0;
};

}  // namespace cobalt64::research

#endif
