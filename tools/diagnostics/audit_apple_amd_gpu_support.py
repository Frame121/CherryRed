#!/usr/bin/env python3
"""Read-only audit of Apple AMD GPU PCI matches and current Metal services."""
from __future__ import annotations

import plistlib
import re
import subprocess
from pathlib import Path

TARGET_VENDOR = 0x1002
TARGET_DEVICE = 0x7480
TARGET_PACKED = (TARGET_DEVICE << 16) | TARGET_VENDOR

BUNDLES = (
    Path("/System/Library/Extensions/AMDRadeonX6000.kext/Contents/Info.plist"),
    Path("/System/Library/Extensions/AMDRadeonX6000Framebuffer.kext/Contents/Info.plist"),
    Path("/System/Library/Extensions/AMDRadeonX6000HWServices.kext/Contents/Info.plist"),
)


def command(args: list[str]) -> str:
    try:
        result = subprocess.run(args, check=False, capture_output=True, text=True)
        return result.stdout + result.stderr
    except OSError as exc:
        return f"unavailable: {exc}"


def match_token(token: str) -> tuple[int, int] | None:
    token = token.strip()
    if "&" in token:
        left, right = token.split("&", 1)
        try:
            return int(left, 0), int(right, 0)
        except ValueError:
            return None
    try:
        return int(token, 0), 0xFFFFFFFF
    except ValueError:
        return None


def inspect_bundle(path: Path) -> bool:
    print(f"\n[{path}]")
    if not path.is_file():
        print("  MISSING")
        return False
    try:
        with path.open("rb") as stream:
            info = plistlib.load(stream)
    except Exception as exc:
        print(f"  plist parse failed: {exc}")
        return False

    print(f"  bundle: {info.get('CFBundleIdentifier', 'unknown')} "
          f"version={info.get('CFBundleVersion', 'unknown')}")
    personalities = info.get("IOKitPersonalities", {})
    target_matches: list[str] = []
    all_ids: set[int] = set()

    for name, personality in personalities.items():
        if not isinstance(personality, dict):
            continue
        raw = personality.get("IOPCIMatch", "")
        values = raw if isinstance(raw, (list, tuple)) else str(raw).split()
        for value in values:
            parsed = match_token(str(value))
            if parsed is None:
                continue
            ident, mask = parsed
            # IOPCIMatch encodes device in high 16 bits and vendor in low 16 bits.
            if (TARGET_PACKED & mask) == (ident & mask):
                target_matches.append(
                    f"{name} (IOClass={personality.get('IOClass', 'unknown')}, "
                    f"match={value})"
                )
            if mask & 0xFFFFFFFF == 0xFFFFFFFF:
                all_ids.add(ident)
    if target_matches:
        print("  TARGET MATCH: YES")
        for match in target_matches:
            print(f"    {match}")
    else:
        print("  TARGET MATCH: NO (1002:7480 is not in this bundle's IOPCIMatch)")
    print("  exact PCI IDs:", " ".join(
        f"{ident & 0xFFFF:04x}:{ident >> 16:04x}" for ident in sorted(all_ids)
    ) or "(none)")
    return bool(target_matches)


def main() -> int:
    print("CherryRed read-only Apple AMD driver compatibility audit")
    print(f"Target PCI identity: {TARGET_VENDOR:04x}:{TARGET_DEVICE:04x}")
    matches = [inspect_bundle(path) for path in BUNDLES]
    print("\n[Runtime state]")
    for title, args in (
        ("Metal devices", ["/usr/bin/swift", "-e",
         "import Metal; print(MTLCopyAllDevices().map{$0.name})"]),
        ("IOAccelerator services", ["/usr/sbin/ioreg", "-r", "-c", "IOAccelerator", "-l"]),
        ("loaded AMD graphics drivers", ["/usr/bin/kmutil", "showloaded"]),
    ):
        output = command(args)
        if title == "loaded AMD graphics drivers":
            output = "\n".join(
                line for line in output.splitlines()
                if re.search(r"AMD|Radeon|Lilu|WhateverGreen", line, re.I)
            )
        print(f"  {title}:")
        print("\n".join("    " + line for line in output.splitlines()[:35]) or "    (none)")
    print("\nConclusion:")
    if any(matches):
        print("  At least one inspected Apple AMD bundle declares a PCI match.")
    else:
        print("  No inspected Apple AMD accelerator/framebuffer/HW-services bundle")
        print("  declares PCI match for RX 7600 1002:7480.")
    print("  This audit reads metadata only; it does not modify the OS, load drivers,")
    print("  write PCI registers, reset the GPU, or claim Metal acceleration.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
