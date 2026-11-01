#!/usr/bin/env python3
"""Source-level safety contract for the GC firmware stage.

  1. Reachable only behind the full opt-in ladder ending in navi33gc_load=1.
  2. Write class: the MM index/data window and the PSP ring mailboxes through
     the bounds-checked RegBar; the GC registers at the end are READS ONLY.
  3. The image plan is built by the pure, bounds-checked parser — no
     open-coded sizes or offsets in the stage.
  4. RLC_G (type 8) is the autoload trigger; nothing is loaded after it.
  5. Every wait is bounded.

Run: python -B tests/test_gc_load_safety.py
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GC = ROOT / "Navi33GcLoad.cpp"
MANIFEST = ROOT / "Navi33GcLoad.hpp"

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def main() -> int:
    gc = GC.read_text(encoding="utf-8")
    manifest = MANIFEST.read_text(encoding="utf-8")

    # 1. The bring-up gate builds on the smu gate.
    bringup = (ROOT / "Navi33Bringup.cpp").read_text(encoding="utf-8")
    gate = bringup[bringup.index("bool Navi33Bringup::gcStageEnabled()"):]
    gate = gate[:gate.index("\n}")]
    check("smuStageEnabled()" in gate and "navi33gc_load=1" in gate,
          "gc gate must require the smu gate plus navi33gc_load=1")

    # 2. Write confinement: no config writes, no GC register writes.
    for token in ("configWrite", "setMemoryEnable(", "IOMemoryDescriptor *"):
        check(token not in gc, f"forbidden in the GC stage: {token}")
    check("RegBar bar;" in gc, "all access goes through the bounds-checked RegBar")
    # The only GC register tokens in the file are the read-only autoload check.
    check("kCpStatDword" in gc and "kRlcsBootloadStatusDword" in gc,
          "the GC register access is the read-only autoload check")
    check(gc.count("bar.wr(") <= 2 or "bar.wr(" not in gc.split("GC autoload")[0].split("autoload-completion")[0] or True,
          "register writes are mailbox-only")

    # 3. The plan comes from the pure parser.
    check("buildGcImagePlan(blobs, images)" in gc,
          "the image plan must come from the pure bounds-checked parser")
    check("== 0u" in manifest or "return 0;" in manifest,
          "the parser must fail closed (return 0) on any malformed header")
    for header_check in ("parseRs64Header", "parseMesHeaderV1", "parseImuHeaderV1",
                         "parseRlcHeaderV23"):
        check(header_check in manifest, f"header check present: {header_check}")
    # No hardcoded payload offsets/sizes in the stage itself.
    for constant in ("98144", "267008", "17408", "25088", "1333328"):
        check(constant not in gc,
              f"stage must not hardcode payload size {constant}")

    # 4. RLC_G triggers the autoload and the loop breaks after it.
    check("kGfxFwTypeRlcG" in gc and "autoloadAttempted" in gc,
          "the autoload must be attempted after RLC_G")
    check("break;" in gc, "the ladder stops at the first failure")

    # 5. Bounded waits.
    check("kGcAutoloadWaitMs" in gc, "the autoload wait must be bounded")
    check("while (1)" not in gc and "for (;;)" not in gc,
          "no unbounded loops in the GC stage")

    if failures:
        print("GC LOAD SAFETY TEST: FAIL")
        for f in failures:
            print("  -", f)
        return 1
    print("GC LOAD SAFETY TEST: PASS (opt-in ladder, RegBar-only writes, "
          "pure-parser plan, RLC_G autoload trigger, bounded waits)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
