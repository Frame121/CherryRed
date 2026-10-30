#ifndef COBALT64_NAVI33_PCI_MATCH_HPP
#define COBALT64_NAVI33_PCI_MATCH_HPP

#include <cstdint>

namespace cobalt64::navi33 {

inline constexpr std::uint16_t kAmdVendorId = 0x1002;
inline constexpr std::uint16_t kRx7600DeviceId = 0x7480;
inline constexpr std::uint16_t kCapturedSubsystemVendorId = 0x148C;
inline constexpr std::uint16_t kCapturedSubsystemDeviceId = 0x2421;

struct PciIdentity {
    std::uint16_t vendorId = 0;
    std::uint16_t deviceId = 0;
    std::uint16_t subsystemVendorId = 0;
    std::uint16_t subsystemDeviceId = 0;
};

// Primary recognition is a PCI vendor/device identity check. Do not tie the
// GPU model to one AIB/board subsystem ID.
constexpr bool isRx7600(const PciIdentity& identity) noexcept {
    return identity.vendorId == kAmdVendorId &&
           identity.deviceId == kRx7600DeviceId;
}

// This stronger predicate is a match to the currently captured board only.
// It is not used to decide whether the GPU itself is an RX 7600.
constexpr bool matchesCapturedEvidenceBoard(
    const PciIdentity& identity) noexcept {
    return isRx7600(identity) &&
           identity.subsystemVendorId == kCapturedSubsystemVendorId &&
           identity.subsystemDeviceId == kCapturedSubsystemDeviceId;
}

}  // namespace cobalt64::navi33

#endif
