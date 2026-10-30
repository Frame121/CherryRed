#!/usr/bin/env python3
"""Static safety contract for the unbuilt Navi33 bring-up kext scaffold.

These checks do not compile or execute a kext and do not establish macOS ABI
compatibility. They guard the intended fail-closed / read-only / opt-in scaffold
and the hard rule that the Navi 33 target never touches Navi 48's PCI IDs.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "src" / "navi33-bringup" / "Navi33Bringup.cpp"
IDENTITY_HEADER = ROOT / "src" / "navi33-bringup" / "Navi33PciMatch.hpp"
PLIST = ROOT / "src" / "navi33-bringup" / "Info.plist"
MAKEFILE = ROOT / "src" / "navi33-bringup" / "Makefile"


def main():
    src = SOURCE.read_text(encoding="utf-8")
    identity_header = IDENTITY_HEADER.read_text(encoding="utf-8")
    plist = PLIST.read_text(encoding="utf-8")
    make = MAKEFILE.read_text(encoding="utf-8")
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    # Opt-in: exact boot-arg token, read from the device-tree "chosen" node
    # (com.apple.kpi.pexpert no longer exists as a linkable KPI on macOS 26, so
    # PE_parse_boot_argn must NOT be used — kxld would refuse to link the kext).
    check('bootArgTokenPresent(args, "navi33bringup=1")' in src,
          "opt-in boot-arg token check missing")
    check("/chosen" in src and "gIODTPlane" in src,
          "boot-args must be read from the device-tree chosen node")
    check("PE_parse_boot_argn" not in src,
          "pexpert boot-arg API must not be used (kpi.pexpert absent on Tahoe)")
    check("sizeof(args) - 1" in src and "args[len] = '\\0'" in src,
          "boot-args copy into the fixed buffer must be bounded and NUL-terminated")
    opt_in = src.index('bootArgTokenPresent(args, "navi33bringup=1")')
    identity = src.index("kIOPCIConfigVendorID", opt_in)
    check(opt_in < identity, "opt-in must be checked before device identity")

    # Basic recognition is by AMD vendor/device ID only. Subsystem identity
    # is a separate evidence-board classification, not a recognition gate.
    check("0x7480" in identity_header, "RX 7600 device id 0x7480 missing")
    check("0x148C" in identity_header and "0x2421" in identity_header,
          "evidenced subsystem ids missing")
    gpu_match = re.search(
        r"bool Navi33Bringup::identityMatchesGpu\(IOPCIDevice \*pci\)(.*?)\n\}",
        src, re.S
    )
    check(gpu_match is not None, "GPU identity matcher missing")
    if gpu_match:
        body = gpu_match.group(1)
        check("kIOPCIConfigVendorID" in body and "kIOPCIConfigDeviceID" in body,
              "GPU recognition must use PCI vendor/device IDs")
        check("kIOPCIConfigSubSystem" not in body,
              "subsystem IDs must not block RX 7600 recognition")
    board_match = re.search(
        r"bool Navi33Bringup::identityMatchesEvidenceBoard\(IOPCIDevice \*pci\)(.*?)\n\}",
        src, re.S
    )
    check(board_match is not None and "kIOPCIConfigSubSystemVendorID" in
          (board_match.group(1) if board_match else ""),
          "subsystem check must remain a separate evidence-board classification")
    check("identityMatchesGpu(pci)" in src,
          "probe/start must use the generic RX 7600 identity gate")
    check('#include "Navi33PciMatch.hpp"' in src,
          "kext must use the same host-tested PCI identity implementation")
    check("identity.subsystemVendorId == kCapturedSubsystemVendorId" in identity_header and
          "identity.subsystemDeviceId == kCapturedSubsystemDeviceId" in identity_header,
          "captured subsystem match must be separate from generic GPU recognition")
    check("identity.vendorId == kAmdVendorId" in identity_header and
          "identity.deviceId == kRx7600DeviceId" in identity_header,
          "shared PCI matcher must identify RX 7600 using vendor/device only")
    check('"Navi33Bringup,GPUName", "AMD Radeon RX 7600"' in src,
          "RX 7600 model property missing")
    check('"Navi33Bringup,Architecture", "Navi 33"' in src and
          '"Navi33Bringup,GfxTarget", "gfx1102"' in src,
          "Navi 33 / gfx1102 identification properties missing")
    check('"Navi33Bringup,FutureHardwareInitAllowed", false' in src,
          "unknown board must not be authorized for future hardware init")
    check("<string>0x74801002</string>" in plist, "Info.plist must match 0x74801002")
    check("<string>IOPCIDevice</string>" in plist, "Info.plist provider must be IOPCIDevice")

    # HARD RULE: the Navi 33 target must never reference Navi 48 PCI ids.
    for nid in ("0x7550", "0x7551", "75501002", "75511002"):
        check(nid not in src and nid not in plist,
              f"Navi 48 PCI id {nid} leaked into the Navi 33 target")

    # No register mapping or device writes (read-only discovery only).
    forbidden = (
        "mapDeviceMemoryWithRegister",
        "setMemoryEnable(",
        "configWrite8(",
        "configWrite16(",
        "configWrite32(",
        "writeRegister(",
        "write32(",
        "write16(",
        "write8(",
    )
    for token in forbidden:
        check(token not in src, f"forbidden write/mapping token present: {token}")

    # Fail-closed markers are published.
    check('"Navi33Bringup,FirmwareLoaded", false' in src,
          "FirmwareLoaded=false marker missing")
    check('"Navi33Bringup,HardwareWrites", false' in src,
          "HardwareWrites=false marker missing")
    check("refuseFirmwareAndWrites" in src, "explicit firmware/writes refusal missing")

    # ReBAR decoding must follow PCIe field layout and fail closed when
    # capability/control metadata disagree. Keep this test source-level only;
    # it does not emulate a PCI config space or establish hardware compatibility.
    check("uint32_t capBarIndex = cap & 0x7;" in src,
          "ReBAR capability BAR index decoding missing")
    check("uint32_t ctlBarIndex = ctl & 0x7;" in src,
          "ReBAR control BAR index decoding missing")
    check("uint32_t supportedSizes = cap >> 4;" in src,
          "ReBAR supported-size bitmap decoding missing")
    check("uint32_t sizeEncoding = (ctl >> 8) & 0x3F;" in src,
          "ReBAR current size encoding decoding missing")
    check("uint32_t extendedSupportedSizes = ctl >> 16;" in src,
          "ReBAR extended supported-size bitmap decoding missing")
    check("bool encodingValid = sizeEncoding <= 43;" in src,
          "ReBAR size encoding must support PCIe encodings through 43 and be bounded")
    check("(supportedSizes & (1U << sizeEncoding)) != 0" in src and
          "(extendedSupportedSizes & (1U << (sizeEncoding - 28))) != 0" in src,
          "ReBAR current size must be validated against the correct supported-size bitmap")
    check("bool indicesMatch = capBarIndex == ctlBarIndex;" in src,
          "ReBAR capability/control BAR index consistency check missing")
    check("if (!encodingValid || !encodingSupported || !indicesMatch)" in src,
          "ReBAR metadata must fail closed on invalid/inconsistent values")
    check('"Navi33Bringup,ResizableBARCurrentSizeMiB"' in src and
          "1ULL << sizeEncoding" in src and
          "currentSizeMiB, 64" in src,
          "ReBAR current-size property must preserve 64-bit MiB value")

    # Firmware blob set in the Makefile matches the RX 7600 (no Navi 48 blobs).
    for blob in ("gc_11_0_2_me", "sdma_6_0_2", "psp_13_0_7_sos", "smu_13_0_7",
                 "dcn_3_2_1_dmcub", "vcn_4_0_4"):
        check(blob in make, f"Makefile FW_REQUIRED missing RX 7600 blob: {blob}")
    for blob in ("gc_12_0_1", "psp_14_0_3", "smu_14_0_3", "sdma_7_0_1"):
        check(blob not in make, f"Navi 48 blob leaked into Navi 33 Makefile: {blob}")

    if failures:
        print("BRINGUP SAFETY TEST: FAIL")
        for f in failures:
            print("  -", f)
        raise SystemExit(1)
    print("BRINGUP SAFETY TEST: PASS (opt-in, RX7600-only, read-only, fail-closed, no Navi 48 ids)")


if __name__ == "__main__":
    main()
