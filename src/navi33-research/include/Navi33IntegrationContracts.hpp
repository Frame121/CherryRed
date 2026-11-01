#ifndef COBALT64_NAVI33_INTEGRATION_CONTRACTS_HPP
#define COBALT64_NAVI33_INTEGRATION_CONTRACTS_HPP

#include <stdint.h> // freestanding-safe: the kext includes this header with -nostdinc

namespace cobalt64::research {

enum class IntegrationLayer : uint8_t {
    PciTransport, Firmware, Memory, QueueSubmission,
    IoAccelerator, Metal, Display
};

enum class IntegrationState : uint8_t {
    Missing, Prototype, HostModelOnly, ImplementedUnverified, HardwareVerified
};

struct IntegrationReadiness {
    IntegrationState pciTransport = IntegrationState::Missing;
    IntegrationState firmware = IntegrationState::Missing;
    IntegrationState memory = IntegrationState::Missing;
    IntegrationState queueSubmission = IntegrationState::Missing;
    IntegrationState ioAccelerator = IntegrationState::Missing;
    IntegrationState metal = IntegrationState::Missing;
    IntegrationState display = IntegrationState::Missing;
};

enum class ReadinessError : uint8_t {
    None, MissingPciTransport, MissingFirmware, MissingMemory,
    MissingQueueSubmission, MissingIoAccelerator, MissingMetal,
    MissingDisplay, HardwareNotVerified
};

struct ReadinessResult {
    bool ready = false;
    ReadinessError error = ReadinessError::None;
};

// Strictly reports whether all layers are hardware-verified. Host models and
// prototypes never satisfy a production readiness gate.
ReadinessResult validateFullGraphicsReadiness(
    const IntegrationReadiness& readiness) noexcept;

struct DisplayMode {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refreshMilliHz = 0;
    uint32_t bitsPerPixel = 0;
};

enum class DisplayContractError : uint8_t {
    None, ZeroDimension, DimensionTooLarge, InvalidRefresh,
    UnsupportedPixelFormat, ArithmeticOverflow, InvalidPitch
};

struct DisplayLayout {
    uint64_t rowBytes = 0;
    uint64_t frameBytes = 0;
};

struct DisplayLayoutResult {
    bool valid = false;
    DisplayContractError error = DisplayContractError::None;
    DisplayLayout layout{};
};

DisplayLayoutResult calculateLinearDisplayLayout(
    const DisplayMode& mode, uint32_t rowAlignment) noexcept;

class FenceTimelineModel {
public:
    bool signal(uint64_t value) noexcept;
    bool isComplete(uint64_t value) const noexcept;
    uint64_t completedValue() const noexcept;

private:
    uint64_t completed_ = 0;
};

}  // namespace cobalt64::research

#endif
