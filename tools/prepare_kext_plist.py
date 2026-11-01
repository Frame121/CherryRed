#!/usr/bin/env python3
"""Prepare a target-specific Info.plist without changing the source template."""
from pathlib import Path
import argparse
import plistlib


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--navi33", action="store_true")
    target.add_argument("--default", action="store_true")
    args = parser.parse_args()

    data = plistlib.loads(args.source.read_bytes())
    if args.navi33:
        bundle_id = "org.rednavi.RedNavi.Navi33"
        data["CFBundleIdentifier"] = bundle_id
        data["CFBundleName"] = "RedNavi-Navi33"
        data["CFBundleVersion"] = "0.0.1"
        personalities = data.get("IOKitPersonalities", {})
        for personality in personalities.values():
            personality["CFBundleIdentifier"] = bundle_id
            personality["IOPCIPrimaryMatch"] = "0x74801002"
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(plistlib.dumps(data, fmt=plistlib.FMT_XML, sort_keys=False))
    check = plistlib.loads(args.destination.read_bytes())
    if args.navi33:
        matches = [p.get("IOPCIPrimaryMatch") for p in check.get("IOKitPersonalities", {}).values()]
        if matches != ["0x74801002"]:
            raise SystemExit("Navi33 plist PCI matching did not narrow to RX 7600 (1002:7480)")
    print(f"Prepared {args.destination} for {'Navi 33 RX 7600' if args.navi33 else 'default multi-target'}")


if __name__ == "__main__":
    main()
