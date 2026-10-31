#!/usr/bin/env python3
"""Static safety contract for the Navi33Accelerator service skeleton.

The accelerator is the C6/IOAccelerator-direction service. It must never
touch hardware (the gate is evaluated from published evidence properties
only), it must refuse while the bring-up layers are unverified, and its
Info.plist personality must attach to the bring-up node — never to the PCI
device directly. These checks do not compile or execute a kext.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
ACCELERATOR = ROOT / "src" / "navi33-bringup" / "Navi33Accelerator.cpp"
CONTRACTS = ROOT / "src" / "navi33-research" / "include" / "Navi33ClientContracts.hpp"
PLIST = ROOT / "src" / "navi33-bringup" / "Info.plist"
MAKEFILE = ROOT / "src" / "navi33-bringup" / "Makefile"


class AcceleratorSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.acc = ACCELERATOR.read_text(encoding="utf-8")
        cls.contracts = CONTRACTS.read_text(encoding="utf-8")
        cls.plist = PLIST.read_text(encoding="utf-8")
        cls.make = MAKEFILE.read_text(encoding="utf-8")

    def test_zero_hardware_access_in_every_state(self):
        forbidden = (
            "mapDeviceMemoryWithRegister",
            "getDeviceMemoryWithRegister",
            "setMemoryEnable(",
            "configRead",
            "configWrite",
            "IOMemoryDescriptor",
        )
        for token in forbidden:
            with self.subTest(token=token):
                self.assertNotIn(token, self.acc)

    def test_gate_refusal_is_published(self):
        self.assertIn('"Navi33Accelerator,State"', self.acc)
        self.assertIn('"Navi33Accelerator,BlockedReason"', self.acc)
        self.assertIn('"Navi33Accelerator,MetalDispatchAvailable", false', self.acc)
        self.assertIn('"Navi33Accelerator,HardwareAccess", false', self.acc)
        self.assertIn('"Navi33Accelerator,FirmwareLoaded", false', self.acc)

    def test_gate_reads_the_named_evidence_properties(self):
        for key in ("Navi33Bringup,Stage",
                    "Navi33Bringup,FirmwareVerified",
                    "Navi33Bringup,MemoryVerified",
                    "Navi33Bringup,QueueVerified"):
            self.assertIn(key, self.acc, f"evidence property contract: {key}")

    def test_personality_attaches_to_bringup_node_not_pci(self):
        self.assertIn("<key>Navi33Accelerator</key>", self.plist)
        self.assertIn("<string>Navi33Accelerator</string>", self.plist)
        self.assertIn("<string>Navi33Bringup</string>", self.plist)
        # The accelerator must not carry a PCI match of its own.
        self.assertNotIn("0x74801002\n\t\t\t<key>IOClass</key>\n\t\t\t<string>Navi33Accelerator",
                         self.plist)

    def test_build_contains_the_accelerator(self):
        self.assertIn("Navi33Accelerator.cpp", self.make)
        self.assertIn("-I../navi33-research/include", self.make)

    def test_compute_gate_demands_exactly_the_compute_layers(self):
        gate = self.contracts[self.contracts.index("ClientError computeGate"):]
        gate = gate[:gate.index("\n}")]
        for layer in ("pciTransport", "firmware", "memory", "queueSubmission"):
            self.assertIn(f"verified(readiness.{layer})", gate,
                          f"the compute gate must require {layer}")
        for upper in ("ioAccelerator", "metal", "display"):
            self.assertNotIn(f"verified(readiness.{upper})", gate,
                             f"the compute gate must NOT require {layer}")

    def test_handle_generations_are_per_slot(self):
        self.assertIn("++slots_[slot].generation;", self.contracts,
                      "slot generation advances on reuse, invalidating stale handles")
        self.assertIn("slots_[s].generation != generationOf(handle)",
                      self.contracts,
                      "lookups must reject stale generation tags")


if __name__ == "__main__":
    unittest.main(verbosity=2)
