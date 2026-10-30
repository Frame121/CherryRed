#!/usr/bin/env python3
"""Tests for verify_board_firmware_manifest.py using synthetic fixtures only."""
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from verify_board_firmware_manifest import ConsistencyError, verify


def _ip_record(hw_id, version, name="SYN", instance=0):
    return {
        "hw_id": hw_id,
        "hw_id_name": name,
        "instance": instance,
        "version": version,
        "num_bases": 1,
        "bases": [0],
    }


def _sidecar(records, valid=True, mode="strict", sha256="a" * 64):
    return {
        "analysis": {"computed_sha256": sha256, "input_size": 10240},
        "parse": {"valid": valid, "mode": mode, "ip_count": len(records), "ips": records},
    }


def _manifest(overrides=None):
    manifest = {
        "schema_version": 1,
        "gpu": {
            "pci_vendor": "0x1002",
            "pci_device": "0x7480",
            "subsystem_vendor": "0x148c",
            "subsystem_device": "0x2421",
        },
        "ip_blocks": {
            "GC": {"version": "11.0.2"},
            "SDMA": {"version": "6.0.2"},
            "VCN": {"version": "4.0.4"},
            "DCN": {"version": "3.2.1"},
            "PSP": {"version": "13.0.7"},
            "SMU": {"version": "13.0.7"},
        },
        "unresolved_hwids": [
            {"hwid": 282, "version": "13.0.2", "label": "UNKNOWN_HWID_282"},
            {"hwid": 286, "version": "4.0.2", "label": "UNKNOWN_HWID_286"},
        ],
        "firmware": [
            {"path": "amdgpu/gc_11_0_2_pfp.bin", "ip": "GC", "role": "pfp", "source_ref": "a"},
            {"path": "amdgpu/gc_11_0_2_me.bin", "ip": "GC", "role": "me", "source_ref": "a"},
            {"path": "amdgpu/gc_11_0_2_mec.bin", "ip": "GC", "role": "mec", "source_ref": "a"},
            {"path": "amdgpu/gc_11_0_2_rlc.bin", "ip": "GC", "role": "rlc", "source_ref": "a"},
            {"path": "amdgpu/gc_11_0_2_imu.bin", "ip": "GC", "role": "imu", "source_ref": "a"},
            {"path": "amdgpu/gc_11_0_2_mes.bin", "ip": "GC", "role": "mes", "source_ref": "a"},
            {"path": "amdgpu/gc_11_0_2_mes1.bin", "ip": "GC", "role": "mes1", "source_ref": "a"},
            {"path": "amdgpu/gc_11_0_2_mes_2.bin", "ip": "GC", "role": "mes2", "source_ref": "a"},
            {"path": "amdgpu/psp_13_0_7_sos.bin", "ip": "PSP", "role": "sos", "source_ref": "a"},
            {"path": "amdgpu/psp_13_0_7_ta.bin", "ip": "PSP", "role": "ta", "source_ref": "a"},
            {"path": "amdgpu/smu_13_0_7.bin", "ip": "SMU", "role": "smu", "source_ref": "a"},
            {"path": "amdgpu/sdma_6_0_2.bin", "ip": "SDMA", "role": "sdma", "source_ref": "a"},
            {"path": "amdgpu/vcn_4_0_4.bin", "ip": "VCN", "role": "vcn", "source_ref": "a"},
            {"path": "amdgpu/dcn_3_2_1_dmcub.bin", "ip": "DCN", "role": "dmub", "source_ref": "a"},
        ],
        "provenance": {"ip_discovery_sha256": "a" * 64},
    }
    if overrides:
        overrides(manifest)
    return manifest


def _vbios(overrides=None):
    record = {
        "vbios_identity": {
            "atom_bios_name": "113-EXT86626-001",
            "version_string": "022.003.001.027.000001",
        },
        "gpu": {
            "pci_vendor": "1002",
            "pci_device": "7480",
            "subsystem_vendor": "148c",
            "subsystem_device": "2421",
        },
        "provenance": {
            "source_artifact_sha256": "b" * 64,
            "related_ip_discovery_sha256": "a" * 64,
            "authentication": "NONE",
        },
    }
    if overrides:
        overrides(record)
    return record


class BoardManifestConsistencyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.records = [
            _ip_record(11, "11.0.2", "GC"),
            _ip_record(42, "6.0.2", "SDMA0"),
            _ip_record(43, "6.0.2", "SDMA1"),
            _ip_record(12, "4.0.4", "UNKNOWN_12"),
            _ip_record(255, "13.0.7", "MP0"),
            _ip_record(1, "13.0.7", "MP1"),
            _ip_record(271, "3.2.1", "DMU"),
            _ip_record(282, "13.0.2", "UNKNOWN_282"),
            _ip_record(286, "4.0.2", "UNKNOWN_286"),
        ]

    def tearDown(self):
        self.temp.cleanup()

    def _write(self, name, value):
        path = self.root / name
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_valid_inputs_pass(self):
        report = verify(
            self._write("m.json", _manifest()),
            self._write("p.json", _sidecar(self.records, sha256="a" * 64)),
            self._write("v.json", _vbios()),
        )
        self.assertEqual(report["result"], "PASS")
        self.assertEqual(report["firmware_entries_checked"], 14)
        self.assertEqual(
            report["family_versions"],
            {
                "GC": "11.0.2",
                "PSP": "13.0.7",
                "SMU": "13.0.7",
                "SDMA": "6.0.2",
                "VCN": "4.0.4",
                "DCN": "3.2.1",
            },
        )
        self.assertEqual(report["source_journal_binding"], "SKIPPED")

    def test_source_log_binding(self):
        log = self.root / "journal.txt"
        log.write_bytes(b"SYNTHETIC BOOT JOURNAL")
        vbios = _vbios(
            lambda r: r["provenance"].__setitem__(
                "source_artifact_sha256", hashlib.sha256(b"SYNTHETIC BOOT JOURNAL").hexdigest()
            )
        )
        report = verify(
            self._write("m.json", _manifest()),
            self._write("p.json", _sidecar(self.records)),
            self._write("v.json", vbios),
            source_log=log,
        )
        self.assertTrue(report["source_journal_binding"].startswith("BOUND"))

    def test_wrong_source_log_hash_fails(self):
        log = self.root / "journal.txt"
        log.write_bytes(b"TAMPERED JOURNAL")
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest()),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
                source_log=log,
            )

    def test_family_version_mismatch_fails(self):
        def mutate(m):
            m["ip_blocks"]["GC"]["version"] = "11.5.0"
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )

    def test_firmware_filename_mismatch_fails(self):
        def mutate(m):
            m["firmware"][0]["path"] = "amdgpu/gc_11_0_1_pfp.bin"
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )

    def test_missing_required_role_fails(self):
        def mutate(m):
            m["firmware"] = [f for f in m["firmware"] if f["role"] != "mec"]
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )

    def test_duplicate_firmware_path_fails(self):
        def mutate(m):
            m["firmware"][1] = dict(m["firmware"][0])
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )

    def test_missing_unresolved_hwid_bookkeeping_fails(self):
        def mutate(m):
            m["unresolved_hwids"] = [m["unresolved_hwids"][0]]
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )

    def test_guessed_unresolved_name_fails(self):
        def mutate(m):
            m["unresolved_hwids"][0]["label"] = "LSDMA"
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )

    def test_unresolved_version_mismatch_fails(self):
        records = [r for r in self.records if r["hw_id"] != 282] + [
            _ip_record(282, "13.0.9", "UNKNOWN_282")
        ]
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest()),
                self._write("p.json", _sidecar(records)),
                self._write("v.json", _vbios()),
            )

    def test_compat_mode_sidecar_fails(self):
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest()),
                self._write("p.json", _sidecar(self.records, mode="compatibility")),
                self._write("v.json", _vbios()),
            )

    def test_invalid_sidecar_fails(self):
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest()),
                self._write("p.json", _sidecar(self.records, valid=False)),
                self._write("v.json", _vbios()),
            )

    def test_ipd_hash_mismatch_fails(self):
        def mutate(m):
            m["provenance"]["ip_discovery_sha256"] = "c" * 64
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )

    def test_wrong_gpu_identity_in_vbios_fails(self):
        def mutate(r):
            r["gpu"]["pci_device"] = "7550"
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest()),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios(mutate)),
            )

    def test_malformed_vbios_version_fails(self):
        def mutate(r):
            r["vbios_identity"]["version_string"] = "022.003.001.027"
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest()),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios(mutate)),
            )

    def test_authenticated_claim_fails(self):
        def mutate(r):
            r["provenance"]["authentication"] = "VERIFIED"
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest()),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios(mutate)),
            )

    def test_path_traversal_fails(self):
        def mutate(m):
            m["firmware"][0]["path"] = "amdgpu/../etc/gc_11_0_2_pfp.bin"
        with self.assertRaises(ConsistencyError):
            verify(
                self._write("m.json", _manifest(mutate)),
                self._write("p.json", _sidecar(self.records)),
                self._write("v.json", _vbios()),
            )


if __name__ == "__main__":
    unittest.main()
