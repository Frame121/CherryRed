"""Check diagnostic match scope and identity agreement in kmod metadata."""

import plistlib
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ManifestTests(unittest.TestCase):
    def test_exact_match_and_metadata(self):
        with (ROOT / "Info.plist").open("rb") as source:
            manifest = plistlib.load(source)
        self.assertEqual(manifest["CFBundleExecutable"], "Navi33Probe")
        self.assertEqual(manifest["CFBundlePackageType"], "KEXT")
        personalities = manifest["IOKitPersonalities"]
        self.assertEqual(set(personalities), {"Navi33Probe"})
        personality = personalities["Navi33Probe"]
        self.assertEqual(personality["IOPCIPrimaryMatch"], "0x74801002")
        self.assertEqual(personality["IOProviderClass"], "IOPCIDevice")
        self.assertEqual(personality["IOClass"], "Navi33Probe")
        self.assertEqual(personality["CFBundleIdentifier"], manifest["CFBundleIdentifier"])
        glue = (ROOT / "src" / "kmod_info.c").read_text(encoding="utf-8")
        match = re.search(r'KMOD_EXPLICIT_DECL\(([^,]+),\s*"([^"]+)"', glue)
        self.assertIsNotNone(match)
        self.assertEqual(match[1], manifest["CFBundleIdentifier"])
        self.assertEqual(match[2], manifest["CFBundleVersion"])
        self.assertEqual(manifest["CFBundleVersion"], manifest["CFBundleShortVersionString"])


if __name__ == "__main__":
    unittest.main()
