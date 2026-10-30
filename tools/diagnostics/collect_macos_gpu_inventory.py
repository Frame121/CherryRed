#!/usr/bin/env python3
"""Collect a read-only, structured macOS GPU visibility snapshot for CherryRed."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


RX7600_VENDOR_ID = 0x1002
RX7600_DEVICE_ID = 0x7480
METAL_PROBE = Path(__file__).with_name("macos_metal_probe.swift")


def run_command(argv: list[str], timeout: int = 60) -> dict[str, Any]:
    try:
        result = subprocess.run(
            argv, capture_output=True, text=True, timeout=timeout, check=False
        )
    except (FileNotFoundError, subprocess.TimeoutExpired) as exc:
        return {
            "status": "unavailable",
            "returncode": None,
            "stdout": "",
            "stderr": str(exc),
        }
    return {
        "status": "ok" if result.returncode == 0 else "failed",
        "returncode": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }


def _little_endian_hex(value: str) -> int | None:
    try:
        raw = bytes.fromhex(value)
    except ValueError:
        return None
    return int.from_bytes(raw, byteorder="little") if raw else None


def parse_ioreg_properties(text: str) -> dict[str, int]:
    wanted = {
        "vendor-id",
        "device-id",
        "revision-id",
        "subsystem-vendor-id",
        "subsystem-id",
        "class-code",
    }
    found: dict[str, int] = {}
    for key, raw in re.findall(r'"([^"]+)"\s*=\s*<([0-9a-fA-F]+)>', text):
        if key in wanted:
            value = _little_endian_hex(raw)
            if value is not None:
                found[key] = value
    return found


def parse_system_profiler(payload: dict[str, Any]) -> list[dict[str, Any]]:
    safe_keys = (
        "spdisplays_device-id",
        "spdisplays_revision-id",
        "spdisplays_pcie_width",
        "spdisplays_vram",
        "spdisplays_vendor",
        "sppci_bus",
        "sppci_kextinfo",
    )
    rows = payload.get("SPDisplaysDataType", [])
    if not isinstance(rows, list):
        return []
    return [
        {key: row[key] for key in safe_keys if key in row}
        for row in rows
        if isinstance(row, dict) and row.get("sppci_device_type") == "spdisplays_gpu"
    ]


def parse_ioaccelerator_count(text: str) -> int:
    return len(re.findall(r"(?m)^.*<class IOAccelerator,", text))


def parse_loaded_amd_extensions(text: str) -> list[dict[str, str]]:
    result: list[dict[str, str]] = []
    pattern = re.compile(r"\b([A-Za-z0-9_.-]+)\s+\(([^)]+)\)")
    for line in text.splitlines():
        if not re.search(r"AMD|Radeon|Lilu|WhateverGreen|Noot", line, re.IGNORECASE):
            continue
        match = pattern.search(line)
        if match:
            item = {"bundle_id": match.group(1), "version": match.group(2)}
            if item not in result:
                result.append(item)
    return result


def parse_metal_probe(text: str) -> dict[str, Any]:
    count_match = re.search(r"^Metal devices visible:\s*(\d+)\s*$", text, re.MULTILINE)
    if not count_match:
        return {"status": "unavailable", "device_count": None, "devices": [], "default_device": None}

    devices: list[dict[str, Any]] = []
    blocks = re.finditer(
        r"(?ms)^\[Device (\d+)\]\s*\n(.*?)(?=^\[Device \d+\]\s*$|\Z)", text
    )
    for block in blocks:
        body = block.group(2)
        name = re.search(r"^Name:\s*(.+)$", body, re.MULTILINE)
        registry_id = re.search(r"^Registry ID:\s*(.+)$", body, re.MULTILINE)
        devices.append(
            {
                "index": int(block.group(1)),
                "name": name.group(1).strip() if name else None,
                "registry_id": registry_id.group(1).strip() if registry_id else None,
            }
        )

    default_match = re.search(r"^Default Metal device:\s*(.+)$", text, re.MULTILINE)
    default_device = default_match.group(1).strip() if default_match else None
    if default_device == "none":
        default_device = None
    return {
        "status": "ok",
        "device_count": int(count_match.group(1)),
        "devices": devices,
        "default_device": default_device,
    }


def assess_current_boot(
    pci: dict[str, int],
    profiler_gpus: list[dict[str, Any]],
    accelerator_count: int,
    metal: dict[str, Any],
) -> dict[str, Any]:
    rx7600_seen = (
        pci.get("vendor-id") == RX7600_VENDOR_ID
        and pci.get("device-id") == RX7600_DEVICE_ID
    )
    if not rx7600_seen:
        rx7600_seen = any(
            row.get("spdisplays_device-id", "").lower() == "0x7480"
            and "amd" in row.get("spdisplays_vendor", "").lower()
            for row in profiler_gpus
        )

    metal_count = metal.get("device_count")
    if not rx7600_seen:
        status = "rx7600_not_detected"
    elif accelerator_count == 0 or metal_count == 0:
        status = "rx7600_not_accelerated_on_current_boot"
    else:
        status = "metal_visible_but_rx7600_attribution_unverified"

    return {
        "rx7600_pci_detected": rx7600_seen,
        "ioaccelerator_service_count": accelerator_count,
        "metal_device_count": metal_count,
        "status": status,
        "hardware_validation_required": True,
    }


def collect_snapshot() -> dict[str, Any]:
    if platform.system() != "Darwin":
        raise RuntimeError("This collector is macOS-only.")

    product_version = run_command(["sw_vers", "-productVersion"])
    build_version = run_command(["sw_vers", "-buildVersion"])
    profiler_result = run_command(["system_profiler", "-json", "SPDisplaysDataType"])
    ioreg_result = run_command(["ioreg", "-r", "-n", "GFX0", "-l", "-w0"])
    accelerator_result = run_command(["ioreg", "-r", "-c", "IOAccelerator", "-l", "-w0"])
    loaded_result = run_command(["kmutil", "showloaded"])

    profiler_payload: dict[str, Any] = {}
    if profiler_result["status"] == "ok":
        try:
            decoded = json.loads(profiler_result["stdout"])
            if isinstance(decoded, dict):
                profiler_payload = decoded
        except json.JSONDecodeError:
            pass

    pci = parse_ioreg_properties(ioreg_result["stdout"])
    profiler_gpus = parse_system_profiler(profiler_payload)
    accelerator_count = parse_ioaccelerator_count(accelerator_result["stdout"])
    loaded_extensions = parse_loaded_amd_extensions(loaded_result["stdout"])

    swift_path = shutil.which("swift")
    if swift_path:
        metal_result = run_command([swift_path, str(METAL_PROBE)], timeout=90)
        metal = parse_metal_probe(metal_result["stdout"])
        if metal_result["status"] != "ok" and metal["status"] == "ok":
            metal["status"] = "failed"
    else:
        metal = {"status": "unavailable", "device_count": None, "devices": [], "default_device": None}

    identity = {
        "vendor_id": pci.get("vendor-id"),
        "device_id": pci.get("device-id"),
        "revision_id": pci.get("revision-id"),
        "subsystem_vendor_id": pci.get("subsystem-vendor-id"),
        "subsystem_id": pci.get("subsystem-id"),
        "class_code": pci.get("class-code"),
    }
    for key, value in list(identity.items()):
        if value is not None:
            width = 2 if key == "revision_id" else 4
            identity[key] = f"0x{value:0{width}x}"

    report = {
        "schema_version": 1,
        "collected_at_utc": datetime.now(timezone.utc).isoformat(),
        "host": {
            "product_name": "macOS",
            "product_version": product_version["stdout"].strip() or None,
            "build_version": build_version["stdout"].strip() or None,
            "architecture": platform.machine(),
        },
        "gpu": {
            "ioreg_gfx0_identity": identity,
            "system_profiler": profiler_gpus,
            "loaded_related_extensions": loaded_extensions,
            "ioaccelerator_service_count": accelerator_count,
            "metal": metal,
        },
        "current_boot_assessment": assess_current_boot(
            pci, profiler_gpus, accelerator_count, metal
        ),
        "ip_discovery": {
            "status": "not_collected",
            "reason": "This macOS collector does not expose Linux AMDGPU sysfs IP Discovery data.",
        },
        "safety": {
            "read_only": True,
            "uses_public_metal_enumeration": True,
            "maps_gpu_bars": False,
            "reads_gpu_registers": False,
            "loads_or_unloads_kernel_extensions": False,
            "changes_efi_or_system_security": False,
        },
        "source_status": {
            "system_profiler": profiler_result["status"],
            "ioreg_gfx0": ioreg_result["status"],
            "ioreg_ioaccelerator": accelerator_result["status"],
            "kmutil": loaded_result["status"],
        },
    }
    return report


def write_new_file(path: Path, contents: str) -> None:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    try:
        descriptor = os.open(path, flags, 0o644)
    except FileExistsError as exc:
        raise RuntimeError(f"Refusing to overwrite existing output: {path}") from exc
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(contents)
    except Exception:
        try:
            path.unlink()
        except OSError:
            pass
        raise


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Collect read-only macOS PCI, IOAccelerator, and Metal visibility evidence."
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="new JSON file to create; omitted means print JSON to stdout",
    )
    args = parser.parse_args(argv)

    try:
        report = collect_snapshot()
    except RuntimeError as exc:
        parser.error(str(exc))

    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        try:
            write_new_file(args.output, rendered)
        except (OSError, RuntimeError) as exc:
            parser.error(str(exc))
        print(f"macOS GPU inventory written to: {args.output}")
        print(f"Current boot assessment: {report['current_boot_assessment']['status']}")
    else:
        sys.stdout.write(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
