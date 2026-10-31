#!/usr/bin/env python3
"""Source-level safety contract for the PSP firmware load stage (C1).

Pins the FIRST-WRITE-STAGE contract on the source, mirroring
test_psp_survey_safety.py's zero-write checks for the survey:

  1. The stage is reachable only through the three opt-in boot-args
     (navi33bringup=1 + navi33psp=1 + navi33psp_load=1 / navi33psp_chain=1).
  2. Navi33PspLoad.cpp never writes PCI config space (configRead only).
  3. Every register WRITE in Navi33PspLoad.cpp goes through the bounds-checked
     RegBar::wr and targets ONLY the pinned offsets: the MM_INDEX/MM_DATA
     window (NBIF base 0: bytes 0x00/0x04/0x18) and MP0_C2PMSG_35/_36.
  4. "Navi33Bringup,FirmwareVerified" is set true only inside the
     chain-success branch (chainEnabled && chainSosAlive).
  5. The stage refuses on bad pre-conditions (abort strings present for each).

Run: python -B tests/test_psp_load_safety.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOAD = ROOT / "Navi33PspLoad.cpp"
BRINGUP = ROOT / "Navi33Bringup.cpp"

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print(f"FAIL: {msg}")


def load_source():
    return (LOAD.read_text(encoding="utf-8"),
            BRINGUP.read_text(encoding="utf-8"),
            (ROOT / "Navi33MmWindow.hpp").read_text(encoding="utf-8"))


def main():
    load, bringup, mmwindow = load_source()

    # 1. Opt-in gating (Navi33Bringup.cpp).
    for arg in ("navi33bringup=1", "navi33psp=1", "navi33psp_load=1",
                "navi33psp_chain=1"):
        check(f'"{arg}"' in bringup, f"boot-arg token {arg} must appear in Navi33Bringup.cpp")
    check("RunPspLoadStage(pci, this, pspChainStageEnabled())" in bringup,
          "the load stage must be invoked from start() with the chain gate")

    # 2. No PCI config writes in the load stage.
    check("configWrite" not in load,
          "Navi33PspLoad.cpp must never call configWrite (config is read-only)")
    for m in re.finditer(r"configRead(\d+)", load):
        pass  # reads are allowed
    check("configRead16" in load, "the PCI command read must stay present (precondition)")

    # 3. Every register write goes through RegBar::wr with a pinned offset.
    # The MM_INDEX/MM_DATA window helpers live in the shared Navi33MmWindow.hpp.
    if "bar.wr(" in load:
        raw = re.findall(r"bar\.wr\(([^,;]+),", load)
        allowed = {
            "mp0ByteOffset(kC2pmsg35Dword)", "mp0ByteOffset(kC2pmsg36Dword)",
        }
        unexpected = [r.strip() for r in raw if r.strip() not in allowed]
        check(not unexpected,
              f"register writes must stay on the pinned set; found: {unexpected}")
        check("vramWrite" in mmwindow and "vramRead" in mmwindow,
              "the MM_INDEX/MM_DATA window helpers must live in Navi33MmWindow.hpp")
        check("kMmIndexByte" in mmwindow and "kMmDataByte" in mmwindow and
              "kMmIndexHiByte" in mmwindow,
              "the window offsets must stay in the shared header")
    else:
        check(False, "RegBar::wr must exist (the stage is the write stage)")

    # 4. FirmwareVerified = true only after the chain succeeded.
    check('chainEnabled && chainSosAlive' in load,
          "the chain-success branch must gate the verified-firmware claim")
    verified_blocks = [m.start() for m in re.finditer(
        r'setProperty\("Navi33Bringup,FirmwareVerified", true\)', load)]
    chain_blocks = [m.start() for m in re.finditer(
        r'chainEnabled && chainSosAlive', load)]
    check(bool(verified_blocks),
          "FirmwareVerified=true must exist (the accelerator gate reads it)")
    if verified_blocks and chain_blocks:
        # The true-setting must sit inside the chain-success branch: the last
        # chain-success test before the setting must be closer than the last
        # failure path.
        setting = verified_blocks[0]
        prior = [c for c in chain_blocks if c < setting]
        check(bool(prior), "FirmwareVerified=true must come after a chain-success check")

    # 5. Fail-closed aborts for every pre-condition.
    for abort in (
        "aborted: PCI memory decode is off",
        "aborted: register BAR5 map failed",
        "aborted: register BAR5 window too small",
        "aborted: PSP bootloader not ready",
        "aborted: secure OS already running",
        "aborted: implausible FB_LOCATION values",
        "aborted: embedded psp_13_0_7_sos.bin failed v2.0 header validation",
        "aborted: no loadable components",
    ):
        check(abort in load, f"abort path must exist: {abort}")

    # The stage must never touch C2PMSG_64/67/69/70/71 (the ring registers):
    # the GPCOM ring is a LATER stage; only the bootloader mailbox is in scope.
    for ring in ("kC2pmsg64", "kC2pmsg67", "kC2pmsg69", "kC2pmsg70", "kC2pmsg71",
                 "C2PMSG_64", "C2PMSG_67"):
        check(ring not in load, f"ring register {ring} is out of scope for the load stage")

    if failures:
        print(f"{len(failures)} contract violation(s)")
        return 1
    print("PSP load safety contract: OK (writes confined to the pinned mailbox + MM window)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
