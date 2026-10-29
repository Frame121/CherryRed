#!/usr/bin/env python
"""Host test for tools/dcn321/gen_regs.py (the DCN 3.2.1 register generator).

Runs on the host (no macOS, no real DCN 3.2.1 headers). It builds a SYNTHETIC
offset/sh_mask fixture with known values and proves the generator:
  - emits per-instance offsets and BASE_IDX straight from the input,
  - emits field masks/shifts from the instance-0 name,
  - REFUSES (SystemExit) on a missing instance, disagreeing BASE_IDX, or a
    non-instance-0 mask/shift that differs.
This is a synthetic-fixture test: it proves the generator LOGIC, NOT that any
DCN 3.2.1 register offset is known. The real header is generated on Mac-day
from the real dcn_3_2_1_offset.h / dcn_3_2_1_sh_mask.h.
"""
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GEN = ROOT / "tools" / "dcn321" / "gen_regs.py"


def load_gen():
    spec = importlib.util.spec_from_file_location("dcn321_gen_regs", GEN)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    gen = load_gen()
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    # A minimal synthetic spec: one 4-instance OTG register + one single-instance DMCUB.
    spec = [
        ["OTG", "OTG{i}_", 4, "OTG_CONTROL", ["OTG_MASTER_EN"]],
        ["DMCUB", None, 1, "DMCUB_CNTL", ["DMCUB_ENABLE"]],
    ]

    # Synthetic offset header: OTG0..OTG3 at 0x1000..0x1040 (BASE_IDX 2), DMCUB at 0x2000 (BASE_IDX 0).
    offset_text = "\n".join(
        [
            "#define regOTG0_OTG_CONTROL 0x1000",
            "#define regOTG0_OTG_CONTROL_BASE_IDX 2",
            "#define regOTG1_OTG_CONTROL 0x1040",
            "#define regOTG1_OTG_CONTROL_BASE_IDX 2",
            "#define regOTG2_OTG_CONTROL 0x1080",
            "#define regOTG2_OTG_CONTROL_BASE_IDX 2",
            "#define regOTG3_OTG_CONTROL 0x10c0",
            "#define regOTG3_OTG_CONTROL_BASE_IDX 2",
            "#define regDMCUB_CNTL 0x2000",
            "#define regDMCUB_CNTL_BASE_IDX 0",
        ]
    )
    # Synthetic sh_mask header: instance-0 names, with all instances agreeing.
    shmask_text = "\n".join(
        [
            "#define OTG0_OTG_CONTROL__OTG_MASTER_EN_MASK 0x00000001",
            "#define OTG0_OTG_CONTROL__OTG_MASTER_EN__SHIFT 0",
            "#define OTG1_OTG_CONTROL__OTG_MASTER_EN_MASK 0x00000001",
            "#define OTG1_OTG_CONTROL__OTG_MASTER_EN__SHIFT 0",
            "#define OTG2_OTG_CONTROL__OTG_MASTER_EN_MASK 0x00000001",
            "#define OTG2_OTG_CONTROL__OTG_MASTER_EN__SHIFT 0",
            "#define OTG3_OTG_CONTROL__OTG_MASTER_EN_MASK 0x00000001",
            "#define OTG3_OTG_CONTROL__OTG_MASTER_EN__SHIFT 0",
            "#define DMCUB_CNTL__DMCUB_ENABLE_MASK 0x00000001",
            "#define DMCUB_CNTL__DMCUB_ENABLE__SHIFT 0",
        ]
    )

    # 1. Happy path: offsets and masks are emitted straight from the input.
    out = gen.generate(offset_text, shmask_text, spec, "DCN321")
    check("#define DCN321_OTG_OTG_CONTROL(0) " in out.replace("(i)", "(0)") or
          "DCN321_OTG_OTG_CONTROL(i)" in out, "OTG per-instance macro missing")
    check("0x1000u" in out and "0x1040u" in out and "0x1080u" in out and "0x10c0u" in out,
          "OTG per-instance offsets not emitted verbatim")
    check("#define DCN321_OTG_OTG_CONTROL_BASE_IDX 2u" in out, "OTG BASE_IDX missing")
    check("#define DCN321_OTG_OTG_CONTROL__OTG_MASTER_EN_MASK 0x00000001u" in out,
          "OTG field mask missing")
    check("#define DCN321_DMCUB_CNTL 0x2000u" in out, "single-instance DMCUB offset missing")
    check("#define DCN321_DMCUB_CNTL_BASE_IDX 0u" in out, "DMCUB BASE_IDX missing")
    check("#define DCN321_MAX_OFFSET_BASE2 0x10c0u" in out, "max offset per BASE_IDX missing")
    check("#define DCN321_BAD_OFFSET 0xFFFFFFFFu" in out, "BAD_OFFSET sentinel missing")

    # 2. Fail-closed: a missing instance must refuse.
    bad_missing = offset_text.replace("#define regOTG3_OTG_CONTROL 0x10c0", "").replace(
        "#define regOTG3_OTG_CONTROL_BASE_IDX 2", "")
    try:
        gen.generate(bad_missing, shmask_text, spec, "DCN321")
        check(False, "missing instance did not refuse")
    except SystemExit:
        pass

    # 3. Fail-closed: instances disagreeing on BASE_IDX must refuse.
    bad_base = offset_text.replace("#define regOTG1_OTG_CONTROL_BASE_IDX 2",
                                   "#define regOTG1_OTG_CONTROL_BASE_IDX 3")
    try:
        gen.generate(bad_base, shmask_text, spec, "DCN321")
        check(False, "disagreeing BASE_IDX did not refuse")
    except SystemExit:
        pass

    # 4. Fail-closed: a non-instance-0 mask differing from instance 0 must refuse.
    bad_mask = shmask_text.replace("#define OTG1_OTG_CONTROL__OTG_MASTER_EN_MASK 0x00000001",
                                   "#define OTG1_OTG_CONTROL__OTG_MASTER_EN_MASK 0x00000002")
    try:
        gen.generate(offset_text, bad_mask, spec, "DCN321")
        check(False, "disagreeing mask did not refuse")
    except SystemExit:
        pass

    if failures:
        print("DCN321 GEN REGS TEST: FAIL")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print("DCN321 GEN REGS TEST: PASS (synthetic fixture: emits verbatim, refuses on missing/disagreeing)")


if __name__ == "__main__":
    main()
