#!/usr/bin/env python3
"""Verify on-disk RX 7600 GFX11 evidence and firmware hashes without touching hardware.

The JSON manifest is a claim supplied by its author. This tool checks identity
fields, file containment, sizes and SHA-256 values; it cannot authenticate the
board, upstream source, firmware licence, or the truth of human review flags.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import stat
import sys
from pathlib import Path
from typing import Any

MAX_FIRMWARE_BYTES = 64 * 1024 * 1024
MAX_EVIDENCE_BYTES = 1024 * 1024
EXPECTED_TARGET = {
    "vendor_id": "1002",
    "device_id": "7480",
    "subsystem_vendor_id": "148c",
    "subsystem_id": "2421",
}
ROLES = {
    "psp_sos": ("psp", "psp", True),
    "psp_ta": ("psp", "psp", False),
    "smu": ("smu", "smu", True),
    "gc_pfp": ("gc", "gc", True),
    "gc_me": ("gc", "gc", True),
    "gc_mec": ("gc", "gc", True),
    "gc_rlc": ("gc", "gc", True),
    "sdma": ("sdma", "sdma", True),
}
HEX64 = re.compile(r"^[0-9a-fA-F]{64}$")


class VerificationError(Exception):
    pass


def _load_json(path: Path) -> dict[str, Any]:
    try:
        raw = path.read_text(encoding="utf-8")
        value = json.loads(raw)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise VerificationError(f"cannot read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise VerificationError(f"top-level JSON must be an object: {path}")
    return value


def _safe_file(root: Path, relative: str, limit: int) -> tuple[Path, bytes]:
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise VerificationError("evidence paths must be non-empty relative paths")
    candidate = root / relative
    try:
        resolved = candidate.resolve(strict=True)
        resolved.relative_to(root.resolve(strict=True))
        info = resolved.stat()
    except (OSError, ValueError) as exc:
        raise VerificationError(f"unsafe or missing evidence file {relative!r}: {exc}") from exc
    if not stat.S_ISREG(info.st_mode) or candidate.is_symlink():
        raise VerificationError(f"evidence path is not a regular non-symlink file: {relative}")
    if info.st_size <= 0 or info.st_size > limit:
        raise VerificationError(f"evidence file size outside 1..{limit} bytes: {relative}")
    data = resolved.read_bytes()
    if len(data) != info.st_size:
        raise VerificationError(f"evidence file changed while reading: {relative}")
    return resolved, data


def _check_hash(label: str, expected: Any, data: bytes) -> str:
    if not isinstance(expected, str) or not HEX64.fullmatch(expected):
        raise VerificationError(f"{label}: sha256 must contain exactly 64 hex characters")
    actual = hashlib.sha256(data).hexdigest()
    if actual.lower() != expected.lower():
        raise VerificationError(f"{label}: SHA-256 mismatch (actual {actual})")
    return actual


def _version(value: Any, name: str) -> tuple[int, int, int]:
    if not isinstance(value, dict):
        raise VerificationError(f"{name} version must be an object")
    parts = []
    for key in ("major", "minor", "revision"):
        item = value.get(key)
        if not isinstance(item, int) or isinstance(item, bool) or not 0 <= item <= 255:
            raise VerificationError(f"{name}.{key} must be an integer in 0..255")
        parts.append(item)
    if parts[0] == 0:
        raise VerificationError(f"{name}.major cannot be zero")
    return tuple(parts)


def _prefix(family: str, version: tuple[int, int, int], trailing: bool) -> str:
    result = f"{family}_{version[0]}_{version[1]}_{version[2]}"
    return result + "_" if trailing else result


def verify_manifest(manifest_path: Path) -> dict[str, Any]:
    manifest_path = manifest_path.resolve(strict=True)
    root = manifest_path.parent
    manifest = _load_json(manifest_path)

    target = manifest.get("target")
    if not isinstance(target, dict):
        raise VerificationError("target identity object is required")
    for key, expected in EXPECTED_TARGET.items():
        actual = target.get(key)
        if not isinstance(actual, str) or actual.lower().removeprefix("0x").zfill(4) != expected:
            raise VerificationError(f"target.{key} must identify RX 7600 board value {expected}")

    if manifest.get("evidence_level") != "board_attributed":
        raise VerificationError("evidence_level must be board_attributed")
    vbios = manifest.get("vbios")
    if not isinstance(vbios, dict) or vbios.get("identity_matched") is not True:
        raise VerificationError("VBIOS identity must be explicitly matched")
    _safe_file(root, vbios.get("file"), MAX_EVIDENCE_BYTES)
    _, vbios_data = _safe_file(root, vbios.get("file"), MAX_EVIDENCE_BYTES)
    vbios_hash = _check_hash("vbios", vbios.get("sha256"), vbios_data)

    discovery = manifest.get("ip_discovery")
    if not isinstance(discovery, dict):
        raise VerificationError("ip_discovery evidence object is required")
    if discovery.get("artifact_class") != "physical_capture":
        raise VerificationError("IP Discovery must be a physical_capture, not a fixture/example")
    if not isinstance(discovery.get("acquisition_method"), str) or not discovery["acquisition_method"].strip():
        raise VerificationError("IP Discovery acquisition_method is required")
    _, discovery_data = _safe_file(root, discovery.get("file"), MAX_EVIDENCE_BYTES)
    discovery_hash = _check_hash("ip_discovery", discovery.get("sha256"), discovery_data)

    versions = {
        name: _version(manifest.get("ip_versions", {}).get(name), name)
        for name in ("psp", "smu", "gc", "sdma")
    }
    expected_major = {"psp": 13, "smu": 13, "gc": 11, "sdma": 6}
    for name, version in versions.items():
        if version[0] != expected_major[name]:
            raise VerificationError(f"{name} major version is not in the reviewed GFX11 family policy")

    firmware = manifest.get("firmware")
    if not isinstance(firmware, list) or not firmware or len(firmware) > len(ROLES):
        raise VerificationError("firmware must be a non-empty array of at most 8 records")
    seen: set[str] = set()
    verified: list[dict[str, Any]] = []
    total_bytes = 0
    for record in firmware:
        if not isinstance(record, dict):
            raise VerificationError("each firmware record must be an object")
        role = record.get("role")
        if role not in ROLES:
            raise VerificationError(f"unsupported firmware role: {role!r}")
        if role in seen:
            raise VerificationError(f"duplicate firmware role: {role}")
        seen.add(role)
        family, version_name, required = ROLES[role]
        if record.get("board_attributed") is not True or record.get("license_approved") is not True:
            raise VerificationError(f"{role}: board attribution and human licence approval are required")
        if not isinstance(record.get("source_revision"), str) or not record["source_revision"].strip():
            raise VerificationError(f"{role}: source_revision is required")
        license_evidence = record.get("license_evidence")
        if not isinstance(license_evidence, str) or not license_evidence.strip():
            raise VerificationError(f"{role}: license_evidence reference is required")
        path, data = _safe_file(root, record.get("file"), MAX_FIRMWARE_BYTES)
        actual_hash = _check_hash(role, record.get("sha256"), data)
        if record.get("byte_length") != len(data):
            raise VerificationError(f"{role}: byte_length does not match file")
        prefix = _prefix(family, versions[version_name], role not in ("smu", "sdma"))
        if not path.name.startswith(prefix):
            raise VerificationError(f"{role}: filename does not match expected prefix {prefix!r}")
        total_bytes += len(data)
        verified.append({"role": role, "file": str(path.relative_to(root)), "bytes": len(data), "sha256": actual_hash})

    missing = sorted(role for role, (_, _, required) in ROLES.items() if required and role not in seen)
    if missing:
        raise VerificationError("missing required firmware roles: " + ", ".join(missing))

    return {
        "status": "STRUCTURE_AND_HASHES_VERIFIED_ONLY",
        "target": EXPECTED_TARGET,
        "vbios_sha256": vbios_hash,
        "ip_discovery_sha256": discovery_hash,
        "firmware_records": verified,
        "firmware_total_bytes": total_bytes,
        "limitations": [
            "This tool does not authenticate the board, capture source, firmware source, or licence claims.",
            "A human must review the referenced licence evidence and source revision.",
            "No firmware is loaded and no GPU, PCI, BAR, MMIO, EFI, or kernel extension is accessed.",
            "Passing this tool does not authorize GPU initialization or establish macOS support.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args(argv)
    try:
        result = verify_manifest(args.manifest)
    except (VerificationError, OSError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
