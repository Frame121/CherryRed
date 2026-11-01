#include "Navi33PciMatch.hpp"

#include <cstdio>

using cobalt64::navi33::PciIdentity;
using cobalt64::navi33::isRx7600;
using cobalt64::navi33::matchesCapturedEvidenceBoard;

namespace {
int failures = 0;
void check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}
}

int main() {
    constexpr PciIdentity capturedBoard{0x1002, 0x7480, 0x148C, 0x2421};
    constexpr PciIdentity otherBoard{0x1002, 0x7480, 0x1043, 0x0001};
    constexpr PciIdentity otherAmdGpu{0x1002, 0x7550, 0x148C, 0x2421};
    constexpr PciIdentity wrongVendor{0x1234, 0x7480, 0x148C, 0x2421};
    constexpr PciIdentity emptyIdentity{};

    static_assert(isRx7600(capturedBoard), "captured board must identify as RX 7600");
    static_assert(isRx7600(otherBoard), "different AIB subsystem must still identify as RX 7600");
    static_assert(!isRx7600(otherAmdGpu), "Navi 48 must not identify as RX 7600");
    static_assert(!isRx7600(wrongVendor), "wrong vendor must not identify as RX 7600");
    static_assert(matchesCapturedEvidenceBoard(capturedBoard),
                  "captured subsystem must match evidence board");
    static_assert(!matchesCapturedEvidenceBoard(otherBoard),
                  "other board must not inherit captured-board evidence");

    check(isRx7600(capturedBoard), "captured PCI identity recognized");
    check(isRx7600(otherBoard), "RX 7600 recognized with different subsystem");
    check(!matchesCapturedEvidenceBoard(otherBoard),
          "different subsystem remains evidence-unverified");
    check(!isRx7600(otherAmdGpu), "different AMD device is rejected");
    check(!isRx7600(wrongVendor), "non-AMD vendor is rejected");
    check(!isRx7600(emptyIdentity), "empty PCI identity is rejected");

    if (failures != 0) {
        std::fprintf(stderr, "Navi33 PCI identity tests: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("Navi33 PCI identity tests: PASS (6 runtime scenarios + compile-time assertions)");
    return 0;
}
