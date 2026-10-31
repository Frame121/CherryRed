#!/usr/bin/env python3
"""Static safety contract for the unbuilt Navi33 survey kext.

These checks do not compile or execute a kext and do not establish macOS ABI
compatibility. They guard the intended read-only/explicit-opt-in scaffold.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "src" / "navi33-survey" / "Navi33Survey.cpp"
PLIST = ROOT / "src" / "navi33-survey" / "Info.plist"


class Navi33SurveySafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.plist = PLIST.read_text(encoding="utf-8")

    def test_boot_arg_requires_exact_string_one(self):
        self.assertIn('PE_parse_boot_argn("navi33survey", &enabled, sizeof(enabled))', self.source)
        self.assertIn("enabled != 1", self.source)
        self.assertIn("opt-in only", self.source)

    def test_probe_checks_opt_in_before_device_identity(self):
        opt_in = self.source.index('PE_parse_boot_argn("navi33survey"')
        identity = self.source.index("pci->configRead16(kIOPCIConfigVendorID)", opt_in)
        self.assertLess(opt_in, identity)
        self.assertIn("pci->configRead16(kIOPCIConfigDeviceID) != kNavi33Device", self.source)

    def test_personality_is_narrowly_matched_to_rx7600_pci_id(self):
        self.assertIn("<string>0x74801002</string>", self.plist)
        self.assertIn("<string>IOPCIDevice</string>", self.plist)

    def test_survey_has_no_register_mapping_or_device_writes(self):
        forbidden = (
            "mapDeviceMemoryWithRegister",
            "setMemoryEnable(",
            "configWrite8(",
            "configWrite16(",
            "configWrite32(",
            "writeRegister(",
            "write32(",
            "write16(",
            "write8(",
        )
        for token in forbidden:
            with self.subTest(token=token):
                self.assertNotIn(token, self.source)

    def test_read_only_survey_scope_is_documented(self):
        self.assertIn("READ-ONLY", self.source)
        self.assertIn("no register BAR mapped, no writes", self.source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
