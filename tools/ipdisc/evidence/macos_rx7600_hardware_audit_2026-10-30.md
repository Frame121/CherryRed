# RX 7600 macOS hardware audit — 2026-10-30

Host-side, read-only observation on the connected macOS Tahoe 26.7.1 host
(build 25G241, x86_64, SMBIOS MacPro7,1). No hardware registers were read or
written by this audit.

## Confirmed PCI identity

| Property | Observed value |
|---|---|
| GPU model reported by IORegistry | AMD Radeon RX 7600 |
| PCI vendor/device | `1002:7480` |
| PCI subsystem vendor/device | `148c:2421` |
| Revision | `0x00cf` |
| PCI class | `0x030000` (display controller) |
| PCI location | `3:0:0` |
| PCIe negotiated link status | raw `IOPCIExpressLinkStatus = 4228 = 0x1084`; decoded as 16 GT/s, x8 (PCIe Gen4 x8) |
| VRAM reported by system_profiler | 8 GB |
| IORegistry `VRAM,totalMB` | present; byte encoding `00 20 00 00` (8,192 MiB) |

## Assigned PCI resources

IORegistry reports `IOPCIResourced = Yes`, `IODeviceMemory` descriptors, and
`assigned-addresses`. The observed resource lengths include 256 MiB, 2 MiB,
1 MiB, and 128 KiB regions. These are OS-assigned resource descriptors only;
they do not prove that GPU MMIO, VRAM, or any engine is initialized or safe to
access. This audit did not map BARs or access their contents.

## Driver / acceleration state

- `AMDSupport` matches the generic PCI display class.
- The GPU has an `IONDRVFramebuffer` display path, which is sufficient for the
  currently visible basic display output; it is not evidence of acceleration.
- No `IOAccelerator` service was found for this GPU.
- `MTLCopyAllDevices()` returned `[]`; `MTLCreateSystemDefaultDevice()` was nil.
- Prior `kmutil showloaded` audit found no attached `AMDRadeonX6000` accelerator
  for this device. Apple Tahoe X6000 accelerator, framebuffer, and HW-services
  personalities omit PCI ID `1002:7480`.

## ROM investigation — important distinction

A `rom@0` node exists in the **IODeviceTree** and reports:

- `vendor = Acidanthera`
- `rom-size = 16777216` (16 MiB)
- `version = 2094.80.5.0.0`
- `release-date = 01/21/2026`

This is OpenCore/Acidanthera boot firmware metadata, not a dump of the RX 7600
VBIOS. The node has no ROM image byte property exposed by the inspected
IORegistry output. The similarly named IOService query returned no GPU ROM
service. No VBIOS binary was extracted. Do not label this 16 MiB value as the
RX 7600 VBIOS size.

No `VFCT`, `ATOMBIOS`, or GPU VBIOS image property was exposed in the inspected
IODeviceTree output. macOS on this host therefore did not provide a confirmed
read-only path to the board VBIOS or AMD IP Discovery table through the
interfaces inspected here.

## Artifacts

The full raw command captures are in the host Desktop directory:

`~/Desktop/RX7600-macOS-hardware-audit-20261030-222854/`

- `audit.txt`
- `targeted.txt`
- `rom-investigation.txt`

## Conclusion / next gate

The host can confirm the exact PCI/subsystem identity, negotiated PCIe link,
VRAM reported by the platform, and assigned PCI resource metadata. It cannot,
through the inspected stock macOS interfaces, provide the board-attributed
VBIOS or physical AMD IP Discovery payload needed for firmware mapping.

Do not read arbitrary BAR offsets, map GPU memory, force driver matching, or
attempt GPU reset/firmware loading to obtain these artifacts. Continue with a
Linux AMDGPU capture for IP Discovery and VBIOS, or a separately reviewed,
board-specific, read-only acquisition method. Hardware acceleration remains
unavailable: no IOAccelerator service and no Metal device were observed.
