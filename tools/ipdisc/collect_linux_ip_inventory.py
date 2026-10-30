#!/usr/bin/env python3
"""Capture Linux AMDGPU's decoded IP Discovery sysfs tree for an RX 7600.

This tool reads sysfs attributes only. It does not access PCI config space,
BARs, MMIO, VRAM, firmware, or GPU registers, and it does not load/unload
drivers or change system settings. The capture is a decoded sysfs snapshot,
not the original binary IP Discovery artifact.
"""
import argparse
import datetime
import hashlib
import json
import platform
import re
import sys
from pathlib import Path

TARGET_VENDOR = "0x1002"
TARGET_DEVICE = "0x7480"
BDF_PATTERN = re.compile(r"^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]$")
PCI_ATTRIBUTES = (
    "vendor",
    "device",
    "subsystem_vendor",
    "subsystem_device",
    "revision",
    "class",
)
MAX_FILES = 4096
MAX_FILE_BYTES = 65536
MAX_TOTAL_BYTES = 8 * 1024 * 1024


class CaptureError(Exception):
    """A safe, expected capture failure."""


def _read_attribute(path):
    try:
        return path.read_text(encoding="ascii").strip()
    except (OSError, UnicodeError) as exc:
        raise CaptureError("cannot read sysfs attribute {}: {}".format(path, exc))


def _read_bounded(path):
    data = bytearray()
    try:
        with path.open("rb") as stream:
            while True:
                chunk = stream.read(min(4096, MAX_FILE_BYTES + 1 - len(data)))
                if not chunk:
                    break
                data.extend(chunk)
                if len(data) > MAX_FILE_BYTES:
                    raise CaptureError(
                        "sysfs attribute exceeds {} bytes: {}".format(MAX_FILE_BYTES, path)
                    )
    except OSError as exc:
        raise CaptureError("cannot read IP Discovery attribute {}: {}".format(path, exc))
    return bytes(data)


def _matching_devices(sysfs_root):
    matches = []
    try:
        entries = sorted(sysfs_root.iterdir(), key=lambda item: item.name)
    except OSError as exc:
        raise CaptureError("cannot list PCI sysfs directory {}: {}".format(sysfs_root, exc))

    for entry in entries:
        if not BDF_PATTERN.fullmatch(entry.name):
            continue
        try:
            if _read_attribute(entry / "vendor").lower() != TARGET_VENDOR:
                continue
            if _read_attribute(entry / "device").lower() != TARGET_DEVICE:
                continue
        except CaptureError:
            continue
        matches.append(entry)
    return matches


def _choose_device(sysfs_root, bdf):
    matches = _matching_devices(sysfs_root)
    if bdf:
        if not BDF_PATTERN.fullmatch(bdf):
            raise CaptureError("invalid PCI BDF: {}".format(bdf))
        for device in matches:
            if device.name.lower() == bdf.lower():
                return device
        raise CaptureError(
            "no AMD 1002:7480 endpoint found at BDF {}".format(bdf)
        )

    if not matches:
        raise CaptureError(
            "no AMD 1002:7480 endpoint found under {}".format(sysfs_root)
        )
    if len(matches) != 1:
        names = ", ".join(item.name for item in matches)
        raise CaptureError(
            "found multiple AMD 1002:7480 endpoints ({}); select one with --bdf".format(names)
        )
    return matches[0]


def _output_is_outside_sysfs(sysfs_root, output_dir):
    root = sysfs_root.resolve()
    target = output_dir.absolute()
    resolved_parent = target.parent.resolve()
    target = resolved_parent / target.name
    try:
        target.relative_to(root)
    except ValueError:
        return
    raise CaptureError("output directory must be outside the sysfs tree")


def _read_ip_tree(ip_root):
    files = []
    total_bytes = 0
    try:
        entries = sorted(ip_root.rglob("*"))
    except OSError as exc:
        raise CaptureError("cannot enumerate IP Discovery sysfs tree: {}".format(exc))

    for source in entries:
        if source.is_symlink() or not source.is_file():
            continue
        relative = source.relative_to(ip_root).as_posix()
        data = _read_bounded(source)
        total_bytes += len(data)
        if total_bytes > MAX_TOTAL_BYTES:
            raise CaptureError(
                "IP Discovery sysfs snapshot exceeds {} bytes".format(MAX_TOTAL_BYTES)
            )
        files.append((relative, data))
        if len(files) > MAX_FILES:
            raise CaptureError("IP Discovery sysfs tree exceeds {} files".format(MAX_FILES))

    if not files:
        raise CaptureError("IP Discovery sysfs tree contains no readable attribute files")
    return files


def _sha256(data):
    return hashlib.sha256(data).hexdigest()


def create_snapshot(sysfs_root, output_dir, bdf=None, kernel_release=None):
    """Capture one endpoint into a new output directory; never overwrite."""
    sysfs_root = Path(sysfs_root)
    output_dir = Path(output_dir).expanduser()

    if not sysfs_root.is_dir():
        if platform.system() != "Linux":
            raise CaptureError(
                "this collector is Linux-only; macOS does not provide Linux AMDGPU "
                "sysfs at {}. Boot Linux with the RX 7600 bound to amdgpu, then "
                "run this script there. The macOS diagnostic only records PCI "
                "identity and Metal visibility; it cannot collect IP Discovery.".format(
                    sysfs_root
                )
            )
        raise CaptureError("PCI sysfs directory does not exist: {}".format(sysfs_root))
    _output_is_outside_sysfs(sysfs_root, output_dir)
    if output_dir.exists():
        raise CaptureError("output directory already exists; refusing to overwrite it")

    device = _choose_device(sysfs_root, bdf)
    pci = {}
    for name in PCI_ATTRIBUTES:
        pci[name] = _read_attribute(device / name)

    ip_root = device / "ip_discovery"
    if not ip_root.is_dir():
        raise CaptureError(
            "amdgpu IP Discovery sysfs tree is not exposed at {}; "
            "no data was captured".format(ip_root)
        )
    files = _read_ip_tree(ip_root)
    captured_at = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
    captured_at_text = captured_at.isoformat().replace("+00:00", "Z")
    driver_path = device / "driver"
    driver = driver_path.resolve().name if driver_path.exists() else "UNBOUND"

    metadata = {
        "artifact_class": "AMDGPU_SYSFS_IP_DISCOVERY_SNAPSHOT",
        "captured_at_utc": captured_at_text,
        "capture_tool": "collect_linux_ip_inventory.py",
        "kernel_release": kernel_release or platform.release(),
        "pci_bdf": device.name,
        "pci_attributes": pci,
        "driver": driver,
        "ip_discovery_source": str(ip_root),
        "ip_discovery_representation": "decoded_sysfs_attributes_not_raw_binary",
        "raw_ip_discovery_binary_included": False,
        "board_model": "NOT_IDENTIFIED",
        "files_captured": len(files),
        "bytes_captured": sum(len(data) for _, data in files),
        "limitations": [
            "This snapshot records values exposed by the Linux AMDGPU driver.",
            "It is not the original binary IP Discovery artifact.",
            "PCI identity and a sysfs snapshot do not establish macOS driver support.",
            "No BAR, MMIO, VRAM, firmware, or GPU register access is performed by this tool.",
        ],
    }

    # All source attributes are read and bounded before creating any output.
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(mode=0o700, exist_ok=False)
    records = []
    for relative, data in files:
        destination = output_dir / "ip_discovery" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        records.append(
            {
                "path": "ip_discovery/" + relative,
                "size_bytes": len(data),
                "sha256": _sha256(data),
            }
        )

    manifest = {
        "artifact_class": metadata["artifact_class"],
        "captured_at_utc": captured_at_text,
        "pci_bdf": device.name,
        "files": records,
    }
    (output_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    sums = [
        "{}  {}".format(record["sha256"], record["path"])
        for record in records
    ]
    (output_dir / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="ascii")
    return metadata


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sysfs-root",
        default="/sys/bus/pci/devices",
        help="PCI device sysfs directory (default: /sys/bus/pci/devices)",
    )
    parser.add_argument(
        "--bdf",
        help="select one matching PCI function when multiple 1002:7480 devices exist",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="new output directory; existing directories are never overwritten",
    )
    args = parser.parse_args(argv)

    try:
        metadata = create_snapshot(args.sysfs_root, args.output, args.bdf)
    except (CaptureError, OSError) as exc:
        parser.error(str(exc))
    print("Snapshot created: {}".format(Path(args.output).expanduser().absolute()))
    print("PCI endpoint: {} ({}/{})".format(
        metadata["pci_bdf"],
        metadata["pci_attributes"]["vendor"],
        metadata["pci_attributes"]["device"],
    ))
    print("Captured {} decoded sysfs files ({} bytes).".format(
        metadata["files_captured"], metadata["bytes_captured"]
    ))
    print("Raw IP Discovery binary included: no")
    return 0


if __name__ == "__main__":
    sys.exit(main())
