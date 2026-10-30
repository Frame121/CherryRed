#!/usr/bin/env python3
"""Cross-check the board-attributed RX 7600 firmware manifest against parsed evidence.

This tool is read-only and hardware-independent. It checks internal consistency
between three evidence files the project already holds:

  1. the board firmware manifest JSON (claimed firmware file list),
  2. the strict-parse JSON sidecar of the captured amdgpu_discovery.bin,
  3. the VBIOS identity record (identity strings, not a ROM dump).

It verifies filename-prefix/version agreement, required-role coverage, explicit
unresolved-HWID bookkeeping, and an optional SHA-256 binding to the source boot
journal. It cannot authenticate the board, prove any blob is loadable on macOS,
or verify redistribution rights. All conclusions are STRUCTURE_ONLY.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any

MAX_JSON_BYTES = 1024 * 1024

EXPECTED_IDENTITY = {
    "pci_vendor": "1002",
    "pci_device": "7480",
    "subsystem_vendor": "148c",
    "subsystem_device": "2421",
}

# HWID numeric values per drivers/gpu/drm/amd/include/soc15_hw_ip.h, as cited in
# tools/ipdisc/evidence/RX7600_IP_DISCOVERY_ANALYSIS_2026-10-30.md. Numbers only;
# never guessed names for unresolved IDs.
FAMILY_HWIDS = {
    "GC": (11,),
    "SDMA": (42, 43),
    "VCN": (12,),
    "PSP": (255,),  # MP0
    "SMU": (1,),    # MP1
}
# DMU (271) corroborates the display record but is not the DCN claim itself.
DMU_HWID = 271

FAMILY_PREFIX_FORMATS = {
    "GC": r"^gc_{v[0]}_{v[1]}_{v[2]}_",
    "PSP": r"^psp_{v[0]}_{v[1]}_{v[2]}_",
    "SMU": r"^smu_{v[0]}_{v[1]}_{v[2]}\.bin$",
    "SDMA": r"^sdma_{v[0]}_{v[1]}_{v[2]}\.bin$",
    "VCN": r"^vcn_{v[0]}_{v[1]}_{v[2]}\.bin$",
    "DCN": r"^dcn_{v[0]}_{v[1]}_{v[2]}_dmcub\.bin$",
}

REQUIRED_ROLES = {
    "GC": {"imu", "me", "mec", "mes", "mes1", "mes2", "pfp", "rlc"},
    "PSP": {"sos", "ta"},
    "SMU": {"smu"},
    "SDMA": {"sdma"},
    "VCN": {"vcn"},
    "DCN": {"dmub"},
}

HEX64 = re.compile(r"^[0-9a-fA-F]{64}$")
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
VBIOS_VERSION_RE = re.compile(r"^\d{3}\.\d{3}\.\d{3}\.\d{3}\.\d{6}$")
ATOM_NAME_RE = re.compile(r"^\d{3}-[A-Z0-9][A-Z0-9-]*$")
FIRMWARE_PATH_RE = re.compile(r"^amdgpu/[A-Za-z0-9._-]+\.bin$")
RELATIVE_PATH_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9/._-]*$")


class ConsistencyError(Exception):
    pass


def _load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise ConsistencyError(f"cannot read {label} {path}: {exc}") from exc
    if len(raw) > MAX_JSON_BYTES:
        raise ConsistencyError(f"{label} exceeds {MAX_JSON_BYTES} bytes: {path}")
    try:
        value = json.loads(raw.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ConsistencyError(f"invalid JSON in {label} {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ConsistencyError(f"{label} top level must be an object: {path}")
    return value


def _version_tuple(text: str, label: str) -> tuple[int, ...]:
    if not VERSION_RE.match(text):
        raise ConsistencyError(f"{label}: malformed version {text!r}")
    return tuple(int(part) for part in text.split("."))


def _artifact_versions(parsed: dict[str, Any]) -> dict[int, set[str]]:
    parse = parsed.get("parse")
    if not isinstance(parse, dict):
        raise ConsistencyError("parsed sidecar has no 'parse' object")
    if parse.get("valid") is not True:
        raise ConsistencyError("parsed sidecar is not marked valid")
    if parse.get("mode") != "strict":
        raise ConsistencyError("parsed sidecar was not produced in strict mode")
    records = parse.get("ips")
    if not isinstance(records, list) or not records:
        raise ConsistencyError("parsed sidecar has no IP records")
    versions: dict[int, set[str]] = {}
    for record in records:
        if not isinstance(record, dict):
            raise ConsistencyError("non-object IP record in sidecar")
        hw_id = record.get("hw_id")
        version = record.get("version")
        if not isinstance(hw_id, int) or not isinstance(version, str):
            raise ConsistencyError("malformed IP record in sidecar")
        versions.setdefault(hw_id, set()).add(version)
    return versions


def _family_versions(
    manifest: dict[str, Any], artifact_versions: dict[int, set[str]]
) -> dict[str, str]:
    ip_blocks = manifest.get("ip_blocks")
    if not isinstance(ip_blocks, dict):
        raise ConsistencyError("manifest has no ip_blocks object")
    resolved: dict[str, str] = {}
    for family, hwids in FAMILY_HWIDS.items():
        block = ip_blocks.get(family)
        if not isinstance(block, dict) or not isinstance(block.get("version"), str):
            raise ConsistencyError(f"manifest ip_blocks.{family} missing version")
        claimed = block["version"]
        observed: set[str] = set()
        for hw_id in hwids:
            observed |= artifact_versions.get(hw_id, set())
        if claimed not in observed:
            raise ConsistencyError(
                f"{family} claimed {claimed} but artifact records {sorted(observed) or 'nothing'} "
                f"for HWIDs {hwids}"
            )
        resolved[family] = claimed
    dmu = ip_blocks.get("DCN")
    if not isinstance(dmu, dict) or not isinstance(dmu.get("version"), str):
        raise ConsistencyError("manifest ip_blocks.DCN missing version")
    resolved["DCN"] = dmu["version"]
    return resolved


def _check_firmware_list(
    manifest: dict[str, Any], family_versions: dict[str, str]
) -> int:
    firmware = manifest.get("firmware")
    if not isinstance(firmware, list) or not firmware:
        raise ConsistencyError("manifest has no firmware list")
    seen_paths: set[str] = set()
    roles_by_family: dict[str, set[str]] = {}
    for entry in firmware:
        if not isinstance(entry, dict):
            raise ConsistencyError("firmware entry is not an object")
        path = entry.get("path")
        ip = entry.get("ip")
        role = entry.get("role")
        source_ref = entry.get("source_ref")
        if not isinstance(path, str) or not FIRMWARE_PATH_RE.match(path):
            raise ConsistencyError(f"malformed firmware path {path!r}")
        if path in seen_paths:
            raise ConsistencyError(f"duplicate firmware path {path}")
        seen_paths.add(path)
        if ".." in path or path.startswith("/"):
            raise ConsistencyError(f"unsafe firmware path {path}")
        if not isinstance(ip, str) or ip not in REQUIRED_ROLES:
            raise ConsistencyError(f"{path}: unknown firmware family {ip!r}")
        if not isinstance(role, str) or not role:
            raise ConsistencyError(f"{path}: missing role")
        if not isinstance(source_ref, str) or not source_ref:
            raise ConsistencyError(f"{path}: missing source_ref")
        pattern = FAMILY_PREFIX_FORMATS[ip].format(
            v=_version_tuple(family_versions[ip], f"{ip} version")
        )
        basename = path[len("amdgpu/"):]
        if not re.match(pattern, basename):
            raise ConsistencyError(
                f"{path}: filename does not match {ip} {family_versions[ip]} "
                f"(expected prefix pattern {pattern})"
            )
        roles_by_family.setdefault(ip, set()).add(role)
    for family, required in REQUIRED_ROLES.items():
        missing = required - roles_by_family.get(family, set())
        if missing:
            raise ConsistencyError(
                f"{family}: missing required roles {sorted(missing)}"
            )
    return len(firmware)


def _check_unresolved_hwids(
    manifest: dict[str, Any], artifact_versions: dict[int, set[str]]
) -> list[dict[str, Any]]:
    unresolved = manifest.get("unresolved_hwids")
    if not isinstance(unresolved, list):
        raise ConsistencyError("manifest has no unresolved_hwids list")
    explicit: dict[int, dict[str, Any]] = {}
    for entry in unresolved:
        if not isinstance(entry, dict):
            raise ConsistencyError("unresolved_hwids entry is not an object")
        hw_id = entry.get("hwid")
        if not isinstance(hw_id, int) or hw_id < 0:
            raise ConsistencyError("unresolved_hwids entry missing numeric hwid")
        label = entry.get("label", "")
        if not isinstance(label, str) or "UNKNOWN" not in label:
            raise ConsistencyError(
                f"HWID {hw_id}: unresolved entries must keep an UNKNOWN label"
            )
        explicit[hw_id] = entry
    for hw_id in (282, 286):
        if hw_id not in explicit:
            raise ConsistencyError(
                f"HWID {hw_id} must stay explicitly catalogued as unresolved"
            )
        observed = artifact_versions.get(hw_id, set())
        claimed = explicit[hw_id].get("version")
        if observed and claimed not in observed:
            raise ConsistencyError(
                f"HWID {hw_id}: claimed {claimed} but artifact records {sorted(observed)}"
            )
    return unresolved


def _check_vbios_identity(vbios: dict[str, Any]) -> str:
    identity = vbios.get("vbios_identity")
    if not isinstance(identity, dict):
        raise ConsistencyError("VBIOS record has no vbios_identity object")
    name = identity.get("atom_bios_name")
    version = identity.get("version_string")
    if not isinstance(name, str) or not ATOM_NAME_RE.match(name):
        raise ConsistencyError(f"malformed ATOM BIOS name {name!r}")
    if not isinstance(version, str) or not VBIOS_VERSION_RE.match(version):
        raise ConsistencyError(f"malformed VBIOS version string {version!r}")
    gpu = vbios.get("gpu")
    if not isinstance(gpu, dict):
        raise ConsistencyError("VBIOS record has no gpu object")
    for key, expected in EXPECTED_IDENTITY.items():
        if gpu.get(key) != expected:
            raise ConsistencyError(
                f"VBIOS record gpu.{key} is {gpu.get(key)!r}, expected {expected!r}"
            )
    provenance = vbios.get("provenance")
    if not isinstance(provenance, dict):
        raise ConsistencyError("VBIOS record has no provenance object")
    source_hash = provenance.get("source_artifact_sha256")
    if not isinstance(source_hash, str) or not HEX64.match(source_hash):
        raise ConsistencyError("VBIOS provenance source hash missing or malformed")
    ipd_hash = provenance.get("related_ip_discovery_sha256")
    if not isinstance(ipd_hash, str) or not HEX64.match(ipd_hash):
        raise ConsistencyError("VBIOS provenance IP Discovery hash missing or malformed")
    if provenance.get("authentication") != "NONE":
        raise ConsistencyError("VBIOS provenance must keep authentication: NONE")
    return name


def _bind_source_log(path: Path | None, expected_sha256: str) -> str:
    if path is None:
        return "SKIPPED"
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise ConsistencyError(f"cannot read source journal {path}: {exc}") from exc
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected_sha256.lower():
        raise ConsistencyError(
            f"source journal hash {digest} does not match recorded {expected_sha256}"
        )
    return f"BOUND ({digest})"


def verify(
    manifest_path: Path,
    parsed_path: Path,
    vbios_path: Path,
    source_log: Path | None = None,
) -> dict[str, Any]:
    manifest = _load_json(manifest_path, "firmware manifest")
    parsed = _load_json(parsed_path, "parsed IP Discovery sidecar")
    vbios = _load_json(vbios_path, "VBIOS identity record")

    artifact_versions = _artifact_versions(parsed)
    family_versions = _family_versions(manifest, artifact_versions)
    firmware_count = _check_firmware_list(manifest, family_versions)
    unresolved = _check_unresolved_hwids(manifest, artifact_versions)
    atom_name = _check_vbios_identity(vbios)

    expected_ipd_sha = manifest.get("provenance", {}).get("ip_discovery_sha256")
    recorded_ipd_sha = parsed.get("analysis", {}).get("computed_sha256")
    if not isinstance(expected_ipd_sha, str) or not HEX64.match(expected_ipd_sha):
        raise ConsistencyError("manifest provenance ip_discovery_sha256 malformed")
    if (
        isinstance(recorded_ipd_sha, str)
        and recorded_ipd_sha.lower() != expected_ipd_sha.lower()
    ):
        raise ConsistencyError(
            "manifest IP Discovery hash does not match the parsed sidecar"
        )

    binding = _bind_source_log(
        source_log,
        vbios.get("provenance", {}).get("source_artifact_sha256", ""),
    )

    return {
        "scope": "STRUCTURE_ONLY",
        "result": "PASS",
        "gpu": manifest.get("gpu"),
        "artifact_ip_records": sum(len(v) for v in artifact_versions.values()),
        "firmware_entries_checked": firmware_count,
        "family_versions": family_versions,
        "unresolved_hwids": [entry.get("hwid") for entry in unresolved],
        "vbios_identity": atom_name,
        "source_journal_binding": binding,
        "limits": [
            "Consistency between project-held evidence files only.",
            "Does not authenticate the board, the capture, or any firmware blob.",
            "Does not prove firmware is loadable, licensed, or macOS-compatible.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--parsed", required=True, type=Path)
    parser.add_argument("--vbios", required=True, type=Path)
    parser.add_argument("--source-log", type=Path, default=None)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = verify(args.manifest, args.parsed, args.vbios, args.source_log)
    except ConsistencyError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"PASS: {report['firmware_entries_checked']} firmware entries consistent")
        print(f"Families: {report['family_versions']}")
        print(f"VBIOS: {report['vbios_identity']}")
        print(f"Unresolved HWIDs kept explicit: {report['unresolved_hwids']}")
        print(f"Source journal: {report['source_journal_binding']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
