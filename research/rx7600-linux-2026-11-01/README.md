# RX 7600 / Navi 33 Linux Hardware Research

**Capture date:** 2026-11-01
**Target:** AMD Radeon RX 7600, Navi 33, PCI ID `1002:7480`
**Platform:** CachyOS / Linux x86_64, `amdgpu` kernel driver

This folder records read-only Linux-side observations intended to inform future CherryRed/macOS driver research. It is hardware evidence and **not** proof that RX 7600 acceleration or Metal works in macOS.

## Findings

- AMD IP Discovery export decoded as IP Discovery v3: one die, 39 IP records; 37 sysfs aliases cross-checked with all 37 matching.
- The GPU reports GC IP 11.0.2, SDMA 6.0.2, UMC 8.11.0, DMU 3.2.1 and MP 13.0.7.
- VBIOS identifies NAVI33 and an 8 GB GDDR6 Fighter D74510 XL board.
- Linux exposes 8,176 MiB of HMM-registered VRAM and GFX, compute, SDMA, VCN and JPEG engine/ring families.
- Display Core is 3.2.392 / DCN 3.2.1. DP-1 was active at 1920x1080, approximately 180 Hz.
- A 14.997-second display trace showed atomic-check/commit activity and vblank events consistent with approximately 180 Hz output.

## Documents

- [IP Discovery](IP-DISCOVERY.md)
- [Firmware and VBIOS analysis](FIRMWARE-VBIOS.md)
- [DCN display trace](DCN-TRACE.md)
- [Command submission research](COMMAND-SUBMISSION.md)
- [Evidence handling and privacy](EVIDENCE-HANDLING.md)

## Important limitations

This is Linux evidence from a physical Navi 33 device. It does not describe Apple's AGX driver, Metal internals, macOS AMD driver matching, or the complete command processor ABI. A live GPU command-submission trace has not yet been captured. No claim of macOS driver support, hardware acceleration, or Metal functionality is made.
