#ifndef COBALT64_NAVI33_BRINGUP_MODEL_HPP
#define COBALT64_NAVI33_BRINGUP_MODEL_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cobalt64::research {

enum class Navi33FirmwareEngine : std::uint8_t { PSP, SMU, GC, SDMA, MES };
enum class FirmwareProvenance : std::uint8_t {
    Unknown, GenericUpstream, BoardAttributed, TargetVerified
};
enum class FirmwareLicense : std::uint8_t { Unknown, ReviewRequired, Approved };

struct FirmwareDescriptor {
    Navi33FirmwareEngine engine = Navi33FirmwareEngine::PSP;
    std::string_view filename;
    std::size_t byteLength = 0;
    std::string_view sha256;
    FirmwareProvenance provenance = FirmwareProvenance::Unknown;
    FirmwareLicense license = FirmwareLicense::Unknown;
};

enum class ContractError : std::uint8_t {
    None, MissingName, InvalidSize, InvalidDigest, UnknownProvenance,
    LicenseNotApproved, InvalidAlignment, AddressOverflow, OutOfRange,
    InvalidQueueGeometry, QueueFull, QueueEmpty
};

struct ContractResult {
    bool valid = false;
    ContractError error = ContractError::None;
};

ContractResult validateFirmwareDescriptor(const FirmwareDescriptor& descriptor) noexcept;

struct GpuAddressRange {
    std::uint64_t base = 0;
    std::uint64_t length = 0;
};

bool containsRange(const GpuAddressRange& outer,
                   const GpuAddressRange& inner) noexcept;

class GpuVirtualAddressAllocator {
public:
    explicit GpuVirtualAddressAllocator(GpuAddressRange aperture) noexcept;
    ContractResult allocate(std::uint64_t bytes, std::uint64_t alignment,
                            GpuAddressRange& result) noexcept;
    std::uint64_t nextAddress() const noexcept;

private:
    GpuAddressRange aperture_{};
    std::uint64_t next_ = 0;
};

struct QueueGeometry {
    std::uint32_t ringBytes = 0;
    std::uint16_t entryBytes = 0;
    std::uint32_t alignment = 0;
};

ContractResult validateQueueGeometry(const QueueGeometry& geometry) noexcept;

// Host-only arithmetic model. It does not allocate GPU memory, access a
// doorbell, submit packets, or communicate with a physical queue.
class QueueIndexModel {
public:
    explicit QueueIndexModel(std::uint32_t entries) noexcept;
    ContractResult push() noexcept;
    ContractResult pop() noexcept;
    std::uint32_t producer() const noexcept;
    std::uint32_t consumer() const noexcept;
    std::uint32_t capacity() const noexcept;

private:
    std::uint32_t entries_ = 0;
    std::uint32_t producer_ = 0;
    std::uint32_t consumer_ = 0;
};

}  // namespace cobalt64::research

#endif
