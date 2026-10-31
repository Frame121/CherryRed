#!/usr/bin/env python3
"""Safety contract test for Navi33Memory (Stage 5.4 / C2 GFXHUB v3.0 GART bring-up)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
BRINGUP_DIR = ROOT / "src" / "navi33-bringup"
MEM_CPP = BRINGUP_DIR / "Navi33Memory.cpp"
BRINGUP_CPP = BRINGUP_DIR / "Navi33Bringup.cpp"
MAKEFILE = BRINGUP_DIR / "Makefile"


class MemorySafetyTests(unittest.TestCase):
    def test_opt_in_ladder(self):
        src = BRINGUP_CPP.read_text(encoding="utf-8")
        self.assertIn('bootArgTokenPresent(args, "navi33mem=1")', src)
        self.assertIn('RunNavi33MemoryStage(pci, this)', src)

    def test_bounds_checked_access(self):
        src = MEM_CPP.read_text(encoding="utf-8")
        self.assertIn("bar.rd(", src)
        self.assertIn("bar.wr(", src)
        self.assertIn("vramWrite(bar,", src)
        self.assertIn("vramRead(bar,", src)

    def test_memory_verified_publication(self):
        src = MEM_CPP.read_text(encoding="utf-8")
        self.assertIn('"Navi33Bringup,MemoryVerified", true', src)
        self.assertIn('"Navi33Bringup,MemoryVerified", false', src)

    def test_makefile_includes_memory(self):
        make = MAKEFILE.read_text(encoding="utf-8")
        self.assertIn("Navi33Memory.cpp", make)


if __name__ == "__main__":
    unittest.main()
