#ifndef COBALT64_NAVI33_IP_DISCOVERY_HPP
#define COBALT64_NAVI33_IP_DISCOVERY_HPP

#include <cstddef>
#include <cstdint>

namespace cobalt64::research {

// Offline parser output. Base addresses are preserved as discovery metadata
// only; this API does not map BARs, calculate host addresses, or authorize MMIO.
struct Navi33DiscoveredIp {
    std::uint16_t hwId = 0;
    std::uint8_t instance = 0;
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
    std::uint8_t revision = 0;
    std::uint8_t subRevision = 0;
    std::uint8_t variant = 0;
    std::uint8_t numBaseAddresses = 0;
    std::uint8_t dieIndex = 0;
    std::uint32_t baseAddresses[16]{};
};

enum class Navi33IpDiscoveryError : std::uint8_t {
    None = 0,
    NullInput,
    NullOutput,
    InputTooSmall,
    BinaryNotFound,
    UnsupportedBinaryVersion,
    InvalidBinarySize,
    BinaryChecksumMismatch,
    TableDirectoryInvalid,
    IpDiscoveryMissing,
    IpDiscoveryHeaderInvalid,
    IpDiscoveryChecksumMismatch,
    UnsupportedIpDiscoveryVersion,
    InvalidDieLayout,
    TooManyRecords,
    TooManyBaseAddresses,
    TruncatedRecord,
    OutputCapacityTooSmall
};

struct Navi33IpDiscoveryResult {
    bool valid = false;
    Navi33IpDiscoveryError error = Navi33IpDiscoveryError::None;
    std::size_t binaryOffset = 0;
    std::uint16_t binaryMajor = 0;
    std::uint16_t binaryMinor = 0;
    std::uint16_t declaredBinarySize = 0;
    std::uint16_t ipDiscoveryVersion = 0;
    std::uint16_t dieCount = 0;
    std::size_t recordCount = 0;
    const char* message = "";
};

// Parses AMD Discovery binary v1/v2 with IP Discovery table v3 (the format
// captured from this RX 7600). All ranges and both checksums are validated.
// Unknown HWIDs are preserved numerically; no symbolic name is guessed.
// Output records are valid only when result.valid is true.
Navi33IpDiscoveryResult parseNavi33IpDiscovery(
    const std::uint8_t* data, std::size_t size,
    Navi33DiscoveredIp* output, std::size_t outputCapacity) noexcept;

const char* navi33IpDiscoveryErrorName(Navi33IpDiscoveryError error) noexcept;

}  // namespace cobalt64::research

#endif
