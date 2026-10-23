#pragma once

#include "Firmware.hpp"
#include "Discovery.hpp"
#include "VramArena.hpp"

namespace cherryred::navi33 {

enum class FirmwareKind { Pfp, Me, Mec, Rlc, Sdma, PspToc, PspTa, Smu, Dmub, Mes, Count };
struct FirmwareExpectation {
    uint16_t headerMajor = 0, headerMinor = 0;
    uint16_t ipMajor = 0, ipMinor = 0;
};
struct FirmwareInput {
    FirmwareKind kind = FirmwareKind::Count;
    const uint8_t* bytes = nullptr;
    size_t size = 0;
    FirmwareExpectation expected{};
};
struct PreparedFirmware {
    FirmwareKind kind = FirmwareKind::Count;
    FirmwareInfo info{};
    Allocation staging{};
    DiscoveryRecord discovery{};
    bool hasDiscoveryRecord = false;
};
// An explicit caller-supplied requirement for each firmware input, in the
// same order. No firmware-role/IP mapping is inferred from the PCI ID.
struct DiscoveryRequirement {
    uint16_t hwId = 0;
    uint8_t instance = 0;
    uint8_t major = 0, minor = 0, revision = 0, attributes = 0;
};
enum class PreparationState { New, Identified, Prepared, Stopped };
enum class PreparationError {
    None, InvalidState, WrongDevice, InvalidRequest, DuplicateFirmware,
    FirmwareEnvelope, FirmwareMismatch, Memory, HardwareBackendUnavailable,
    DiscoveryInvalid, MissingIp, IpMismatch
};
struct PreparationResult {
    PreparationError error = PreparationError::None;
    size_t firmwareIndex = 0; // meaningful for firmware validation/allocation failures
    FirmwareError firmwareError = FirmwareError::None;
    MemoryError memoryError = MemoryError::None;
    DiscoveryError discoveryError = DiscoveryError::None;
};

// Software preparation transaction. Copies metadata only, never firmware bytes.
// All addresses, exclusions, expected versions, and the requested firmware set
// come from the caller. No defaults imply a measured Navi33 hardware layout.
// This is not a hardware backend and cannot transition to a running GPU.
class Preparation {
public:
    static constexpr size_t kMaxFirmware = static_cast<size_t>(FirmwareKind::Count);
    Preparation() = default;
    ~Preparation() noexcept { stop(); }
    Preparation(const Preparation&) = delete;
    Preparation& operator=(const Preparation&) = delete;
    Preparation(Preparation&&) = delete;
    Preparation& operator=(Preparation&&) = delete;

    PreparationResult identify(uint16_t vendor, uint16_t device) noexcept;
    PreparationResult prepare(AddressRange aperture, const AddressRange* excluded,
                              size_t excludedCount, const FirmwareInput* firmware,
                              size_t count, uint64_t alignment) noexcept;
    // Additive evidence-aware path. Parses a raw IP Discovery binary and
    // compares full IP tuples before allocating; snapshots matched records.
    // Caller bytes are read synchronously and never retained. Matching proves
    // agreement with the input artifact, not physical-board attribution.
    PreparationResult prepareFromDiscovery(
        const uint8_t* discoveryBytes, size_t discoverySize,
        const DiscoveryRequirement* requirements,
        AddressRange aperture, const AddressRange* excluded, size_t excludedCount,
        const FirmwareInput* firmware, size_t count, uint64_t alignment) noexcept;
    PreparationResult start() const noexcept;
    void stop() noexcept;
    PreparationState state() const noexcept { return state_; }
    size_t firmwareCount() const noexcept { return count_; }
    const PreparedFirmware* firmware(size_t index) const noexcept {
        return index < count_ ? &firmware_[index] : nullptr;
    }
    size_t liveAllocations() const noexcept { return arena_.liveAllocations(); }

private:
    void releaseStaging() noexcept;
    PreparationState state_ = PreparationState::New;
    VramArena arena_{};
    PreparedFirmware firmware_[kMaxFirmware]{};
    size_t count_ = 0;
};
}  // namespace cherryred::navi33
