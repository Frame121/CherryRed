# Stage 4 pre-comparison — userspace PCI facts vs Linux evidence (2026-10-31)

Read-only userspace comparison, recorded BEFORE the kernel survey (Stage 3).
This is NOT the kernel-survey comparison; the authoritative per-BAR/ReBAR data
comes from the `Navi33Bringup` kext after it is injected and booted with
`navi33bringup=1` (see `stage3_verify_after_reboot.sh`).

Sources:
- macOS: `ioreg -rln GFX0` on Tahoe 26.7.1 (userspace-gpu-node-2026-10-31.txt)
- Linux: `RX7600-Linux-Evidence/pci-resource-survey.md` (BDF 0000:03:00.0,
  kernel 7.2.11-arch1-1, amdgpu bound)

## Identity — all agree

| Field | macOS | Linux | Status |
|---|---|---|---|
| vendor:device | 1002:7480 | 1002:7480 | PASS |
| subsystem | 148c:2421 (compatible string) | 148c:2421 | PASS |
| revision | 0x00cf | (Windows probe rev CF) | PASS |
| class | 0x030000 display/VGA | display | PASS |
| PCIe link | 0x1084 = Gen4 x8 | Gen4 x8 | PASS |
| VRAM reported | 8192 MiB (`VRAM,totalMB`) | 8176 MB (HMM) | PASS (platform rounding) |

## PCI BARs — decoded from `assigned-addresses` (Apple little-endian cells)

| BAR | macOS size | Linux size | Verdict |
|---|---|---|---|
| BAR0 (0x10, 64-bit PF) | 256 MiB @ 0x880000000 | 8 GiB @ 0xfa00000000 | INFO — ReBAR-programmable; per-OS size |
| BAR2 (0x18, 64-bit PF) | 2 MiB @ 0x890000000 | 256 MiB @ 0xfc00000000 | INFO — ReBAR-programmable; per-OS size |
| BAR4 (0x20, I/O) | 256 B @ 0x1000 | 256 B @ 0xf000 | PASS (size identical) |
| BAR5 (0x24, 32-bit MEM) | 1 MiB @ 0xfcb00000 | 1 MiB @ 0xfcb00000 | PASS (size AND base identical) |
| BAR6 ROM (0x30) | 128 KiB @ 0xfcc00000 | 128 KiB @ 0xfcc00000 | PASS (size AND base identical) |

Key finding: the two BARs whose sizes differ between the operating systems are
exactly the two 64-bit prefetchable BARs — the Resizable-BAR set. All three
non-resizable BARs match Linux byte-for-byte (BAR5 and the ROM window even at
the same base addresses). This is consistent with ReBAR being programmed
differently per OS (Linux amdgpu -> BAR0 = 8 GiB; macOS -> BAR0 = 256 MiB,
BAR2 = 2 MiB) and is expected OS behavior, not a hardware discrepancy.

The kernel survey (Stage 3/4) will read the ReBAR capability raw values
(`ResizableBAR*` properties: supported-size bitmask, BAR count, current size
encoding) and close this question from inside the kernel.

## Driver/acceleration state at capture time

- `AMDSupport` is attached directly under the GPU function (generic display
  class match); `.Display_boot` is an `IONDRVFramebuffer` — the basic,
  non-accelerated display path that is currently driving the desktop.
- The GPU sits behind a PCIe switch: `PCI0/GPP0@1,1/SWUS@0/SWDS@0/GFX0@0`
  (matches Linux BDF 0000:03:00.0 topology).
- `HDAU@0,1` (audio function, pci1002,ab30, 3:0:1) has its own 16 KiB BAR.
- No `IOAccelerator`, no Metal device — unchanged, acceleration BLOCKED until
  a real driver stack exists.

## What only the kext survey can add

- ReBAR capability/controls: supported sizes bitmask, resizable-BAR count,
  current size encoding(s), indices match.
- Authoritative per-BAR `IOMemoryDescriptor` metadata as the kernel sees it.
- Fail-closed markers: `FirmwareLoaded=false`, `HardwareWrites=false`,
  `FirmwareRefused=true`, `RegisterBARMapped=false`.
- `ExactEvidenceBoardMatch=true` (subsystem read from config space in-kernel).
