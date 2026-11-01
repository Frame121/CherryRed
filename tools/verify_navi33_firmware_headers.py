#!/usr/bin/env python3
"""Verify the locally generated RX7600 firmware wrapper versions and contents.

This checks header metadata and required image entries only. It does not load or
execute firmware on a GPU and does not validate PSP/SMU hardware initialization.
"""
from pathlib import Path
import re
import sys


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def main() -> None:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("RedNavi")
    psp = (root / "psp_fw_navi33.h").read_text()
    smu = (root / "smu_fw_navi33.h").read_text()
    gc = (root / "gc_fw_navi33.h").read_text()
    dmub = (root / "dmub_fw_navi33.h").read_text()

    require(re.search(r'\{"SOS",\s*0x20000,\s*0x330024,', psp) is not None,
            "PSP SOS version must match RX7600 runtime 0x00330024")
    required_psp = ["KDB", "SPL", "SYS_DRV", "SOS"]
    positions = [psp.find(f'"{name}"') for name in required_psp]
    require(all(pos >= 0 for pos in positions), "PSP chain must include KDB, SPL, SYS_DRV and SOS")
    require(positions == sorted(positions), "PSP chain order must be KDB -> SPL -> SYS_DRV -> SOS")
    require(re.search(r'\{"SPL",\s*0x10000000,', psp) is not None,
            "PSP SPL must use PSP_BL__LOAD_TOS_SPL_TABLE (0x10000000)")
    require(re.search(r"#define\s+kRNSmuFwVersion\s+0x526100u", smu) is not None,
            "SMU firmware version must match RX7600 runtime 82.97.0 / 0x00526100")
    require(re.search(r"#define\s+kRNDmubFwVersion\s+0x7003600u", dmub) is not None,
            "DMCUB version must match RX7600 runtime 0x07003600")
    require(re.search(r"#define\s+kRNDmubVbiosSize\s+57856u", dmub) is not None,
            "DMCUB VBIOS window must match the captured 57,856-byte RX7600 ROM")
    require(re.search(r"#define\s+kRNGcFwCount\s+26u", gc) is not None,
            "GC/SDMA firmware manifest must contain 26 extracted images")
    for name in ("SDMA_UCODE_TH0", "CP_RS64_PFP", "CP_RS64_ME", "CP_RS64_MEC",
                 "CP_MES", "CP_MES1", "IMU_I", "IMU_D", "RLC_G"):
        require(f'"{name}"' in gc, f"GC firmware manifest missing {name}")

    print("Navi33 firmware wrapper verification: PASS")
    print("  PSP SOS 0x00330024; SMU 0x00526100; DMCUB 0x07003600")
    print("  PSP chain: KDB -> SPL -> SYS_DRV -> SOS")
    print("  GC 11.0.2 + SDMA 6.0.2 manifest: 26 images")
    print("  This is local header verification, not a hardware firmware-init test.")


if __name__ == "__main__":
    main()
