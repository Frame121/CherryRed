#!/usr/bin/env python3
"""Source-level safety contract for the PSP ring stage (M2a completion).

Pins the ring stage's write confinement on the source:

  1. Reachable only through navi33bringup=1 + navi33psp=1 + navi33psp_ring=1.
  2. Navi33PspRing.cpp never writes PCI config space.
  3. Every register write goes through the bounds-checked RegBar::wr and
     targets ONLY the ring mailboxes (C2PMSG_64/67/69/70/71) — the load
     stage's C2PMSG_35/36 and the MM window are not this stage's writes.
  4. The ring is refused when the secure OS is not alive (C2PMSG_81 check).
  5. SETUP_TMR only runs with a tmr_size returned by LOAD_TOC, and the TMR
     placement comes from tmrOffsetForMode (no open-coded address).

Run: python -B tests/test_psp_ring_safety.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RING = ROOT / "Navi33PspRing.cpp"
BRINGUP = ROOT / "Navi33Bringup.cpp"

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print(f"FAIL: {msg}")


def main():
    ring = RING.read_text(encoding="utf-8")
    bringup = BRINGUP.read_text(encoding="utf-8")

    # 1. Opt-in gating.
    for arg in ("navi33bringup=1", "navi33psp=1", "navi33psp_ring=1"):
        check(f'"{arg}"' in bringup, f"boot-arg token {arg} must appear in Navi33Bringup.cpp")
    check("RunPspRingStage(pci, this, pspTmrMode())" in bringup,
          "the ring stage must be invoked from start() with the TMR mode gate")

    # 2. No PCI config writes.
    check("configWrite" not in ring, "Navi33PspRing.cpp must never call configWrite")

    # 3. Register writes: only the ring mailboxes.
    sites = re.findall(r"bar\.wr\(([^,;]+),", ring)
    allowed = {
        "mp0ByteOffset(kC2pmsg64Dword)",
        "mp0ByteOffset(kC2pmsg67Dword)",
        "mp0ByteOffset(kC2pmsg69Dword)",
        "mp0ByteOffset(kC2pmsg70Dword)",
        "mp0ByteOffset(kC2pmsg71Dword)",
    }
    unexpected = [s.strip() for s in sites if s.strip() not in allowed]
    check(not unexpected,
          f"register writes must stay on the ring mailboxes; found: {unexpected}")
    check(sites, "the stage must write the ring mailboxes (it is the write stage)")
    # The load stage's C2PMSG_35/36 must not appear as write targets here.
    check("kC2pmsg35Dword" not in ring and "kC2pmsg36Dword" not in ring,
          "C2PMSG_35/36 are the LOAD stage's writes, not the ring stage's")

    # 4. Secure-OS-alive pre-condition.
    check('"aborted: secure OS not alive' in ring,
          "the ring must refuse when the secure OS is not alive")
    check("sosAlive(alive)" in ring, "the sOS sign-of-life read must gate the ring")

    # 5. TMR discipline.
    check("tmrOffsetForMode(" in ring, "TMR placement must go through tmrOffsetForMode")
    check("tocResp[loadTocTmrSizeIndex]" in ring,
          "the TMR size must come from the LOAD_TOC response")
    check('SetupTmrSubmitted", false)' in ring,
          "SETUP_TMR must be skipped when no usable tmr_size exists")

    # 6. The fence protocol must be bounded.
    check("kFenceWaitMs" in ring, "the fence wait must be bounded")
    check("nextWptr(" in ring, "the write pointer must advance through nextWptr")
    check("wptrValid(" in ring, "the write pointer must be validated before use")

    if failures:
        print(f"{len(failures)} contract violation(s)")
        return 1
    print("PSP ring safety contract: OK (writes confined to the ring mailboxes; sOS + TMR gates pinned)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
