#!/usr/bin/env python3
"""Validate the architecture and Mach-O file type of a linked kernel-module binary."""
from pathlib import Path
import struct
import sys

MH_BUNDLE = 0x8
MH_KEXT_BUNDLE = 0xB
CPU_TYPE_X86_64 = 0x01000007


def main() -> None:
    if len(sys.argv) not in (2, 3):
        raise SystemExit("usage: verify_kext_binary.py <Mach-O> [expected-filetype]")
    path = Path(sys.argv[1])
    data = path.read_bytes()
    if len(data) < 32:
        raise SystemExit("FAIL: file is shorter than a 64-bit Mach-O header")
    magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved = struct.unpack_from("<IiiIIIII", data, 0)
    if magic != 0xFEEDFACF:
        raise SystemExit(f"FAIL: unexpected Mach-O magic 0x{magic:08x}")
    if cputype != CPU_TYPE_X86_64:
        raise SystemExit(f"FAIL: expected x86_64 cputype 0x{CPU_TYPE_X86_64:x}, got 0x{cputype:x}")
    if 32 + sizeofcmds > len(data):
        raise SystemExit("FAIL: load-command region extends beyond the file")
    expected = int(sys.argv[2], 0) if len(sys.argv) == 3 else MH_KEXT_BUNDLE
    labels = {MH_BUNDLE: "MH_BUNDLE (cross-link-check only)", MH_KEXT_BUNDLE: "MH_KEXT_BUNDLE"}
    actual_label = labels.get(filetype, f"Mach-O filetype 0x{filetype:x}")
    expected_label = labels.get(expected, f"filetype 0x{expected:x}")
    if filetype != expected:
        raise SystemExit(f"FAIL: expected {expected_label}, got {actual_label}")
    if expected == MH_KEXT_BUNDLE:
        print(f"PASS: {path} is x86_64 MH_KEXT_BUNDLE ({len(data)} bytes)")
    elif expected == MH_BUNDLE:
        print(f"PASS: {path} is x86_64 MH_BUNDLE ({len(data)} bytes); Apple KEXT filetype still requires native ld -kext")
    else:
        print(f"PASS: {path} has expected {expected_label} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
