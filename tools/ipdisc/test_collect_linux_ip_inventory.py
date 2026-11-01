#!/usr/bin/env python3
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import collect_linux_ip_inventory as collector


class LinuxIPInventoryCaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "pci"
        self.root.mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def make_device(
        self,
        bdf="0000:01:00.0",
        vendor="0x1002",
        device="0x7480",
        include_ip_discovery=True,
    ):
        endpoint = self.root / bdf
        endpoint.mkdir()
        attributes = {
            "vendor": vendor,
            "device": device,
            "subsystem_vendor": "0x148c",
            "subsystem_device": "0x2421",
            "revision": "0xcf",
            "class": "0x030000",
        }
        for name, value in attributes.items():
            (endpoint / name).write_text(value + "\n", encoding="ascii")
        if include_ip_discovery:
            instance = endpoint / "ip_discovery" / "die" / "0" / "8" / "0"
            instance.mkdir(parents=True)
            (instance / "hw_id").write_text("8\n", encoding="ascii")
            (instance / "major").write_text("11\n", encoding="ascii")
            (instance / "minor").write_text("0\n", encoding="ascii")
            (instance / "revision").write_text("2\n", encoding="ascii")
            (instance / "harvest").write_text("0x0\n", encoding="ascii")
            (instance / "base_addr_0").write_text("0x1234\n", encoding="ascii")
        return endpoint

    def test_captures_inventory_and_hash_manifest(self):
        endpoint = self.make_device()
        output = Path(self.temp.name) / "snapshot"
        metadata = collector.create_snapshot(
            self.root, output, kernel_release="fixture-kernel"
        )

        self.assertEqual(metadata["pci_bdf"], endpoint.name)
        self.assertEqual(metadata["pci_attributes"]["subsystem_vendor"], "0x148c")
        self.assertEqual(metadata["pci_attributes"]["subsystem_device"], "0x2421")
        self.assertEqual(metadata["kernel_release"], "fixture-kernel")
        self.assertFalse(metadata["raw_ip_discovery_binary_included"])
        self.assertEqual(metadata["ip_discovery_representation"],
                         "decoded_sysfs_attributes_not_raw_binary")
        copied = output / "ip_discovery" / "die" / "0" / "8" / "0" / "major"
        self.assertEqual(copied.read_text(encoding="ascii"), "11\n")

        manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
        record = next(item for item in manifest["files"] if item["path"].endswith("/major"))
        self.assertEqual(
            record["sha256"],
            hashlib.sha256(copied.read_bytes()).hexdigest(),
        )
        self.assertTrue((output / "SHA256SUMS").is_file())

    def test_requires_unique_matching_gpu_unless_bdf_selected(self):
        self.make_device("0000:01:00.0")
        self.make_device("0000:02:00.0")
        output = Path(self.temp.name) / "ambiguous"
        with self.assertRaisesRegex(collector.CaptureError, "multiple"):
            collector.create_snapshot(self.root, output)
        self.assertFalse(output.exists())

        selected = Path(self.temp.name) / "selected"
        metadata = collector.create_snapshot(
            self.root, selected, bdf="0000:02:00.0"
        )
        self.assertEqual(metadata["pci_bdf"], "0000:02:00.0")

    def test_non_linux_missing_sysfs_explains_required_environment(self):
        missing_root = Path(self.temp.name) / "linux-sysfs-not-present"
        output = Path(self.temp.name) / "no-capture"
        with mock.patch.object(collector.platform, "system", return_value="Darwin"):
            with self.assertRaisesRegex(
                collector.CaptureError, "Linux-only.*Linux AMDGPU sysfs"
            ):
                collector.create_snapshot(missing_root, output)
        self.assertFalse(output.exists())

    def test_rejects_unmatched_device(self):
        self.make_device(device="0x7550")
        with self.assertRaisesRegex(collector.CaptureError, "no AMD 1002:7480"):
            collector.create_snapshot(self.root, Path(self.temp.name) / "empty")

    def test_requires_ip_discovery_tree(self):
        self.make_device(include_ip_discovery=False)
        output = Path(self.temp.name) / "missing-ipd"
        with self.assertRaisesRegex(collector.CaptureError, "not exposed"):
            collector.create_snapshot(self.root, output)
        self.assertFalse(output.exists())

    def test_never_overwrites_existing_output(self):
        self.make_device()
        output = Path(self.temp.name) / "existing"
        output.mkdir()
        sentinel = output / "keep.txt"
        sentinel.write_text("preserve\n", encoding="ascii")
        with self.assertRaisesRegex(collector.CaptureError, "refusing to overwrite"):
            collector.create_snapshot(self.root, output)
        self.assertEqual(sentinel.read_text(encoding="ascii"), "preserve\n")

    def test_refuses_output_inside_sysfs(self):
        self.make_device()
        output = self.root / "snapshot"
        with self.assertRaisesRegex(collector.CaptureError, "outside the sysfs"):
            collector.create_snapshot(self.root, output)
        self.assertFalse(output.exists())

    def test_bounds_attribute_size(self):
        endpoint = self.make_device()
        large = endpoint / "ip_discovery" / "die" / "0" / "8" / "0" / "large"
        large.write_bytes(b"x" * (collector.MAX_FILE_BYTES + 1))
        output = Path(self.temp.name) / "oversized"
        with self.assertRaisesRegex(collector.CaptureError, "exceeds"):
            collector.create_snapshot(self.root, output)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
