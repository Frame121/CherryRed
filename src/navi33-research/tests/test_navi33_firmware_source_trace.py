#!/usr/bin/env python3
"""Check RX7600 firmware manifest references against the retained Linux source snapshot.

This validates static source/name correlation only. It is not evidence that the
firmware was requested, loaded, accepted, or is compatible with macOS.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "RX7600-Linux-Evidence" / "navi33-firmware-manifest.json"
SOURCE_ROOT = (
    ROOT / "RX7600-Linux-Evidence" / "source-investigation"
    / "arch-linux-v7.2.11" / "drivers" / "gpu" / "drm" / "amd"
)


def main() -> int:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    failures = []
    checked = 0
    for entry in manifest.get("firmware", []):
        relative = entry.get("source_ref", "")
        blob_path = entry.get("path", "")
        blob_name = Path(blob_path).name
        source = SOURCE_ROOT / relative
        if not relative or not source.is_file():
            failures.append(f"missing source_ref file: {relative!r} ({blob_name})")
            continue
        content = source.read_text(encoding="utf-8", errors="replace")
        if blob_name not in content:
            failures.append(
                f"{relative} does not contain the manifest firmware name {blob_name}"
            )
            continue
        checked += 1

    if failures:
        print("NAVI33 FIRMWARE SOURCE TRACE: FAIL")
        for failure in failures:
            print(" -", failure)
        return 1

    print(
        "NAVI33 FIRMWARE SOURCE TRACE: PASS "
        f"({checked} manifest entries referenced by their source_ref file)"
    )
    print("Static source correlation only; no runtime load or macOS compatibility is implied.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
