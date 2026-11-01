#include "Navi33DisplayContracts.hpp"
#include <limits>
namespace cobalt64::research {
namespace {
constexpr std::uint8_t kEdidHeader[8] = {0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00};
}
EdidValidation validateEdidBaseBlock(const std::uint8_t* bytes, std::size_t length) noexcept {
    if (bytes == nullptr || length < kEdidBlockBytes) return {};
    bool header = true;
    for (std::size_t i=0; i<8; ++i) header = header && bytes[i] == kEdidHeader[i];
    std::uint8_t sum = 0;
    for (std::size_t i=0; i<kEdidBlockBytes; ++i) sum = static_cast<std::uint8_t>(sum + bytes[i]);
    const bool checksum = sum == 0;
    return {header && checksum, header, checksum};
}
ScanoutValidation validateScanoutBuffer(const ScanoutAperture& aperture, const ScanoutBuffer& buffer, DisplayEvidenceState evidence) noexcept {
    if (evidence != DisplayEvidenceState::BoardAttributed && evidence != DisplayEvidenceState::HardwareVerified) return {false, ScanoutError::UnknownEvidence};
    if (aperture.size == 0) return {false, ScanoutError::EmptyAperture};
    if (buffer.length == 0) return {false, ScanoutError::EmptyBuffer};
    if (buffer.pitchBytes == 0) return {false, ScanoutError::InvalidPitch};
    if (buffer.height == 0) return {false, ScanoutError::InvalidHeight};
    if (buffer.height > std::numeric_limits<std::uint64_t>::max() / buffer.pitchBytes) return {false, ScanoutError::ArithmeticOverflow};
    const std::uint64_t required = std::uint64_t(buffer.pitchBytes) * buffer.height;
    if (required > buffer.length) return {false, ScanoutError::EmptyBuffer};
    if (aperture.base > std::numeric_limits<std::uint64_t>::max() - aperture.size || buffer.address > std::numeric_limits<std::uint64_t>::max() - buffer.length) return {false, ScanoutError::ArithmeticOverflow};
    const std::uint64_t apertureEnd = aperture.base + aperture.size;
    const std::uint64_t bufferEnd = buffer.address + buffer.length;
    if (buffer.address < aperture.base || bufferEnd > apertureEnd) return {false, ScanoutError::OutsideAperture};
    return {true, ScanoutError::None};
}
FlipResult DisplayFlipTimelineModel::submit(std::uint64_t sequence, const ScanoutBuffer& buffer, const ScanoutAperture& aperture, DisplayEvidenceState evidence) noexcept {
    if (pending_) return {false, FlipError::AlreadyPending};
    if (sequence == 0 || sequence <= completedSequence_) return {false, FlipError::InvalidSequence};
    if (!validateScanoutBuffer(aperture, buffer, evidence).valid) return {false, FlipError::InvalidBuffer};
    pending_ = true;
    pendingSequence_ = sequence;
    pendingAddress_ = buffer.address;
    return {true, FlipError::None};
}
FlipResult DisplayFlipTimelineModel::completeAtVblank(std::uint64_t sequence) noexcept {
    if (!pending_) return {false, FlipError::NoPendingFlip};
    if (sequence < pendingSequence_) return {false, FlipError::StaleCompletion};
    if (sequence > pendingSequence_) return {false, FlipError::OutOfOrderCompletion};
    completedSequence_ = pendingSequence_;
    activeAddress_ = pendingAddress_;
    pendingSequence_ = 0;
    pendingAddress_ = 0;
    pending_ = false;
    return {true, FlipError::None};
}
bool DisplayFlipTimelineModel::hasPendingFlip() const noexcept { return pending_; }
std::uint64_t DisplayFlipTimelineModel::completedSequence() const noexcept { return completedSequence_; }
std::uint64_t DisplayFlipTimelineModel::activeAddress() const noexcept { return activeAddress_; }
}  // namespace cobalt64::research
