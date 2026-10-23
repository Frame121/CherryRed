#!/usr/bin/env python3
"""Tests for the read-only Windows driver-binary scanner."""
import contextlib
import importlib.util
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from mkfixture import FixtureGenerator

spec = importlib.util.spec_from_file_location("scan_windows_driver", HERE / "scan_windows_driver.py")
scanner = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(scanner)


class TestWindowsDriverScan(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def test_finds_valid_fixture_embedded_at_aligned_offset(self):
        fixture = FixtureGenerator().generate_valid_minimal()
        source = self.root / "embedded.bin"
        source.write_bytes(b"\x00" * 16 + fixture + b"\x00" * 16)
        report = scanner.scan_file(source)
        self.assertEqual(report["source_size"], len(source.read_bytes()))
        self.assertEqual(report["aligned_ipds_signature_offsets"], [16 + 60])
        self.assertEqual(len(report["valid_strict_binary_candidates"]), 1)
        candidate = report["valid_strict_binary_candidates"][0]
        self.assertEqual(candidate["binary_offset"], 16)
        self.assertEqual(candidate["ip_count"], 1)

    def test_ipds_string_without_valid_binary_is_not_accepted(self):
        source = self.root / "false-positive.bin"
        data = bytearray(1024)
        data[512:516] = b"IPDS"
        source.write_bytes(data)
        report = scanner.scan_file(source)
        self.assertEqual(report["aligned_ipds_signature_offsets"], [512])
        self.assertEqual(report["valid_strict_binary_candidates"], [])

    def test_rejects_too_small_input(self):
        source = self.root / "small.bin"
        source.write_bytes(b"\x00" * 511)
        with self.assertRaisesRegex(ValueError, "at least 512"):
            scanner.scan_file(source)

    def test_rejects_input_above_limit(self):
        source = self.root / "large.bin"
        source.write_bytes(b"\x00" * 512)
        with self.assertRaisesRegex(ValueError, "size limit"):
            scanner.scan_file(source, max_file_size=511)

    def test_cli_refuses_to_overwrite_input_with_json_report(self):
        source = self.root / "source.bin"
        source.write_bytes(FixtureGenerator().generate_valid_minimal())
        original = source.read_bytes()
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            code = scanner.main([str(source), "--json", str(source)])
        self.assertEqual(code, 2)
        self.assertIn("must not overwrite", stderr.getvalue())
        self.assertEqual(source.read_bytes(), original)

    def test_rejects_content_hash_change_during_scan(self):
        source = self.root / "changing.bin"
        source.write_bytes(b"\\x00" * 1024)
        with patch.object(scanner, "sha256_file", side_effect=["a" * 64, "b" * 64]):
            with self.assertRaisesRegex(ValueError, "content hash changed"):
                scanner.scan_file(source)

    def test_rejects_symlink(self):
        source = self.root / "source.bin"
        source.write_bytes(b"\x00" * 512)
        link = self.root / "link.bin"
        try:
            link.symlink_to(source)
        except (OSError, NotImplementedError):
            self.skipTest("symlink creation is unavailable")
        with self.assertRaisesRegex(ValueError, "non-symlink"):
            scanner.scan_file(link)


if __name__ == "__main__":
    unittest.main()
