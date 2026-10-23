#include "Preparation.hpp"

namespace cherryred::navi33 {
PreparationResult Preparation::identify(uint16_t vendor, uint16_t device) noexcept {
    if (state_ != PreparationState::New) return {PreparationError::InvalidState};
    if (vendor != 0x1002 || device != 0x7480) return {PreparationError::WrongDevice};
    state_ = PreparationState::Identified;
    return {};
}

PreparationResult Preparation::prepare(AddressRange aperture, const AddressRange* excluded,
                                        size_t excludedCount, const FirmwareInput* inputs,
                                        size_t count, uint64_t alignment) noexcept {
    if (state_ != PreparationState::Identified) return {PreparationError::InvalidState};
    if (!inputs || !count || count > kMaxFirmware)
        return {PreparationError::InvalidRequest};
    FirmwareInfo metadata[kMaxFirmware]{};
    for (size_t i = 0; i < count; ++i) {
        const auto& input = inputs[i];
        if (static_cast<size_t>(input.kind) >= kMaxFirmware)
            return {PreparationError::InvalidRequest, i};
        for (size_t j = 0; j < i; ++j)
            if (input.kind == inputs[j].kind) return {PreparationError::DuplicateFirmware, i};
        const auto error = inspectFirmware(input.bytes, input.size, metadata[i]);
        if (error != FirmwareError::None)
            return {PreparationError::FirmwareEnvelope, i, error};
        const auto& info = metadata[i];
        const auto& expected = input.expected;
        if (info.headerMajor != expected.headerMajor || info.headerMinor != expected.headerMinor ||
            info.ipMajor != expected.ipMajor || info.ipMinor != expected.ipMinor)
            return {PreparationError::FirmwareMismatch, i};
    }
    auto memoryError = arena_.configure(aperture, excluded, excludedCount);
    if (memoryError != MemoryError::None)
        return {PreparationError::Memory, 0, FirmwareError::None, memoryError};
    for (size_t i = 0; i < count; ++i) {
        Allocation allocation;
        memoryError = arena_.allocate(metadata[i].fileSize, alignment, allocation);
        if (memoryError != MemoryError::None) {
            releaseStaging();
            return {PreparationError::Memory, i, FirmwareError::None, memoryError};
        }
        firmware_[count_++] = {inputs[i].kind, metadata[i], allocation};
    }
    state_ = PreparationState::Prepared;
    return {};
}

PreparationResult Preparation::start() const noexcept {
    if (state_ != PreparationState::Prepared) return {PreparationError::InvalidState};
    // Parsed envelopes and planned addresses do not authorize register writes.
    return {PreparationError::HardwareBackendUnavailable};
}

void Preparation::releaseStaging() noexcept {
    while (count_) {
        auto& item = firmware_[--count_];
        (void)arena_.release(item.staging);
        item = {};
    }
}

void Preparation::stop() noexcept {
    releaseStaging();
    state_ = PreparationState::Stopped;
}
}  // namespace cherryred::navi33
