#!/usr/bin/env python3
"""Static safety contract for the build-only Navi 33 DriverKit PCI survey."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
PROBE = ROOT / "src/navi33-research/driverkit-pci-probe/Navi33PciProbe.cpp"
PLIST = ROOT / "src/navi33-research/driverkit-pci-probe/Info.plist"
BUILD = ROOT / "src/navi33-research/driverkit-pci-probe/build_compile_check.sh"


class Navi33PciProbeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = PROBE.read_text(encoding="utf-8")
        cls.plist = PLIST.read_text(encoding="utf-8")
        cls.build = BUILD.read_text(encoding="utf-8")

    def test_personality_is_narrowly_matched_to_captured_board(self):
        self.assertIn("0x74801002", self.plist)
        self.assertIn("0x2421148c", self.plist)

    def test_probe_only_queries_bar_metadata(self):
        self.assertIn("GetBARInfo", self.source)
        self.assertIn("pci->Open(this, 0)", self.source)
        self.assertIn("pci->Close(this, 0)", self.source)
        self.assertNotIn("MemoryRead", self.source)
        self.assertNotIn("MemoryWrite", self.source)
        self.assertNotIn("ConfigurationWrite", self.source)

    def test_probe_does_not_attempt_gpu_initialization(self):
        forbidden = (
            "LoadFirmware", "PSPInit", "SMUInit", "GFXInit",
            "SDMAInit", "MESInit", "Reset(", "InitializeGPU",
        )
        for symbol in forbidden:
            with self.subTest(symbol=symbol):
                self.assertNotIn(symbol, self.source)

    def test_build_check_does_not_install_or_activate_bundle(self):
        self.assertIn("-bundle", self.build)
        self.assertNotIn("systemextensionsctl", self.build)
        self.assertNotIn("launchctl", self.build)
        self.assertNotIn("sudo ", self.build)
        self.assertNotIn("kmutil load", self.build)


if __name__ == "__main__":
    unittest.main(verbosity=2)
