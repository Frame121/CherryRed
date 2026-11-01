#!/usr/bin/env python
"""Host test for the RX 7600 (Navi 33) firmware + IP manifest.

Runs on the host (no macOS, no hardware). Validates that the manifest is well
formed, internally consistent, and matches the card's own captured evidence.
This does NOT establish that the blobs load on macOS or are redistributable.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "RX7600-Linux-Evidence" / "navi33-firmware-manifest.json"
CORRELATION = ROOT / "RX7600-Linux-Evidence" / "firmware-source-correlation.json"

# Ground truth from the card's own captured evidence (firmware-dependency-audit.md
# boot journal + firmware-source-correlation.json). These are the versions THIS
# RX 7600 reported, not generic RDNA 3 defaults.
EXPECTED_IP = {
    "GC": "11.0.2",
    "SDMA": "6.0.2",
    "VCN": "4.0.4",
    "DCN": "3.2.1",
    "PSP": "13.0.7",
    "SMU": "13.0.7",
}
EXPECTED_REQUIRED_BLOBS = {
    "gc_11_0_2_imu.bin",
    "gc_11_0_2_me.bin",
    "gc_11_0_2_mec.bin",
    "gc_11_0_2_mes.bin",
    "gc_11_0_2_mes1.bin",
    "gc_11_0_2_mes_2.bin",
    "gc_11_0_2_pfp.bin",
    "gc_11_0_2_rlc.bin",
    "sdma_6_0_2.bin",
    "vcn_4_0_4.bin",
    "dcn_3_2_1_dmcub.bin",
    "psp_13_0_7_sos.bin",
    "psp_13_0_7_ta.bin",
    "smu_13_0_7.bin",
}


def load(path):
    with path.open(encoding="utf-8") as fh:
        return json.load(fh)


class ManifestTests:
    def __init__(self):
        self.manifest = load(MANIFEST)
        self.correlation = load(CORRELATION)
        self.failures = []

    def check(self, cond, msg):
        if not cond:
            self.failures.append(msg)

    def run(self):
        m = self.manifest
        # Well-formed top level.
        self.check(m.get("schema_version") == 1, "schema_version must be 1")
        self.check(m["gpu"]["pci_device"] == "0x7480", "gpu pci_device must be 0x7480")
        self.check(m["gpu"]["gfx_target"] == "gfx1102", "gfx_target must be gfx1102")

        # IP versions match the card's own captured evidence.
        for ip, ver in EXPECTED_IP.items():
            self.check(m["ip_blocks"].get(ip, {}).get("version") == ver,
                       f"ip_blocks[{ip}].version must be {ver}, got {m['ip_blocks'].get(ip, {}).get('version')}")

        # Required firmware set matches the captured evidence exactly.
        required = {Path(f["path"]).name for f in m["firmware"] if f.get("required")}
        self.check(required == EXPECTED_REQUIRED_BLOBS,
                   "required firmware set mismatch: "
                   f"missing={sorted(EXPECTED_REQUIRED_BLOBS - required)} "
                   f"extra={sorted(required - EXPECTED_REQUIRED_BLOBS)}")

        # Every firmware entry has a source reference and a role.
        for f in m["firmware"]:
            self.check(bool(f.get("source_ref")), f"firmware {f.get('path')} missing source_ref")
            self.check(bool(f.get("role")), f"firmware {f.get('path')} missing role")

        # No Navi 48 / RDNA 4 firmware leaked into the Navi 33 manifest.
        for f in m["firmware"]:
            self.check("12_0_1" not in f["path"] and "14_0_3" not in f["path"],
                       f"Navi 48 blob leaked into Navi 33 manifest: {f['path']}")

        # Unresolved HWIDs are documented (282, 286), not silently dropped.
        unresolved = {u["hwid"] for u in m["unresolved_hwids"]}
        self.check(unresolved == {282, 286},
                   f"unresolved_hwids must be {{282, 286}}, got {sorted(unresolved)}")

        # Cross-check: every manifest source_ref that is a MODULE_FIRMWARE path
        # should appear in the project's own correlation evidence.
        corr_paths = {e["path"] for e in self.correlation["entries"]}
        for f in m["firmware"]:
            if f.get("module_firmware"):
                self.check(f["path"] in corr_paths,
                           f"module_firmware {f['path']} not in firmware-source-correlation.json")

        # Provenance integrity: IP discovery SHA must match the evidence doc.
        self.check(
            m["provenance"]["ip_discovery_sha256"]
            == "4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89",
            "ip_discovery_sha256 does not match the captured artifact",
        )

        return self.failures


def main():
    t = ManifestTests()
    failures = t.run()
    if failures:
        print("MANIFEST TEST: FAIL")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print("MANIFEST TEST: PASS (%d required blobs, IP versions match captured evidence)"
          % len(EXPECTED_REQUIRED_BLOBS))


if __name__ == "__main__":
    main()
