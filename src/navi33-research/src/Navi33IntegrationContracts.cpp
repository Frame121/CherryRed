#include "Navi33BuildConfiguration.hpp"
#include "Navi33IntegrationContracts.hpp"

#include <limits>

namespace cobalt64::research {
namespace {
bool powerOfTwo(std::uint64_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}
bool verified(IntegrationState state) noexcept {
    return state == IntegrationState::HardwareVerified;
}
}

ReadinessResult validateFullGraphicsReadiness(
    const IntegrationReadiness& r) noexcept {
    if (!verified(r.pciTransport))
        return {false, ReadinessError::MissingPciTransport};
    if (!verified(r.firmware))
        return {false, ReadinessError::MissingFirmware};
    if (!verified(r.memory))
        return {false, ReadinessError::MissingMemory};
    if (!verified(r.queueSubmission))
        return {false, ReadinessError::MissingQueueSubmission};
    if (!verified(r.ioAccelerator))
        return {false, ReadinessError::MissingIoAccelerator};
    if (!verified(r.metal))
        return {false, ReadinessError::MissingMetal};
    if (!verified(r.display))
        return {false, ReadinessError::MissingDisplay};
    return {true, ReadinessError::None};
}

DisplayLayoutResult calculateLinearDisplayLayout(
    const DisplayMode& mode, std::uint32_t rowAlignment) noexcept {
    if (mode.width == 0 || mode.height == 0)
        return {false, DisplayContractError::ZeroDimension, {}};
    if (mode.width > 16384 || mode.height > 16384)
        return {false, DisplayContractError::DimensionTooLarge, {}};
    if (mode.refreshMilliHz < 1000 || mode.refreshMilliHz > 1000000)
        return {false, DisplayContractError::InvalidRefresh, {}};
    if (mode.bitsPerPixel != 24 && mode.bitsPerPixel != 32)
        return {false, DisplayContractError::UnsupportedPixelFormat, {}};
    if (!powerOfTwo(rowAlignment) || rowAlignment > 4096)
        return {false, DisplayContractError::InvalidPitch, {}};

    const std::uint64_t bytesPerPixel = mode.bitsPerPixel / 8;
    if (mode.width > std::numeric_limits<std::uint64_t>::max() / bytesPerPixel)
        return {false, DisplayContractError::ArithmeticOverflow, {}};
    const std::uint64_t rawRow = std::uint64_t(mode.width) * bytesPerPixel;
    if (rawRow > std::numeric_limits<std::uint64_t>::max() - (rowAlignment - 1))
        return {false, DisplayContractError::ArithmeticOverflow, {}};
    const std::uint64_t row =
        (rawRow + rowAlignment - 1) & ~std::uint64_t(rowAlignment - 1);
    if (mode.height > std::numeric_limits<std::uint64_t>::max() / row)
        return {false, DisplayContractError::ArithmeticOverflow, {}};
    return {true, DisplayContractError::None, {row, row * mode.height}};
}

bool FenceTimelineModel::signal(std::uint64_t value) noexcept {
    if (value <= completed_) return false;
    completed_ = value;
    return true;
}
bool FenceTimelineModel::isComplete(std::uint64_t value) const noexcept {
    return value <= completed_;
}
std::uint64_t FenceTimelineModel::completedValue() const noexcept {
    return completed_;
}

}  // namespace cobalt64::research
