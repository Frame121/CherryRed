#include "Navi33TargetProfile.hpp"

namespace cobalt64::research {
namespace {

struct ExpectedIp {
    std::uint16_t hwId;
    std::uint8_t instance;
    const char* name;
    std::uint8_t major;
    std::uint8_t minor;
    std::uint8_t revision;
};

// Numeric IDs are source-correlated with the captured RX 7600 mapping file.
// Unknown IDs 282 and 286 are deliberately not assigned symbolic names here.
constexpr ExpectedIp kRequired[] = {
    {11, 0, "GC", 11, 0, 2},
    {42, 0, "SDMA0", 6, 0, 2},
    {43, 0, "SDMA1", 6, 0, 2},
    {12, 0, "UVD", 4, 0, 4},
    {255, 0, "MP0", 13, 0, 7},
    {1, 0, "MP1", 13, 0, 7},
    {271, 0, "DMU", 3, 2, 1},
};

TargetProfileResult result(TargetProfileError error, const char* block,
                           const char* message) noexcept {
    return {false, error, block, message};
}

}  // namespace

TargetProfileResult validateNavi33TargetProfile(
    const Navi33IpObservation* observations, std::size_t count) noexcept {
    if (observations == nullptr || count == 0U) {
        return result(TargetProfileError::NullInput, "",
                      "IP observation input is null or empty; fail closed.");
    }

    for (const auto& expected : kRequired) {
        const Navi33IpObservation* found = nullptr;
        for (std::size_t i = 0; i < count; ++i) {
            const auto& current = observations[i];
            if (current.hwId != expected.hwId ||
                current.instance != expected.instance) {
                continue;
            }
            if (found != nullptr) {
                return result(TargetProfileError::DuplicateRequiredBlock,
                              expected.name,
                              "Duplicate required IP block; fail closed.");
            }
            found = &current;
        }
        if (found == nullptr) {
            return result(TargetProfileError::MissingRequiredBlock,
                          expected.name,
                          "Required RX 7600 IP block is missing; fail closed.");
        }
        if (found->major != expected.major ||
            found->minor != expected.minor ||
            found->revision != expected.revision) {
            return result(TargetProfileError::VersionMismatch, expected.name,
                          "IP version differs from captured RX 7600 evidence; fail closed.");
        }
    }

    return {true, TargetProfileError::None, "",
            "Critical numeric HWIDs and IP versions match captured RX 7600 evidence; hardware initialization remains disabled."};
}

}  // namespace cobalt64::research
