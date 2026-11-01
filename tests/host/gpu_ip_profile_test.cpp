#include "gpu_ip_profile.h"
#include <stdio.h>

int main() {
    constexpr auto rx7600 = rednavi::ipProfileForPci(0x1002, 0x7480);
    constexpr auto rx7800 = rednavi::ipProfileForPci(0x1002, 0x747E);
    constexpr auto unknown = rednavi::ipProfileForPci(0x1234, 0x7480);

    static_assert(rx7600.family == rednavi::GpuFamily::Navi33, "RX 7600 family");
    static_assert(rx7600.gc.major == 11 && rx7600.gc.minor == 0 && rx7600.gc.revision == 2, "Navi 33 GC 11.0.2");
    static_assert(rx7600.sdma.minor == 0 && rx7600.sdma.revision == 2, "Navi 33 SDMA 6.0.2");
    static_assert(rx7600.psp.revision == 7 && rx7600.smu.revision == 7, "Navi 33 PSP/SMU 13.0.7");
    static_assert(rx7600.dcn.major == 3 && rx7600.dcn.minor == 2 && rx7600.dcn.revision == 1, "Navi 33 DCN 3.2.1");
    static_assert(rx7600.ipDiscoveryObservedOnTarget, "Linux discovery evidence exists");
    static_assert(!rx7600.gcRegisterOffsetsValidatedOnTarget, "Navi 33 GC offsets are not macOS-validated");
    static_assert(!rx7600.firmwareSequenceValidatedOnTarget, "Navi 33 firmware sequence is not validated");
    static_assert(!rx7600.displayInitValidatedOnTarget && !rx7600.graphicsSubmissionValidatedOnTarget, "No Navi 33 macOS display/GFX bring-up claimed");
    static_assert(!rx7600.mayUseNavi32Bringup(), "Navi 33 must not use Navi 32 bring-up");
    static_assert(rx7800.mayUseNavi32Bringup(), "Navi 32 reference path remains eligible");
    static_assert(unknown.family == rednavi::GpuFamily::Unknown, "unknown PCI vendor rejected");

    puts("GPU IP profile tests: PASS (Navi 33 identity, 10 IP versions, 5 validation gates)");
    return 0;
}
