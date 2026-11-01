#ifndef COBALT64_NAVI33_GFX11_MANIFEST_HPP
#define COBALT64_NAVI33_GFX11_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cobalt64::research {

struct Gfx11IpVersion {
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
    std::uint8_t revision = 0;
};

enum class Gfx11FirmwareRole : std::uint8_t {
    PspSos, PspTa, Smu, GcPfp, GcMe, GcMec, GcRlc, Sdma
};

struct Gfx11FirmwareRecord {
    Gfx11FirmwareRole role = Gfx11FirmwareRole::PspSos;
    std::string_view filename;
    std::uint64_t byteLength = 0;
    std::string_view sha256;
    std::string_view upstreamCommit;
    bool boardAttributed = false;
    bool licenseApproved = false;
};

struct Gfx11TargetIdentity {
    std::uint16_t vendorId = 0;
    std::uint16_t deviceId = 0;
    std::uint16_t subsystemVendorId = 0;
    std::uint16_t subsystemId = 0;
    std::uint8_t pciRevision = 0;
};

enum class Gfx11Evidence : std::uint8_t {
    Unknown, GenericUpstream, BoardAttributed, HardwareVerified
};

struct Gfx11Manifest {
    Gfx11TargetIdentity target{};
    Gfx11Evidence evidence = Gfx11Evidence::Unknown;
    Gfx11IpVersion gc{};
    Gfx11IpVersion psp{};
    Gfx11IpVersion smu{};
    Gfx11IpVersion sdma{};
    bool vbiosIdentityMatched = false;
    const Gfx11FirmwareRecord* firmware = nullptr;
    std::size_t firmwareCount = 0;
};

enum class Gfx11ManifestError : std::uint8_t {
    None, WrongTarget, InsufficientEvidence, MissingVbiosMatch,
    InvalidIpVersion, UnsupportedIpFamily, MissingFirmware, TooManyFirmwareRecords,
    InvalidFirmwareRecord, DuplicateFirmwareRole, MissingRequiredRole,
    WrongFirmwarePrefix
};

struct Gfx11ManifestResult {
    bool valid = false;
    Gfx11ManifestError error = Gfx11ManifestError::None;
};

// Validates a board-attributed review manifest only. It never loads firmware,
// touches PCI/BAR/MMIO, resets the GPU, creates queues, or enables a kext.
Gfx11ManifestResult validateGfx11Manifest(const Gfx11Manifest& manifest) noexcept;

} // namespace cobalt64::research

#endif
