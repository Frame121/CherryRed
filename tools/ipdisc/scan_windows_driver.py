#!/usr/bin/env python3
"""Read-only static scanner for IP Discovery tables embedded in local files.

This tool does not access PCI configuration space, GPU registers, MMIO, VRAM,
firmware interfaces, or the GPU itself. A hit in a driver binary is not
board-specific hardware evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import mmap
import os
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from ipdiscovery import BINARY_SIGNATURE, IPDiscovery

IPDS_SIGNATURE = b"IPDS"
BINARY_SIGNATURE_BYTES = struct.pack("<I", BINARY_SIGNATURE)
MAX_BINARY_SIZE = 0xFFFF
DEFAULT_MAX_FILE_SIZE = 1024 * 1024 * 1024


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def scan_file(path: str | os.PathLike[str], max_file_size: int = DEFAULT_MAX_FILE_SIZE) -> dict[str, Any]:
    source = Path(path)
    if source.is_symlink() or not source.is_file():
        raise ValueError("input must be a regular, non-symlink file")
    initial = source.stat()
    if initial.st_size < 512:
        raise ValueError("input file must be at least 512 bytes")
    if initial.st_size > max_file_size:
        raise ValueError(f"input exceeds size limit ({max_file_size} bytes)")

    digest = sha256_file(source)
    hits: list[int] = []
    candidates: dict[int, dict[str, Any]] = {}
    with source.open("rb") as stream:
        with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
            position = 0
            while True:
                hit = data.find(IPDS_SIGNATURE, position)
                if hit < 0:
                    break
                position = hit + 1
                if hit % 4:
                    continue
                hits.append(hit)

                low = max(0, hit - MAX_BINARY_SIZE)
                low = (low + 3) & ~3
                candidate_position = low
                while candidate_position < hit:
                    binary_start = data.find(BINARY_SIGNATURE_BYTES, candidate_position, hit)
                    if binary_start < 0:
                        break
                    candidate_position = binary_start + 1
                    if binary_start % 4 or binary_start in candidates:
                        continue
                    window = bytes(data[binary_start:min(binary_start + MAX_BINARY_SIZE, len(data))])
                    parser = IPDiscovery()
                    valid, error = parser.init(window)
                    if not valid or parser.binary_offset() != 0:
                        continue
                    major = struct.unpack_from("<H", window, 4)[0]
                    table_list = 16 if major >= 2 else 12
                    ipd_relative = struct.unpack_from("<H", window, table_list)[0]
                    if binary_start + ipd_relative != hit:
                        continue
                    parsed = parser.to_dict()
                    candidates[binary_start] = {
                        "binary_offset": binary_start,
                        "binary_size": parser.binary_size,
                        "ipds_offset": hit,
                        "ip_count": parser.ip_count(),
                        "ips": parsed.get("ips", []),
                    }

    final = source.stat()
    if (initial.st_size, initial.st_mtime_ns, initial.st_ctime_ns) != (
        final.st_size, final.st_mtime_ns, final.st_ctime_ns
    ):
        raise ValueError("input file changed during scan")
    final_digest = sha256_file(source)
    if final_digest != digest:
        raise ValueError("input file content hash changed during scan")

    return {
        "collected_at": datetime.now(timezone.utc).isoformat(),
        "source_path": str(source.resolve()),
        "source_size": initial.st_size,
        "source_sha256": digest,
        "scan_scope": "static file bytes only; no GPU or device access",
        "aligned_ipds_signature_offsets": hits,
        "valid_strict_binary_candidates": [candidates[key] for key in sorted(candidates)],
        "interpretation": (
            "A valid embedded structure is only a parser-validated byte sequence. "
            "It does not authenticate the file, prove the sequence came from a physical GPU, "
            "or establish board-specific IP versions."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_file", help="local file to scan; it is read only")
    parser.add_argument("--json", dest="json_path", help="write JSON report to this path")
    parser.add_argument("--max-file-size", type=int, default=DEFAULT_MAX_FILE_SIZE)
    args = parser.parse_args(argv)
    try:
        if args.json_path and Path(args.json_path).resolve() == Path(args.input_file).resolve():
            raise ValueError("JSON report path must not overwrite the input file")
        report = scan_file(args.input_file, args.max_file_size)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(report, indent=2)
    if args.json_path:
        Path(args.json_path).write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
