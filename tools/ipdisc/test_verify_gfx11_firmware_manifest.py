#!/usr/bin/env python3
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from verify_gfx11_firmware_manifest import VerificationError, verify_manifest


class FirmwareManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "evidence").mkdir()
        (self.root / "firmware").mkdir()
        self._write("evidence/vbios.rom", b"SYNTHETIC VBIOS TEST DATA")
        self._write("evidence/ipd.bin", b"SYNTHETIC IP DISCOVERY TEST DATA")
        self.manifest = {
            "target": {
                "vendor_id": "1002",
                "device_id": "7480",
                "subsystem_vendor_id": "148c",
                "subsystem_id": "2421",
            },
            "evidence_level": "board_attributed",
            "vbios": {
                "file": "evidence/vbios.rom",
                "identity_matched": True,
                "sha256": self._hash(b"SYNTHETIC VBIOS TEST DATA"),
            },
            "ip_discovery": {
                "artifact_class": "physical_capture",
                "acquisition_method": "synthetic test fixture only",
                "file": "evidence/ipd.bin",
                "sha256": self._hash(b"SYNTHETIC IP DISCOVERY TEST DATA"),
            },
            "ip_versions": {
                "psp": {"major": 13, "minor": 0, "revision": 4},
                "smu": {"major": 13, "minor": 0, "revision": 4},
                "gc": {"major": 11, "minor": 0, "revision": 0},
                "sdma": {"major": 6, "minor": 0, "revision": 0},
            },
            "firmware": [],
        }
        for role, filename in (
            ("psp_sos", "psp_13_0_4_sos.bin"),
            ("smu", "smu_13_0_4.bin"),
            ("gc_pfp", "gc_11_0_0_pfp.bin"),
            ("gc_me", "gc_11_0_0_me.bin"),
            ("gc_mec", "gc_11_0_0_mec.bin"),
            ("gc_rlc", "gc_11_0_0_rlc.bin"),
            ("sdma", "sdma_6_0_0.bin"),
        ):
            data = ("SYNTHETIC:" + role).encode()
            self._write("firmware/" + filename, data)
            self.manifest["firmware"].append({
                "role": role,
                "file": "firmware/" + filename,
                "byte_length": len(data),
                "sha256": self._hash(data),
                "source_revision": "synthetic-test-revision",
                "license_evidence": "synthetic test only; not a real approval",
                "board_attributed": True,
                "license_approved": True,
            })
        self.manifest_path = self.root / "manifest.json"
        self._save()

    def tearDown(self):
        self.temp.cleanup()

    @staticmethod
    def _hash(data):
        return hashlib.sha256(data).hexdigest()

    def _write(self, relative, data):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def _save(self):
        self.manifest_path.write_text(json.dumps(self.manifest), encoding="utf-8")

    def test_synthetic_manifest_hashes_validate(self):
        result = verify_manifest(self.manifest_path)
        self.assertEqual(result["status"], "STRUCTURE_AND_HASHES_VERIFIED_ONLY")
        self.assertEqual(len(result["firmware_records"]), 7)

    def test_wrong_pci_identity_rejected(self):
        self.manifest["target"]["device_id"] = "7550"
        self._save()
        with self.assertRaisesRegex(VerificationError, "target.device_id"):
            verify_manifest(self.manifest_path)

    def test_firmware_hash_mismatch_rejected(self):
        self.manifest["firmware"][0]["sha256"] = "0" * 64
        self._save()
        with self.assertRaisesRegex(VerificationError, "SHA-256 mismatch"):
            verify_manifest(self.manifest_path)

    def test_firmware_size_mismatch_rejected(self):
        self.manifest["firmware"][0]["byte_length"] += 1
        self._save()
        with self.assertRaisesRegex(VerificationError, "byte_length"):
            verify_manifest(self.manifest_path)

    def test_missing_required_role_rejected(self):
        self.manifest["firmware"] = [
            item for item in self.manifest["firmware"] if item["role"] != "gc_mec"
        ]
        self._save()
        with self.assertRaisesRegex(VerificationError, "missing required"):
            verify_manifest(self.manifest_path)

    def test_firmware_prefix_mismatch_rejected(self):
        item = self.manifest["firmware"][0]
        old = self.root / item["file"]
        new = old.with_name("gc_11_0_0_sos.bin")
        old.rename(new)
        item["file"] = "firmware/" + new.name
        item["sha256"] = self._hash(new.read_bytes())
        self._save()
        with self.assertRaisesRegex(VerificationError, "filename"):
            verify_manifest(self.manifest_path)

    def test_path_escape_rejected(self):
        self.manifest["firmware"][0]["file"] = "../outside.bin"
        self._save()
        with self.assertRaisesRegex(VerificationError, "unsafe or missing"):
            verify_manifest(self.manifest_path)

    def test_symlink_rejected(self):
        source = self.root / "firmware/psp_13_0_4_sos.bin"
        link = self.root / "firmware/psp-link.bin"
        link.symlink_to(source)
        self.manifest["firmware"][0]["file"] = "firmware/psp-link.bin"
        self._save()
        with self.assertRaisesRegex(VerificationError, "non-symlink"):
            verify_manifest(self.manifest_path)

    def test_unreviewed_license_rejected(self):
        self.manifest["firmware"][0]["license_approved"] = False
        self._save()
        with self.assertRaisesRegex(VerificationError, "licence approval"):
            verify_manifest(self.manifest_path)


if __name__ == "__main__":
    unittest.main()
