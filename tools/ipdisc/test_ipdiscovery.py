#!/usr/bin/env python3
"""Standard-library tests using synthetic IP Discovery data only."""

import json
import hashlib
import os
import random
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import dump
from ipdiscovery import HwId, IPDiscovery, IPVersion, ValidationMode
from mkfixture import FixtureGenerator


class TestIPDiscovery(unittest.TestCase):
    def setUp(self):
        self.generator = FixtureGenerator()
        self.fixtures = self.generator.generate_all()

    def parse(self, name, mode=ValidationMode.STRICT.value):
        parser = IPDiscovery(mode=mode)
        success, error = parser.init(self.fixtures[name])
        return parser, success, error

    def test_valid_minimal_structure_and_lookup(self):
        parser, success, error = self.parse("valid_minimal.bin")
        self.assertTrue(success, error)
        self.assertTrue(parser.is_valid())
        self.assertEqual(parser.ip_count(), 1)

        entry = parser.find_ip(HwId.GC, 0)
        self.assertIsNotNone(entry)
        self.assertEqual(entry.version, IPVersion(12, 0, 1))
        self.assertEqual(entry.bases, [0x40000])
        self.assertEqual(parser.reg_byte_offset(HwId.GC, 0, 0, 0x100), 0x40100 * 4)

    def test_register_offset_overflow_modes(self):
        strict = IPDiscovery()
        success, error = strict.init(self.fixtures["valid_minimal.bin"])
        self.assertTrue(success, error)
        self.assertIsNone(
            strict.reg_byte_offset(HwId.GC, 0, 0, 0xFFFFFFFF)
        )

        compatible = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = compatible.init(self.fixtures["valid_minimal.bin"])
        self.assertTrue(success, error)
        expected = ((0x40000 + 0xFFFFFFFF) * 4) & 0xFFFFFFFF
        self.assertEqual(
            compatible.reg_byte_offset(HwId.GC, 0, 0, 0xFFFFFFFF),
            expected,
        )

    def test_multiple_records_and_base_arrays(self):
        parser, success, error = self.parse("valid_multi_record.bin")
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_count(), 2)
        self.assertEqual(parser.ip_at(0).bases, [0x40000, 0x50000])
        self.assertEqual(parser.ip_at(1).bases, [0x60000, 0x61000, 0x62000])
        self.assertEqual(parser.find_ip(HwId.SDMA0, 1).version, IPVersion(6, 1, 0))

    def test_v2_binary_header_layout(self):
        binary = self.fixtures["valid_v2_header.bin"]
        self.assertEqual(struct.unpack_from("<H", binary, 4)[0], 2)
        table_count = struct.unpack_from("<H", binary, 12)[0]
        self.assertEqual(table_count, 1)
        table_offset = struct.unpack_from("<H", binary, 16)[0]
        table_size = struct.unpack_from("<H", binary, 20)[0]
        binary_size = struct.unpack_from("<H", binary, 10)[0]
        self.assertGreaterEqual(table_offset, 16 + table_count * 8)
        self.assertLessEqual(table_offset + table_size, binary_size)

        parser, success, error = self.parse("valid_v2_header.bin")
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_count(), 1)
        self.assertEqual(parser.binary_offset(), 0)
        self.assertGreaterEqual(parser.die_base(), table_offset + 80)

    def test_v1_binary_header_and_all_six_table_entries_do_not_overlap(self):
        binary = self.fixtures["valid_minimal.bin"]
        table_offset = struct.unpack_from("<H", binary, 12)[0]
        table_size = struct.unpack_from("<H", binary, 16)[0]
        binary_size = struct.unpack_from("<H", binary, 10)[0]
        self.assertEqual(struct.unpack_from("<H", binary, 4)[0], 1)
        self.assertGreaterEqual(table_offset, 12 + 6 * 8)
        self.assertLessEqual(table_offset + table_size, binary_size)

        parser, success, error = self.parse("valid_minimal.bin")
        self.assertTrue(success, error)
        self.assertEqual(parser.binary_offset(), 0)
        self.assertEqual(parser.ip_count(), 1)
        self.assertEqual(parser.ip_at(0).hw_id, HwId.GC)

    def test_strict_rejects_ip_discovery_table_overlapping_header_or_table_list(self):
        for name in (
            "table_overlaps_binary_header.bin",
            "table_overlaps_table_list.bin",
        ):
            with self.subTest(name=name):
                parser, success, error = self.parse(name)
                self.assertFalse(success)
                self.assertFalse(parser.is_valid())
                self.assertIn("overlaps binary header or table list", error.lower())

    def test_strict_rejects_die_header_overlapping_fixed_ipds_header(self):
        parser, success, error = self.parse("die_overlaps_ipds_header.bin")
        self.assertFalse(success)
        self.assertFalse(parser.is_valid())
        self.assertIn("die header overlaps fixed ip_discovery header", error.lower())

    def test_ip_discovery_v4_with_32bit_bases(self):
        parser, success, error = self.parse("valid_v4_table.bin")
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_at(0).bases, [0x40000])

    def test_nonzero_binary_offset_scan(self):
        binary = b"\x00" * 4 + self.fixtures["valid_minimal.bin"]
        parser = IPDiscovery()
        success, error = parser.init(binary)
        self.assertTrue(success, error)
        self.assertEqual(parser.binary_offset(), 4)

    def test_invalid_signatures_and_checksums(self):
        for name, expected in (
            ("invalid_signature.bin", "signature"),
            ("invalid_checksum.bin", "checksum"),
            ("invalid_table_checksum.bin", "table checksum"),
            ("invalid_ipds_signature.bin", "IP Discovery signature"),
        ):
            with self.subTest(name=name):
                parser, success, error = self.parse(name)
                self.assertFalse(success)
                self.assertIn(expected.lower(), error.lower())
                self.assertFalse(parser.is_valid())

    def test_invalid_offsets_and_counts(self):
        for name, expected in (
            ("invalid_offset.bin", "table"),
            ("zero_ip_count.bin", "IP count"),
            ("excessive_ip_count.bin", "IP count"),
            ("truncated_header.bin", "binary size"),
        ):
            with self.subTest(name=name):
                parser, success, error = self.parse(name)
                self.assertFalse(success)
                self.assertIn(expected.lower(), error.lower())

    def test_truncated_record_and_excessive_base_count(self):
        for name, expected in (
            ("truncated_record.bin", "truncated base array"),
            ("excessive_base_count.bin", "too many base addresses"),
        ):
            with self.subTest(name=name):
                parser, success, error = self.parse(name)
                self.assertFalse(success)
                self.assertIn(expected.lower(), error.lower())

    def test_unsupported_versions_and_multiple_dies(self):
        for name, expected in (
            ("unsupported_version.bin", "unsupported ip discovery version"),
            ("unsupported_binary_version.bin", "unsupported binary header version"),
            ("unsupported_64bit_bases.bin", "64-bit ip base addresses"),
            ("multiple_dies.bin", "die count"),
        ):
            with self.subTest(name=name):
                parser, success, error = self.parse(name)
                self.assertFalse(success)
                self.assertIn(expected.lower(), error.lower())

    def test_compatibility_mode_preserves_cpp_clamping_and_die_behavior(self):
        excessive_bases = bytearray(self.fixtures["excessive_base_count.bin"])
        die_offset = FixtureGenerator._die_offset(excessive_bases)
        excessive_bases[die_offset + 4 + 3] = 17
        FixtureGenerator._refresh_checksums(excessive_bases)

        parser = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = parser.init(bytes(excessive_bases))
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_at(0).num_bases, IPDiscovery.MAX_BASES)

        parser = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = parser.init(self.fixtures["multiple_dies.bin"])
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_count(), 1)

        parser = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = parser.init(self.fixtures["unsupported_version.bin"])
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_count(), 1)

        parser = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = parser.init(
            self.fixtures["unsupported_binary_version.bin"]
        )
        self.assertTrue(success, error)

        parser = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = parser.init(
            self.fixtures["table_overlaps_table_list.bin"]
        )
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_count(), 1)

        parser = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = parser.init(
            self.fixtures["die_overlaps_ipds_header.bin"]
        )
        self.assertTrue(success, error)

    def test_cobalt_cpp_compatibility_lazy_record_bounds(self):
        data = self.fixtures["compatibility_truncated_record.bin"]
        self.assertEqual(len(data), 512)
        parser = IPDiscovery(mode=ValidationMode.COMPATIBILITY.value)
        success, error = parser.init(data)
        self.assertTrue(success, error)
        self.assertEqual(parser.ip_count(), 1)
        self.assertIsNone(parser.ip_at(0))

        strict = IPDiscovery(mode=ValidationMode.STRICT.value)
        success, error = strict.init(data)
        self.assertFalse(success)
        self.assertIn("truncated base array", error.lower())

    def test_parse_failure_resets_previous_state(self):
        parser, success, error = self.parse("valid_minimal.bin")
        self.assertTrue(success, error)
        success, error = parser.init(b"")
        self.assertFalse(success)
        self.assertFalse(parser.is_valid())
        self.assertEqual(parser.ip_count(), 0)
        self.assertIsNone(parser.ip_at(0))

    def test_to_dict_shapes(self):
        parser, success, error = self.parse("valid_minimal.bin")
        self.assertTrue(success, error)
        result = parser.to_dict()
        self.assertTrue(result["valid"])
        self.assertEqual(result["mode"], "strict")
        self.assertEqual(result["ip_count"], 1)
        self.assertEqual(result["ips"][0]["hw_id_name"], "GC")

        invalid = IPDiscovery()
        self.assertEqual(invalid.to_dict(), {"valid": False})

    def test_fixture_generation_is_deterministic(self):
        first = self.generator.generate_all()
        second = FixtureGenerator().generate_all()
        self.assertEqual(first, second)
        self.assertEqual(
            self.generator.generate_manifest(first),
            FixtureGenerator().generate_manifest(second),
        )

    def test_parity_case_manifest_is_valid_json(self):
        path = Path(__file__).with_name("parity_cases.json")
        cases = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(cases["version"], "2.0")
        self.assertTrue(cases["cases"])

    def test_randomized_malformed_inputs(self):
        rng = random.Random(0x1D015C)
        for index in range(512):
            size = rng.randint(512, 2048)
            data = bytearray(rng.getrandbits(8) for _ in range(size))
            has_candidate_signature = index % 2 == 0
            if has_candidate_signature:
                max_offset = (size - 512) // 4
                offset = rng.randrange(max_offset + 1) * 4
                data[offset : offset + 4] = struct.pack("<I", 0x28211407)
            parser = IPDiscovery()
            success, error = parser.init(bytes(data))
            self.assertFalse(success, f"random malformed input {index} parsed")
            if has_candidate_signature:
                self.assertNotEqual(
                    error,
                    "Binary signature not found",
                    f"aligned candidate at input {index} was not validated",
                )

    def test_cli_valid_text_json_and_invalid_input(self):
        cli = Path(__file__).with_name("dump.py")
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            valid_path = Path(temp_dir) / "valid.bin"
            valid_path.write_bytes(self.fixtures["valid_minimal.bin"])
            invalid_path = Path(temp_dir) / "invalid.bin"
            invalid_path.write_bytes(self.fixtures["invalid_signature.bin"])

            text = subprocess.run(
                [sys.executable, "-B", str(cli), str(valid_path)],
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )
            self.assertEqual(text.returncode, 0, text.stderr)
            self.assertIn("IP count: 1", text.stdout)
            self.assertLessEqual(valid_path.stat().st_size, dump.MAX_INPUT_SIZE)

            json_result = subprocess.run(
                [sys.executable, "-B", str(cli), str(valid_path), "--json"],
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )
            self.assertEqual(json_result.returncode, 0, json_result.stderr)
            self.assertTrue(json.loads(json_result.stdout)["valid"])

            invalid = subprocess.run(
                [sys.executable, "-B", str(cli), str(invalid_path)],
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )
            self.assertEqual(invalid.returncode, 1)
            self.assertIn("Parse failed:", invalid.stderr)

    def test_cli_json_validates_provenance_and_binds_artifact_hash(self):
        cli = Path(__file__).with_name("dump.py")
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        artifact = self.fixtures["valid_minimal.bin"]
        expected_hash = hashlib.sha256(artifact).hexdigest()
        metadata = {
            "evidence_id": "TEST-IPD-001",
            "claim": "Synthetic fixture parses under strict structural validation.",
            "artifact_path": "analysis-copy.bin",
            "source_repository": "LOCAL_PROJECT",
            "source_revision": "UNVERIFIED",
            "source_file_or_section": "generated test fixture",
            "checksum": f"sha256:{expected_hash}",
            "applies_to": ["TEST_ENVIRONMENT:synthetic"],
            "verification_method": "python standard-library unittest",
            "verification_status": "UNVERIFIED",
            "limitations": "Synthetic data; no board attribution.",
            "artifact_class": "SYNTHETIC_FIXTURE",
        }

        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            artifact_path = Path(temp_dir) / "analysis-copy.bin"
            provenance_path = Path(temp_dir) / "provenance.json"
            artifact_path.write_bytes(artifact)
            provenance_path.write_text(json.dumps(metadata), encoding="utf-8")

            command = [
                sys.executable,
                "-B",
                str(cli),
                str(artifact_path),
                "--mode",
                "strict",
                "--json",
                "--provenance",
                str(provenance_path),
            ]
            first = subprocess.run(
                command,
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )
            second = subprocess.run(
                command,
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )

        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(first.stdout, second.stdout)
        report = json.loads(first.stdout)
        self.assertEqual(report["analysis"]["computed_sha256"], expected_hash)
        self.assertEqual(report["analysis"]["validation_scope"], "STRUCTURE_ONLY")
        self.assertEqual(report["provenance"]["metadata"], metadata)
        self.assertFalse(report["provenance"]["metadata_authenticated"])
        self.assertTrue(report["parse"]["valid"])

    def test_cli_rejects_invalid_provenance_and_hash_mismatch(self):
        cli = Path(__file__).with_name("dump.py")
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        artifact = self.fixtures["valid_minimal.bin"]
        metadata = {
            "evidence_id": "TEST-IPD-002",
            "claim": "Synthetic test record.",
            "artifact_path": "analysis-copy.bin",
            "source_repository": "LOCAL_PROJECT",
            "source_revision": "UNVERIFIED",
            "source_file_or_section": "generated test fixture",
            "checksum": "sha256:" + "0" * 64,
            "applies_to": "TEST_ENVIRONMENT:synthetic",
            "verification_method": "unit test",
            "verification_status": "UNVERIFIED",
            "limitations": "Synthetic data only.",
            "artifact_class": "SYNTHETIC_FIXTURE",
        }

        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            artifact_path = Path(temp_dir) / "analysis-copy.bin"
            provenance_path = Path(temp_dir) / "provenance.json"
            artifact_path.write_bytes(artifact)

            cases = (
                (
                    "missing_field",
                    {key: value for key, value in metadata.items() if key != "claim"},
                    "missing required provenance fields",
                ),
                (
                    "unknown_status",
                    {**metadata, "verification_status": "AUTHENTICATED"},
                    "unknown value",
                ),
                (
                    "hash_mismatch",
                    metadata,
                    "does not match the input artifact",
                ),
            )
            for name, candidate, expected_error in cases:
                with self.subTest(name=name):
                    provenance_path.write_text(
                        json.dumps(candidate), encoding="utf-8"
                    )
                    result = subprocess.run(
                        [
                            sys.executable,
                            "-B",
                            str(cli),
                            str(artifact_path),
                            "--json",
                            "--provenance",
                            str(provenance_path),
                        ],
                        capture_output=True,
                        text=True,
                        env=env,
                        check=False,
                    )
                    self.assertEqual(result.returncode, 2)
                    self.assertIn(expected_error, result.stderr)
                    self.assertEqual(result.stdout, "")

            invalid_json_cases = (
                (
                    "duplicate_key",
                    '{"evidence_id":"one","evidence_id":"two"}',
                    "duplicate provenance field: evidence_id",
                ),
                ("malformed_json", "{", "invalid provenance JSON"),
            )
            for name, raw_metadata, expected_error in invalid_json_cases:
                with self.subTest(name=name):
                    provenance_path.write_text(raw_metadata, encoding="utf-8")
                    result = subprocess.run(
                        [
                            sys.executable,
                            "-B",
                            str(cli),
                            str(artifact_path),
                            "--json",
                            "--provenance",
                            str(provenance_path),
                        ],
                        capture_output=True,
                        text=True,
                        env=env,
                        check=False,
                    )
                    self.assertEqual(result.returncode, 2)
                    self.assertIn(expected_error, result.stderr)

            provenance_path.write_bytes(b" " * (dump.MAX_INPUT_SIZE + 1))
            oversized = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(cli),
                    str(artifact_path),
                    "--json",
                    "--provenance",
                    str(provenance_path),
                ],
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )
        self.assertEqual(oversized.returncode, 2)
        self.assertIn("input exceeds maximum size", oversized.stderr)

    def test_cli_rejects_input_below_minimum_size(self):
        cli = Path(__file__).with_name("dump.py")
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            path = Path(temp_dir) / "short.bin"
            path.write_bytes(b"\x00" * 511)
            result = subprocess.run(
                [sys.executable, "-B", str(cli), str(path)],
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )
        self.assertEqual(result.returncode, 1)
        self.assertIn("Buffer too small (< 512 bytes)", result.stderr)

    def test_cli_rejects_file_over_input_limit_before_parsing(self):
        cli = Path(__file__).with_name("dump.py")
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            path = Path(temp_dir) / "oversized.bin"
            with path.open("wb") as output:
                output.truncate(dump.MAX_INPUT_SIZE + 1)
            result = subprocess.run(
                [sys.executable, "-B", str(cli), str(path)],
                capture_output=True,
                text=True,
                env=env,
                check=False,
            )
        self.assertEqual(result.returncode, 2)
        self.assertIn(
            f"input exceeds maximum size of {dump.MAX_INPUT_SIZE} bytes",
            result.stderr,
        )
        self.assertNotIn("Parse failed:", result.stderr)

    def test_bounded_reader_rejects_stream_that_grows_past_limit(self):
        class GrowingStream:
            def __init__(self):
                self.chunks = iter((b"1234", b"56789", b""))

            def read(self, size):
                return next(self.chunks)

        with self.assertRaisesRegex(
            dump.InputTooLargeError, "maximum size of 8 bytes"
        ):
            dump._read_bounded(GrowingStream(), 8)

    def test_input_file_growth_beyond_limit_during_read_is_rejected(self):
        original_reader = dump._read_bounded
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            path = Path(temp_dir) / "growing.bin"
            path.write_bytes(b"1234")

            def grow_then_read(stream, max_size):
                with path.open("ab") as output:
                    output.write(b"56789")
                return original_reader(stream, max_size)

            with mock.patch.object(
                dump, "_read_bounded", side_effect=grow_then_read
            ):
                with self.assertRaisesRegex(
                    dump.InputTooLargeError, "maximum size of 8 bytes"
                ):
                    dump._read_input_file(path, max_size=8)

    def test_input_file_metadata_change_during_read_is_rejected(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            path = Path(temp_dir) / "modified.bin"
            path.write_bytes(b"synthetic input")
            before = path.stat()
            after = SimpleNamespace(
                st_mode=before.st_mode,
                st_size=before.st_size,
                st_mtime_ns=before.st_mtime_ns + 1,
                st_ctime_ns=before.st_ctime_ns,
            )
            with mock.patch.object(dump.os, "fstat", side_effect=(before, after)):
                with self.assertRaisesRegex(
                    dump.InputChangedError, "changed while it was being read"
                ):
                    dump._read_input_file(path)

    def test_cli_reports_unsupported_header_and_invalid_layout(self):
        cli = Path(__file__).with_name("dump.py")
        env = dict(os.environ)
        env["PYTHONDONTWRITEBYTECODE"] = "1"
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp_dir:
            for name, expected in (
                ("unsupported_binary_version.bin", "Unsupported binary header version"),
                ("table_overlaps_table_list.bin", "overlaps binary header or table list"),
            ):
                with self.subTest(name=name):
                    path = Path(temp_dir) / name
                    path.write_bytes(self.fixtures[name])
                    result = subprocess.run(
                        [sys.executable, "-B", str(cli), str(path)],
                        capture_output=True,
                        text=True,
                        env=env,
                        check=False,
                    )
                    self.assertEqual(result.returncode, 1)
                    self.assertIn(expected.lower(), result.stderr.lower())

    def test_hardware_ids_and_version_format(self):
        self.assertEqual(HwId.GC.value, 11)
        self.assertEqual(HwId.DMU.value, 271)
        self.assertEqual(str(IPVersion(12, 0, 1)), "12.0.1")
        self.assertEqual(IPVersion(12, 0, 1), IPVersion(12, 0, 1))
        self.assertNotEqual(IPVersion(12, 0, 1), IPVersion(11, 0, 2))


if __name__ == "__main__":
    unittest.main(verbosity=2)
