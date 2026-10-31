#include "gpu_target.h"

#include <stdio.h>
#include <string.h>

namespace {
int failures = 0;

void check(bool condition, const char *label) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}
}

int main() {
    constexpr auto rx7600 = rednavi::identifyGpu(0x1002, 0x7480);
    constexpr auto rx7800 = rednavi::identifyGpu(0x1002, 0x747E);
    constexpr auto rx7900 = rednavi::identifyGpu(0x1002, 0x744C);
    constexpr auto wrongVendor = rednavi::identifyGpu(0x1234, 0x7480);
    constexpr auto unknownAmd = rednavi::identifyGpu(0x1002, 0x7550);

    static_assert(rx7600.family == rednavi::GpuFamily::Navi33,
                  "1002:7480 must identify as Navi 33");
    static_assert(!rx7600.useNavi32RegisterPath,
                  "Navi 33 must never enter the Navi 32 register path");
    static_assert(rx7600.vramMegabytes == 8192,
                  "RX 7600 metadata must report 8 GB");
    static_assert(rx7800.family == rednavi::GpuFamily::Navi32 &&
                  rx7800.useNavi32RegisterPath,
                  "the existing Navi 32 target must retain its path");
    static_assert(rx7900.family == rednavi::GpuFamily::Navi31 &&
                  !rx7900.useNavi32RegisterPath,
                  "Navi 31 must remain outside the Navi 32 path");
    static_assert(!wrongVendor.isKnown() && !unknownAmd.isKnown(),
                  "unknown identities must not be classified");

    check(rx7600.model != nullptr &&
          strcmp(rx7600.model, "AMD Radeon RX 7600") == 0,
          "RX 7600 marketing model");
    check(rx7600.modelBytes == sizeof(rednavi::kRx7600Model),
          "RX 7600 model property includes its NUL terminator");
    check(rx7600.vramMegabytes == 8192, "RX 7600 8 GB metadata");
    check(!rx7600.useNavi32RegisterPath, "RX 7600 remains read-only");
    check(rx7800.useNavi32RegisterPath, "Navi 32 path remains enabled");
    check(!rx7900.useNavi32RegisterPath, "Navi 31 remains read-only");
    check(!wrongVendor.isKnown(), "wrong PCI vendor is rejected");
    check(!unknownAmd.isKnown(), "unknown AMD device is rejected");

    if (failures != 0) {
        fprintf(stderr, "GPU target tests: %d failure(s)\n", failures);
        return 1;
    }

    puts("GPU target tests: PASS (8 runtime checks + compile-time assertions)");
    return 0;
}
