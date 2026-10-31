#ifndef REDNAVI_GPU_TARGET_H
#define REDNAVI_GPU_TARGET_H

#include <stdint.h>

namespace rednavi {

enum class GpuFamily : uint8_t {
    Unknown = 0,
    Navi31,
    Navi32,
    Navi33
};

struct GpuTargetProfile {
    GpuFamily family;
    const char *asicName;
    const char *model;
    uint32_t modelBytes;
    uint32_t vramMegabytes;
    bool useNavi32RegisterPath;

    constexpr bool isKnown() const noexcept {
        return family != GpuFamily::Unknown;
    }
};

static constexpr char kRx7600Model[] = "AMD Radeon RX 7600";
static constexpr char kRx7800XtModel[] = "AMD Radeon RX 7800 XT";

// Keep ASIC recognition separate from register-path authorization.
// Only the exact Navi 32 device used by RedNavi's hardware bring-up may enter
// the current Navi 32 register/firmware path. Other known GPUs remain read-only.
constexpr GpuTargetProfile identifyGpu(uint16_t vendorId,
                                       uint16_t deviceId) noexcept {
    if (vendorId != 0x1002) {
        return {GpuFamily::Unknown, "unknown vendor/device", nullptr, 0, 0, false};
    }

    switch (deviceId) {
        case 0x744C: // Navi 31; currently identification/read-only only.
            return {GpuFamily::Navi31, "Navi 31", nullptr, 0, 0, false};
        case 0x747E: // Navi 32; the existing RedNavi bring-up target.
            return {GpuFamily::Navi32, "Navi 32", kRx7800XtModel,
                    sizeof(kRx7800XtModel), 16384, true};
        case 0x7480: // Navi 33; RX 7600 community capture, read-only only.
            return {GpuFamily::Navi33, "Navi 33", kRx7600Model,
                    sizeof(kRx7600Model), 8192, false};
        default:
            return {GpuFamily::Unknown, "unknown AMD device", nullptr, 0, 0, false};
    }
}

} // namespace rednavi

#endif // REDNAVI_GPU_TARGET_H
