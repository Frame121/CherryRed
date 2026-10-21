"""Static routing-contract checks for the isolated Navi 33 research model."""

import plistlib
import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
NAVI48_PLIST = REPOSITORY_ROOT / "src" / "navi48-bringup" / "Info.plist"
NAVI48_SOURCE = REPOSITORY_ROOT / "src" / "navi48-bringup" / "src" / "Navi48Bringup.cpp"
NAVI33_CORE = REPOSITORY_ROOT / "src" / "navi33-research" / "src" / "Navi33DriverCore.cpp"
NAVI33_HEADER = REPOSITORY_ROOT / "src" / "navi33-research" / "include" / "Navi33DriverCore.hpp"
NAVI33_CMAKE = REPOSITORY_ROOT / "src" / "navi33-research" / "CMakeLists.txt"
EXPECTED_NAVI48_MATCHES = {"0x75501002", "0x75511002"}


class Navi48IsolationTests(unittest.TestCase):
    def test_navi48_personalities_match_only_navi48_pci_ids(self):
        with NAVI48_PLIST.open("rb") as source:
            personalities = plistlib.load(source)["IOKitPersonalities"]

        declared_matches = {
            device_id
            for personality in personalities.values()
            for device_id in personality.get("IOPCIPrimaryMatch", "").split()
        }
        self.assertEqual(declared_matches, EXPECTED_NAVI48_MATCHES)

    def test_runtime_probe_requires_opt_in_and_navi48_identity(self):
        source = NAVI48_SOURCE.read_text(encoding="utf-8")
        probe_match = re.search(
            r"IOService\s*\*\s*Navi48Bringup::probe\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(probe_match, "Navi48Bringup::probe must exist")
        probe = probe_match.group("body")

        opt_in = re.search(
            r'PE_parse_boot_argn\("navi48bringup",\s*&on,\s*sizeof\(on\)\)'
            r".*?on\s*==\s*0",
            probe,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(opt_in, "runtime probe must require navi48bringup opt-in")

        vendor_check = re.search(
            r"\(vendorDevice\s*&\s*0xffff\)\s*!=\s*0x1002",
            probe,
        )
        self.assertIsNotNone(vendor_check, "runtime probe must require AMD vendor ID")

        device_check = re.search(
            r"device\s*!=\s*0x7550\s*&&\s*device\s*!=\s*0x7551",
            probe,
        )
        self.assertIsNotNone(
            device_check,
            "runtime probe must reject every device except Navi48 IDs 7550/7551",
        )

    def test_start_revalidates_identity_before_hardware_setup(self):
        source = NAVI48_SOURCE.read_text(encoding="utf-8")
        start_match = re.search(
            r"bool\s+Navi48Bringup::start\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(start_match, "Navi48Bringup::start must exist")
        start = start_match.group("body")

        pci_provider = re.search(
            r"pciDevice\s*=\s*OSDynamicCast\(IOPCIDevice,\s*provider\)",
            start,
        )
        self.assertIsNotNone(pci_provider, "start must validate its PCI provider")
        identity_guard = re.search(
            r"configRead32\(kIOPCIConfigVendorID\).*?"
            r"\(vendorDevice\s*&\s*0xffff\)\s*!=\s*0x1002\s*\|\|"
            r"\s*\(device\s*!=\s*0x7550\s*&&\s*device\s*!=\s*0x7551\)",
            start,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(
            identity_guard,
            "start must reject every vendor/device ID except Navi48 before setup",
        )

        setup_points = [
            start.find("pciDevice->setMemoryEnable(true)"),
            start.find("latchPciBars(pciDevice)"),
            start.find("mapRegisters()"),
        ]
        self.assertTrue(
            all(position >= 0 for position in setup_points),
            "start must retain its expected hardware-setup sequence",
        )
        self.assertLess(
            identity_guard.start(),
            min(setup_points),
            "identity validation must precede PCI memory enable, BAR access, and MMIO mapping",
        )

    def test_navi33_core_is_independent_of_navi48_initialization(self):
        core_source = NAVI33_CORE.read_text(encoding="utf-8")
        core_header = NAVI33_HEADER.read_text(encoding="utf-8")
        cmake = NAVI33_CMAKE.read_text(encoding="utf-8")

        self.assertNotRegex(
            core_source + core_header,
            r"\bNavi48Bringup\b|navi48-bringup|Navi48::",
            "Navi 33 lifecycle core must not call or link the Navi 48 initializer",
        )
        self.assertIn(
            "Navi33LifecycleResult Navi33DriverCore::identify",
            core_source,
            "Navi 33 identity handling must remain in its own portable core",
        )
        self.assertIn(
            "src/Navi33DriverCore.cpp",
            cmake,
            "the separate Navi 33 core must be built by its own CMake target",
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
