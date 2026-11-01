#!/usr/bin/env python3
"""Source-level safety contract for the SMU stage (M2b).

  1. Reachable only behind the full opt-in ladder ending in navi33psp_smu=1.
  2. Write class: the MM index/data window and the MP1 mailbox registers
     through the bounds-checked RegBar — never PCI config writes, never
     setMemoryEnable, never a raw pointer store.
  3. FirmwareVerified=true is published ONLY in the branch where the SMU
     answered GetSmuVersion with an OK response.
  4. Every wait is bounded.

Run: python -B tests/test_psp_smu_safety.py
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SMU = ROOT / "Navi33SmuLoad.cpp"
BRINGUP = ROOT / "Navi33Bringup.cpp"

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def main() -> int:
    smu = SMU.read_text(encoding="utf-8")
    bringup = BRINGUP.read_text(encoding="utf-8")

    # 1. The bring-up gate delegates to the ring gate (the ring must exist in
    #    the same boot) and adds its own opt-in.
    gate = bringup[bringup.index("bool Navi33Bringup::smuStageEnabled()"):]
    gate = gate[:gate.index("\n}")]
    check("pspRingStageEnabled()" in gate and "navi33psp_smu=1" in gate,
          "smu gate must require the ring gate plus navi33psp_smu=1")
    ring_gate = bringup[bringup.index("bool Navi33Bringup::pspRingStageEnabled()"):]
    ring_gate = ring_gate[:ring_gate.index("\n}")]
    for token in ("navi33bringup=1", "navi33psp=1", "navi33psp_ring=1"):
        check(token in ring_gate, f"ring gate must require {token}")
    # gc gate builds on the smu gate.
    gc_gate = bringup[bringup.index("bool Navi33Bringup::gcStageEnabled()"):]
    gc_gate = gc_gate[:gc_gate.index("\n}")]
    check("smuStageEnabled()" in gc_gate and "navi33gc_load=1" in gc_gate,
          "gc gate must require the smu gate plus navi33gc_load=1")

    # 2. Write confinement.
    forbidden = ("configWrite", "setMemoryEnable(", "IOMemoryDescriptor *",
                 "->writeRegister")
    for token in forbidden:
        check(token not in smu, f"forbidden in the SMU stage: {token}")
    check("RegBar bar;" in smu, "all access goes through the bounds-checked RegBar")
    check("mapDeviceMemoryWithRegister" in smu,
          "the stage maps BAR5 itself (and releases it)")

    # 3. FirmwareVerified only on the SMU-answers branch.
    verified_at = smu.index('"Navi33Bringup,FirmwareVerified", true')
    version_ok_at = smu.index("const bool versionOk = smu13Exchange")
    check(version_ok_at < verified_at,
          "FirmwareVerified=true must sit after the GetSmuVersion exchange")
    check('"Navi33Bringup,FirmwareVerified", false' in smu,
          "the failure path must explicitly publish FirmwareVerified=false")

    # 4. Bounded waits: the two poll loops use kSmuReadyWaitMs.
    check(smu.count("kSmuReadyWaitMs") >= 2,
          "both mailbox polls must be bounded by kSmuReadyWaitMs")
    check("while (1)" not in smu and "for (;;)" not in smu,
          "no unbounded loops in the SMU stage")

    if failures:
        print("PSP SMU SAFETY TEST: FAIL")
        for f in failures:
            print("  -", f)
        return 1
    print("PSP SMU SAFETY TEST: PASS (opt-in ladder, RegBar-only writes, "
          "verified-only publication, bounded waits)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
