#include "gpu_ip_profile.h"
#include "navi33_board.h"
#include <stdio.h>
#include <string.h>

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
    static_assert(rx7600.gcRegisterReadiness == rednavi::BringupReadiness::HostTested, "Navi 33 GC map is host-tested");
    static_assert(rx7600.firmwareReadiness == rednavi::BringupReadiness::HostTested, "Navi 33 firmware preparation is host-tested");
    static_assert(rx7600.displayReadiness == rednavi::BringupReadiness::SourceMapped, "Navi 33 display path is source-mapped only");
    static_assert(rx7600.graphicsSubmissionReadiness == rednavi::BringupReadiness::HostTested, "Navi 33 command packet encoding is host-tested");
    static_assert(rednavi::kNavi33Board.vendorId == 0x1002 && rednavi::kNavi33Board.deviceId == 0x7480, "RX7600 board identity");
    static_assert(rednavi::kNavi33Board.vramBytes == 0x200000000ull, "RX7600 8 GiB VRAM size");
    static_assert(rednavi::kNavi33Board.pspTmrVramOffset == 0x1fc000000ull, "RX7600 TMR VRAM offset");
    static_assert(rednavi::kNavi33Board.pspTmrBytes == 0x01300000u, "RX7600 TMR reservation size");
    static_assert(rednavi::kNavi33Board.pspSosVersion == 0x00330024u, "RX7600 PSP SOS version");
    static_assert(rednavi::kNavi33Board.smuFirmwareVersion == 0x00526100u, "RX7600 SMU firmware version");
    static_assert(rednavi::kNavi33Board.dmubFirmwareVersion == 0x07003600u, "RX7600 DMCUB firmware version");
    static_assert(rx7800.mayUseNavi32Bringup(), "Navi 32 reference path remains eligible");
    static_assert(unknown.family == rednavi::GpuFamily::Unknown, "unknown PCI vendor rejected");

    if (strcmp(rednavi::readinessName(rx7600.gcRegisterReadiness), "HOST_TESTED") != 0 ||
        strcmp(rednavi::readinessName(rx7600.displayReadiness), "SOURCE_MAPPED") != 0) return 1;
    puts("GPU IP profile tests: PASS (Navi 33 identity, 10 IP versions, readiness vs hardware gates)");
    return 0;
}
