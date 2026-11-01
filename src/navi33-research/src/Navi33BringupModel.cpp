#include "Navi33BuildConfiguration.hpp"
#include "Navi33BringupModel.hpp"

#include <limits>

namespace cobalt64::research {
namespace {

bool isPowerOfTwo(std::uint64_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

bool isHexDigest(std::string_view digest) noexcept {
    if (digest.size() != 64) return false;
    for (const char value : digest) {
        const bool digit = value >= '0' && value <= '9';
        const bool lower = value >= 'a' && value <= 'f';
        const bool upper = value >= 'A' && value <= 'F';
        if (!digit && !lower && !upper) return false;
    }
    return true;
}

}  // namespace

ContractResult validateFirmwareDescriptor(
    const FirmwareDescriptor& descriptor) noexcept {
    if (descriptor.filename.empty()) return {false, ContractError::MissingName};
    constexpr std::size_t kMaximumFirmwareBytes = 64U * 1024U * 1024U;
    if (descriptor.byteLength == 0 ||
        descriptor.byteLength > kMaximumFirmwareBytes) {
        return {false, ContractError::InvalidSize};
    }
    if (!isHexDigest(descriptor.sha256)) {
        return {false, ContractError::InvalidDigest};
    }
    if (descriptor.provenance == FirmwareProvenance::Unknown) {
        return {false, ContractError::UnknownProvenance};
    }
    if (descriptor.license != FirmwareLicense::Approved) {
        return {false, ContractError::LicenseNotApproved};
    }
    return {true, ContractError::None};
}

bool containsRange(const GpuAddressRange& outer,
                   const GpuAddressRange& inner) noexcept {
    if (outer.length == 0 || inner.length == 0) return false;
    if (outer.base > std::numeric_limits<std::uint64_t>::max() - outer.length ||
        inner.base > std::numeric_limits<std::uint64_t>::max() - inner.length) {
        return false;
    }
    const std::uint64_t outerEnd = outer.base + outer.length;
    const std::uint64_t innerEnd = inner.base + inner.length;
    return inner.base >= outer.base && innerEnd <= outerEnd;
}

GpuVirtualAddressAllocator::GpuVirtualAddressAllocator(
    GpuAddressRange aperture) noexcept
    : aperture_(aperture), next_(aperture.base) {}

ContractResult GpuVirtualAddressAllocator::allocate(
    std::uint64_t bytes, std::uint64_t alignment,
    GpuAddressRange& result) noexcept {
    result = {};
    if (!isPowerOfTwo(alignment)) {
        return {false, ContractError::InvalidAlignment};
    }
    if (bytes == 0 || aperture_.length == 0 ||
        aperture_.base > std::numeric_limits<std::uint64_t>::max() -
                             aperture_.length) {
        return {false, ContractError::InvalidSize};
    }
    if (next_ > std::numeric_limits<std::uint64_t>::max() - (alignment - 1)) {
        return {false, ContractError::AddressOverflow};
    }
    const std::uint64_t aligned = (next_ + alignment - 1) & ~(alignment - 1);
    const GpuAddressRange candidate{aligned, bytes};
    if (!containsRange(aperture_, candidate)) {
        return {false, ContractError::OutOfRange};
    }
    next_ = aligned + bytes;
    result = candidate;
    return {true, ContractError::None};
}

std::uint64_t GpuVirtualAddressAllocator::nextAddress() const noexcept {
    return next_;
}

ContractResult validateQueueGeometry(const QueueGeometry& geometry) noexcept {
    if (!isPowerOfTwo(geometry.ringBytes) ||
        geometry.ringBytes < 4096 || geometry.ringBytes > 1024U * 1024U ||
        geometry.entryBytes < 4 || geometry.entryBytes > 64 ||
        geometry.ringBytes % geometry.entryBytes != 0 ||
        !isPowerOfTwo(geometry.alignment) ||
        geometry.alignment < geometry.entryBytes ||
        geometry.alignment > geometry.ringBytes) {
        return {false, ContractError::InvalidQueueGeometry};
    }
    return {true, ContractError::None};
}

QueueIndexModel::QueueIndexModel(std::uint32_t entries) noexcept
    : entries_(entries) {}

ContractResult QueueIndexModel::push() noexcept {
    if (entries_ < 2 || producer_ - consumer_ >= entries_ - 1) {
        return {false, ContractError::QueueFull};
    }
    ++producer_;
    return {true, ContractError::None};
}

ContractResult QueueIndexModel::pop() noexcept {
    if (producer_ == consumer_) {
        return {false, ContractError::QueueEmpty};
    }
    ++consumer_;
    return {true, ContractError::None};
}

std::uint32_t QueueIndexModel::producer() const noexcept { return producer_; }
std::uint32_t QueueIndexModel::consumer() const noexcept { return consumer_; }
std::uint32_t QueueIndexModel::capacity() const noexcept {
    return entries_ > 0 ? entries_ - 1 : 0;
}

}  // namespace cobalt64::research
