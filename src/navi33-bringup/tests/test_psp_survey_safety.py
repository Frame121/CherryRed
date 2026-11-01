#!/usr/bin/env python3
"""Static safety contract for the PSP/SMU mailbox survey stage.

Guards the Navi 33 PSP/SMU mailbox read stage: it may map the MMIO register
BAR (BAR5) and read registers, but it must keep the zero-write contract, wait
bounds must be finite, every read must be bounds-checked against the mapped
BAR, and PCI memory decode must never be enabled by the kext. These checks do
not compile or execute a kext and do not establish macOS ABI compatibility.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SURVEY = ROOT / "src" / "navi33-bringup" / "Navi33PspSurvey.cpp"
PURE_HEADER = ROOT / "src" / "navi33-bringup" / "Navi33PspMailbox.hpp"
BRINGUP = ROOT / "src" / "navi33-bringup" / "Navi33Bringup.cpp"
MAKEFILE = ROOT / "src" / "navi33-bringup" / "Makefile"


class PspSurveySafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.survey = SURVEY.read_text(encoding="utf-8")
        cls.header = PURE_HEADER.read_text(encoding="utf-8")
        cls.bringup = BRINGUP.read_text(encoding="utf-8")
        cls.make = MAKEFILE.read_text(encoding="utf-8")

    def test_mapping_is_bar5_and_the_only_mapping(self):
        self.assertIn("kRegBarIndex * 4", self.survey,
                      "the register BAR must be selected through kRegBarIndex")
        self.assertEqual(self.survey.count("mapDeviceMemoryWithRegister"), 1,
                         "the survey must map exactly one BAR (BAR5)")

    def test_zero_write_contract_holds(self):
        forbidden = (
            "setMemoryEnable(",
            "configWrite8(",
            "configWrite16(",
            "configWrite32(",
            "writeRegister(",
            "write32(",
            "write16(",
            "write8(",
            " vramWrite(",
        )
        for token in forbidden:
            with self.subTest(token=token):
                self.assertNotIn(token, self.survey)
        # The command register may only be read; enabling memory decode is
        # refused instead of performed.
        self.assertIn("configRead16(kIOPCIConfigCommand)", self.survey)
        self.assertIn("refusing to enable it", self.survey)
        self.assertIn('"Navi33Bringup,PspStage,HardwareWrites", false', self.survey)
        self.assertIn('"Navi33Bringup,PspStage,RegisterWrites", false', self.survey)

    def test_every_read_is_bounds_checked(self):
        self.assertIn("dwordInBar(off, barLength)", self.survey,
                      "reads must be gated by the dwordInBar bounds check")
        self.assertIn("off / 4", self.survey,
                      "the read must index the mapped window with the checked offset")

    def test_ifwi_wait_is_bounded(self):
        self.assertIn("waitedMs < 2000", self.survey,
                      "the IFWI-init wait must be bounded (2 s, as on Navi 32)")
        self.assertIn("IfwiInitWaitMs", self.survey,
                      "the observed wait must be published")

    def test_base_provenance_is_recorded(self):
        self.assertIn("4b809b32", self.header,
                      "the captured IP Discovery artifact hash must document "
                      "the MP0/MP1 base provenance")
        self.assertIn("0x16000", self.header,
                      "the captured MP0/MP1 base[0] must be the recorded value")
        self.assertIn("static_assert", self.header,
                      "table index pins must be compile-time asserted")

    def test_bringup_keeps_its_own_no_mapping_contract(self):
        self.assertNotIn("mapDeviceMemoryWithRegister", self.bringup,
                         "Navi33Bringup.cpp must stay free of BAR mapping")

    def test_survey_is_part_of_the_kext_build(self):
        self.assertIn("Navi33PspSurvey.cpp", self.make,
                      "the survey translation unit must be in CXX_SRCS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
