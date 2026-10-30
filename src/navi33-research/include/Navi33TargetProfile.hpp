#ifndef COBALT64_NAVI33_TARGET_PROFILE_HPP
#define COBALT64_NAVI33_TARGET_PROFILE_HPP

#include <cstddef>
#include <cstdint>

namespace cobalt64::research {

// Numeric HWID + instance comes directly from IP Discovery. Symbolic labels
// are kept in the expected-profile table, never inferred for unknown HWIDs.
struct Navi33IpObservation {
    std::uint16_t hwId;
    std::uint8_t instance;
    std::uint8_t major;
    std::uint8_t minor;
    std::uint8_t revision;
};

enum class TargetProfileError : std::uint8_t {
    None = 0,
    NullInput,
    MissingRequiredBlock,
    DuplicateRequiredBlock,
    VersionMismatch,
};

struct TargetProfileResult {
    bool valid;
    TargetProfileError error;
    const char* blockName;
    const char* message;
};

// Validates critical Navi 33 IP records using numeric HWIDs. This is an
// evidence-consistency check only: it cannot authorize MMIO or GPU init.
TargetProfileResult validateNavi33TargetProfile(
    const Navi33IpObservation* observations,
    std::size_t count) noexcept;

}  // namespace cobalt64::research

#endif
