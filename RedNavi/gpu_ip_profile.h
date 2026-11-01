#ifndef REDNAVI_GPU_IP_PROFILE_H
#define REDNAVI_GPU_IP_PROFILE_H

#include "gpu_target.h"

namespace rednavi {

struct IpVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t revision;
};

// Hardware-generation facts are kept separate from permission to touch hardware.
// A version match is not proof that register sequences or firmware are compatible.
struct GpuIpProfile {
    GpuFamily family;
    IpVersion gc;
    IpVersion sdma;
    IpVersion psp;
    IpVersion smu;
    IpVersion dcn;
    IpVersion nbio;
    IpVersion mmhub;
    IpVersion osssys;
    IpVersion hdp;
    IpVersion vcn;
    bool ipDiscoveryObservedOnTarget;
    bool gcRegisterOffsetsValidatedOnTarget;
    bool firmwareSequenceValidatedOnTarget;
    bool displayInitValidatedOnTarget;
    bool graphicsSubmissionValidatedOnTarget;

    constexpr bool mayUseNavi32Bringup() const noexcept {
        return family == GpuFamily::Navi32 &&
               gcRegisterOffsetsValidatedOnTarget &&
               firmwareSequenceValidatedOnTarget;
    }
};

constexpr GpuIpProfile ipProfileFor(GpuFamily family) noexcept {
    switch (family) {
        case GpuFamily::Navi32:
            // RX 7800 XT reference target; see docs/ARCHITECTURE.md.
            return {family, {11,0,3}, {6,0,3}, {13,0,10}, {13,0,10},
                    {3,2,0}, {4,3,0}, {3,0,0}, {6,0,0}, {6,0,0}, {4,0,0},
                    true, true, true, true, false};
        case GpuFamily::Navi33:
            // RX 7600 measured Linux capture, 2026-11-01. No macOS write path validated.
            return {family, {11,0,2}, {6,0,2}, {13,0,7}, {13,0,7},
                    {3,2,1}, {4,3,1}, {3,0,2}, {6,0,2}, {6,0,1}, {4,0,0},
                    true, false, false, false, false};
        case GpuFamily::Navi31:
            return {family, {11,0,0}, {6,0,0}, {13,0,0}, {13,0,0},
                    {3,2,0}, {4,3,0}, {3,0,0}, {6,0,0}, {6,0,0}, {4,0,0},
                    false, false, false, false, false};
        default:
            return {GpuFamily::Unknown, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                    false, false, false, false, false};
    }
}

constexpr GpuIpProfile ipProfileForPci(uint16_t vendorId, uint16_t deviceId) noexcept {
    return ipProfileFor(identifyGpu(vendorId, deviceId).family);
}

} // namespace rednavi

#endif // REDNAVI_GPU_IP_PROFILE_H
