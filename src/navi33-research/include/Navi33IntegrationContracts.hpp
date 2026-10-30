#ifndef COBALT64_NAVI33_INTEGRATION_CONTRACTS_HPP
#define COBALT64_NAVI33_INTEGRATION_CONTRACTS_HPP

#include <cstdint>

namespace cobalt64::research {

enum class IntegrationLayer : std::uint8_t {
    PciTransport, Firmware, Memory, QueueSubmission,
    IoAccelerator, Metal, Display
};

enum class IntegrationState : std::uint8_t {
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

enum class ReadinessError : std::uint8_t {
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
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t refreshMilliHz = 0;
    std::uint32_t bitsPerPixel = 0;
};

enum class DisplayContractError : std::uint8_t {
    None, ZeroDimension, DimensionTooLarge, InvalidRefresh,
    UnsupportedPixelFormat, ArithmeticOverflow, InvalidPitch
};

struct DisplayLayout {
    std::uint64_t rowBytes = 0;
    std::uint64_t frameBytes = 0;
};

struct DisplayLayoutResult {
    bool valid = false;
    DisplayContractError error = DisplayContractError::None;
    DisplayLayout layout{};
};

DisplayLayoutResult calculateLinearDisplayLayout(
    const DisplayMode& mode, std::uint32_t rowAlignment) noexcept;

class FenceTimelineModel {
public:
    bool signal(std::uint64_t value) noexcept;
    bool isComplete(std::uint64_t value) const noexcept;
    std::uint64_t completedValue() const noexcept;

private:
    std::uint64_t completed_ = 0;
};

}  // namespace cobalt64::research

#endif
