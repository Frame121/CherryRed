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
PLIST = ROOT / "src" / "navi33-bringup" / "Info.plist"
MAKEFILE = ROOT / "src" / "navi33-bringup" / "Makefile"


def main():
    src = SOURCE.read_text(encoding="utf-8")
    plist = PLIST.read_text(encoding="utf-8")
    make = MAKEFILE.read_text(encoding="utf-8")
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    # Opt-in: exact boot argument, checked before device identity.
    check('PE_parse_boot_argn("navi33bringup", enabled, sizeof(enabled))' in src,
          "opt-in boot argument guard missing")
    check("enabled[0] != '1' || enabled[1] != '\\0'" in src,
          "opt-in exact-string check missing")
    opt_in = src.index('PE_parse_boot_argn("navi33bringup"')
    identity = src.index("kIOPCIConfigVendorID", opt_in)
    check(opt_in < identity, "opt-in must be checked before device identity")

    # Matches ONLY the RX 7600 board.
    check("0x7480" in src, "RX 7600 device id 0x7480 missing")
    check("0x148C" in src and "0x2421" in src, "evidenced subsystem ids missing")
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
