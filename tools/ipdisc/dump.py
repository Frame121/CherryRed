#!/usr/bin/env python3
"""Offline command-line IP Discovery parser."""

import argparse
import hashlib
import json
import os
import re
import stat
import sys
from pathlib import Path

from ipdiscovery import HwId, IPDiscovery, ValidationMode


MAX_INPUT_SIZE = 1024 * 1024
READ_CHUNK_SIZE = 64 * 1024
PROVENANCE_REQUIRED_FIELDS = {
    "evidence_id",
    "claim",
    "artifact_path",
    "source_repository",
    "source_revision",
    "source_file_or_section",
    "checksum",
    "applies_to",
    "verification_method",
    "verification_status",
    "limitations",
    "artifact_class",
}
PROVENANCE_STATUSES = {
    "UNVERIFIED",
    "SOURCE_VERIFIED",
    "BOARD_SPECIFIC",
    "TARGET_TESTED",
    "BLOCKED",
}
ARTIFACT_CLASSES = {
    "PHYSICAL_CAPTURE",
    "SYNTHETIC_FIXTURE",
    "GENERIC_UPSTREAM_EXAMPLE",
    "UNKNOWN",
}


class InputTooLargeError(Exception):
    pass


class EvidenceMetadataError(Exception):
    pass


class InputChangedError(Exception):
    pass


def _read_bounded(stream, max_size: int) -> bytes:
    if isinstance(max_size, bool) or not isinstance(max_size, int) or max_size < 1:
        raise ValueError("max_size must be a positive integer")

    chunks = []
    total = 0
    while total <= max_size:
        request_size = min(READ_CHUNK_SIZE, max_size + 1 - total)
        chunk = stream.read(request_size)
        if not chunk:
            break
        if len(chunk) > request_size:
            raise OSError("Input stream returned more bytes than requested")
        chunks.append(chunk)
        total += len(chunk)
        if total > max_size:
            raise InputTooLargeError(
                f"input exceeds maximum size of {max_size} bytes"
            )
    return b"".join(chunks)


def _read_input_file(path: Path, max_size: int = MAX_INPUT_SIZE) -> bytes:
    if isinstance(max_size, bool) or not isinstance(max_size, int) or max_size < 1:
        raise ValueError("max_size must be a positive integer")

    with path.open("rb") as stream:
        initial_stat = os.fstat(stream.fileno())
        if not stat.S_ISREG(initial_stat.st_mode):
            raise OSError("input is not a regular file")
        if initial_stat.st_size > max_size:
            raise InputTooLargeError(
                f"input exceeds maximum size of {max_size} bytes"
            )

        data = _read_bounded(stream, max_size)
        final_stat = os.fstat(stream.fileno())

    if final_stat.st_size > max_size:
        raise InputTooLargeError(
            f"input exceeds maximum size of {max_size} bytes"
        )
    if (
        final_stat.st_size != initial_stat.st_size
        or final_stat.st_mtime_ns != initial_stat.st_mtime_ns
        or final_stat.st_ctime_ns != initial_stat.st_ctime_ns
        or len(data) != final_stat.st_size
    ):
        raise InputChangedError("input file changed while it was being read")
    return data


def _reject_duplicate_json_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise EvidenceMetadataError(
                f"duplicate provenance field: {key}"
            )
        result[key] = value
    return result


def _read_provenance_file(path: Path, artifact: bytes) -> dict:
    try:
        raw = _read_input_file(path)
        metadata = json.loads(
            raw.decode("utf-8"),
            object_pairs_hook=_reject_duplicate_json_keys,
        )
    except (InputTooLargeError, InputChangedError, OSError) as exc:
        raise EvidenceMetadataError(str(exc)) from exc
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as exc:
        raise EvidenceMetadataError(f"invalid provenance JSON: {exc}") from exc

    if not isinstance(metadata, dict):
        raise EvidenceMetadataError("provenance JSON must be an object")

    missing = sorted(PROVENANCE_REQUIRED_FIELDS - metadata.keys())
    if missing:
        raise EvidenceMetadataError(
            "missing required provenance fields: " + ", ".join(missing)
        )

    for field in sorted(PROVENANCE_REQUIRED_FIELDS - {"applies_to"}):
        if not isinstance(metadata[field], str) or not metadata[field].strip():
            raise EvidenceMetadataError(
                f"provenance field {field} must be a non-empty string"
            )

    applies_to = metadata["applies_to"]
    if isinstance(applies_to, str):
        if not applies_to.strip():
            raise EvidenceMetadataError(
                "provenance field applies_to must not be empty"
            )
    elif (
        not isinstance(applies_to, list)
        or not applies_to
        or any(not isinstance(scope, str) or not scope.strip() for scope in applies_to)
    ):
        raise EvidenceMetadataError(
            "provenance field applies_to must be a non-empty string or list of strings"
        )

    if metadata["verification_status"] not in PROVENANCE_STATUSES:
        raise EvidenceMetadataError(
            "provenance field verification_status has an unknown value"
        )
    if metadata["artifact_class"] not in ARTIFACT_CLASSES:
        raise EvidenceMetadataError(
            "provenance field artifact_class has an unknown value"
        )

    checksum = metadata["checksum"]
    if checksum != "NOT_COMPUTED":
        match = re.fullmatch(r"sha256:([0-9a-fA-F]{64})", checksum)
        if match is None:
            raise EvidenceMetadataError(
                "provenance checksum must be sha256:<64 hex digits> or NOT_COMPUTED"
            )
        actual = hashlib.sha256(artifact).hexdigest()
        if match.group(1).lower() != actual:
            raise EvidenceMetadataError(
                "provenance SHA-256 does not match the input artifact"
            )

    return metadata


def main(argv=None) -> int:
    argument_parser = argparse.ArgumentParser(
        description="Parse an IP Discovery binary file without accessing hardware."
    )
    argument_parser.add_argument("input_file", type=Path)
    argument_parser.add_argument(
        "--mode",
        choices=[mode.value for mode in ValidationMode],
        default=ValidationMode.STRICT.value,
        help="strict validation (default) or verified C++ compatibility behavior",
    )
    argument_parser.add_argument(
        "--json", action="store_true", help="write the parsed result as JSON"
    )
    argument_parser.add_argument(
        "--pretty", action="store_true", help="indent JSON output"
    )
    argument_parser.add_argument(
        "--provenance",
        type=Path,
        help="include a bounded JSON provenance sidecar in JSON output",
    )
    args = argument_parser.parse_args(argv)

    if args.provenance is not None and not args.json:
        print("--provenance requires --json", file=sys.stderr)
        return 2

    try:
        data = _read_input_file(args.input_file)
    except InputTooLargeError as exc:
        print(f"Error reading {args.input_file}: {exc}", file=sys.stderr)
        return 2
    except InputChangedError as exc:
        print(f"Error reading {args.input_file}: {exc}", file=sys.stderr)
        return 2
    except OSError as exc:
        print(f"Error reading {args.input_file}: {exc}", file=sys.stderr)
        return 2

    provenance = None
    if args.provenance is not None:
        try:
            provenance = _read_provenance_file(args.provenance, data)
        except EvidenceMetadataError as exc:
            print(f"Error reading provenance {args.provenance}: {exc}", file=sys.stderr)
            return 2

    parser = IPDiscovery(mode=args.mode)
    success, error = parser.init(data)
    if not success:
        print(f"Parse failed: {error}", file=sys.stderr)
        return 1

    if args.json:
        result = parser.to_dict()
        if provenance is not None:
            result = {
                "analysis": {
                    "input_size": len(data),
                    "computed_sha256": hashlib.sha256(data).hexdigest(),
                    "mode": args.mode,
                    "validation_scope": "STRUCTURE_ONLY",
                },
                "provenance": {
                    "metadata": provenance,
                    "metadata_authenticated": False,
                },
                "parse": result,
            }
        print(
            json.dumps(
                result,
                indent=2 if args.pretty else None,
                sort_keys=provenance is not None,
            )
        )
        return 0

    print("IP Discovery Binary Analysis")
    print(f"File: {args.input_file}")
    print(f"Mode: {args.mode}")
    print(f"Binary offset: 0x{parser.binary_offset():x}")
    print(f"Die offset: 0x{parser.die_base():x}")
    print(f"IP count: {parser.ip_count()}")
    print()
    print("IP Entries:")
    for index in range(parser.ip_count()):
        entry = parser.ip_at(index)
        if entry is None:
            print(f"  [{index}] unavailable (record failed compatibility bounds checks)")
            continue
        try:
            hw_name = HwId(entry.hw_id).name
        except ValueError:
            hw_name = f"UNKNOWN_{entry.hw_id}"
        print(
            f"  [{index}] {hw_name} (hw_id={entry.hw_id}, "
            f"instance={entry.instance})"
        )
        print(f"      Version: {entry.version}")
        print(f"      Bases: {entry.num_bases}")
        for base_index, base in enumerate(entry.bases):
            print(f"        [{base_index}] 0x{base:08x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
