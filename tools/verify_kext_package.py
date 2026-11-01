#!/usr/bin/env python3
"""Check the file layout and module identity of a prepared KEXT package."""
from pathlib import Path
import plistlib
import sys


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_kext_package.py <KEXT.bundle>")
    root = Path(sys.argv[1])
    info_path = root / "Contents" / "Info.plist"
    if not info_path.is_file():
        raise SystemExit(f"FAIL: missing {info_path}")
    info = plistlib.loads(info_path.read_bytes())
    if info.get("CFBundlePackageType") != "KEXT":
        raise SystemExit("FAIL: CFBundlePackageType must be KEXT")
    executable_name = info.get("CFBundleExecutable")
    if not executable_name:
        raise SystemExit("FAIL: CFBundleExecutable is missing")
    executable = root / "Contents" / "MacOS" / executable_name
    if not executable.is_file():
        raise SystemExit(f"FAIL: missing executable {executable}")
    bundle_id = info.get("CFBundleIdentifier")
    if not bundle_id or bundle_id.encode() not in executable.read_bytes():
        raise SystemExit("FAIL: CFBundleIdentifier is missing or does not match the kmod_info identifier")
    personalities = info.get("IOKitPersonalities", {})
    for name, personality in personalities.items():
        if personality.get("CFBundleIdentifier") != bundle_id:
            raise SystemExit(f"FAIL: personality {name} has a mismatched CFBundleIdentifier")
    print(f"PASS: {root} package identity {bundle_id}, executable {executable_name}")
    for name, personality in personalities.items():
        pci = personality.get("IOPCIPrimaryMatch")
        if pci:
            print(f"  {name}: PCI match {pci}")


if __name__ == "__main__":
    main()
