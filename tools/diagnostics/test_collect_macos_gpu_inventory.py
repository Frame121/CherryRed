import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import collect_macos_gpu_inventory as collector


class MacInventoryParsingTests(unittest.TestCase):
    def test_decode_little_endian_ioreg_identity(self):
        text = """
          |   "vendor-id" = <02100000>
          |   "device-id" = <80740000>
          |   "revision-id" = <cf000000>
          |   "subsystem-vendor-id" = <8c140000>
          |   "subsystem-id" = <21240000>
        """
        self.assertEqual(
            collector.parse_ioreg_properties(text),
            {
                "vendor-id": 0x1002,
                "device-id": 0x7480,
                "revision-id": 0xCF,
                "subsystem-vendor-id": 0x148C,
                "subsystem-id": 0x2421,
            },
        )

    def test_system_profiler_keeps_gpu_summary_and_filters_other_data(self):
        payload = {
            "SPDisplaysDataType": [
                {
                    "_name": "spdisplays_display",
                    "sppci_device_type": "spdisplays_gpu",
                    "spdisplays_device-id": "0x7480",
                    "spdisplays_pcie_width": "x8",
                    "spdisplays_vram": "7 MB",
                    "spdisplays_vendor": "sppci_vendor_amd",
                    "sppci_kextinfo": "sppci_kextnotloaded",
                    "spdisplays_ndrvs": [
                        {"_spdisplays_display-serial-number2": "should not be included"}
                    ],
                },
                {"sppci_device_type": "spdisplays_display"},
            ]
        }
        summary = collector.parse_system_profiler(payload)
        self.assertEqual(len(summary), 1)
        self.assertEqual(summary[0]["spdisplays_device-id"], "0x7480")
        self.assertNotIn("spdisplays_ndrvs", summary[0])

    def test_ioaccelerator_count(self):
        self.assertEqual(collector.parse_ioaccelerator_count(""), 0)
        text = (
            "+-o AMDAccelA <class IOAccelerator, id 1>\n"
            "+-o AMDAccelB <class IOAccelerator, id 2>\n"
        )
        self.assertEqual(collector.parse_ioaccelerator_count(text), 2)

    def test_loaded_extension_summary_omits_addresses_and_uuids(self):
        text = (
            "  134 0 0xffffff7f94812000 0xc52a0 0xc52a0 "
            "com.apple.kext.AMDSupport (7.0.1) C5CB19A5-ECA9-33C9-A372-F4A348BA25FA <120>\n"
        )
        self.assertEqual(
            collector.parse_loaded_amd_extensions(text),
            [{"bundle_id": "com.apple.kext.AMDSupport", "version": "7.0.1"}],
        )

    def test_parse_metal_no_devices(self):
        result = collector.parse_metal_probe(
            "Metal devices visible: 0\n"
            "Default Metal device: none\n"
        )
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["device_count"], 0)
        self.assertEqual(result["devices"], [])
        self.assertIsNone(result["default_device"])

    def test_parse_metal_device_summary(self):
        result = collector.parse_metal_probe(
            "Metal devices visible: 1\n"
            "[Device 0]\n"
            "Name: AMD Radeon Example\n"
            "Registry ID: 0x1234\n"
            "Default Metal device: AMD Radeon Example\n"
        )
        self.assertEqual(result["device_count"], 1)
        self.assertEqual(
            result["devices"],
            [{"index": 0, "name": "AMD Radeon Example", "registry_id": "0x1234"}],
        )
        self.assertEqual(result["default_device"], "AMD Radeon Example")

    def test_current_boot_assessment_reflects_missing_acceleration(self):
        result = collector.assess_current_boot(
            {"vendor-id": 0x1002, "device-id": 0x7480},
            [],
            accelerator_count=0,
            metal={"device_count": 0},
        )
        self.assertTrue(result["rx7600_pci_detected"])
        self.assertEqual(result["status"], "rx7600_not_accelerated_on_current_boot")

    def test_write_new_file_refuses_to_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "snapshot.json"
            path.write_text("keep", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                collector.write_new_file(path, "replace")
            self.assertEqual(path.read_text(encoding="utf-8"), "keep")


if __name__ == "__main__":
    unittest.main()
