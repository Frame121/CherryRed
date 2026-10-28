# FramePC Tahoe EFI staging

This is a locally assembled OpenCore **staging EFI**, matched to the Windows
hardware inventory captured for this computer. `config.plist` passes the
OpenCore 1.0.8 `ocvalidate` check. The EFI has not been boot-tested or copied
to a USB/ESP. Keep it as a draft until the items below are resolved.

## Detected hardware

- Motherboard: Gigabyte A620M S2H (UEFI, AMI F4)
- CPU: AMD Ryzen 5 7500F, Zen 4, 6 cores / 12 threads; no integrated GPU
- Memory: 32 GB (2 x 16 GB) DDR5-4800
- GPU: AMD Radeon RX 7600, PCI `1002:7480`, subsystem `148C:2421`
- Storage: WD Blue SN580 1 TB NVMe
- Ethernet: Realtek RTL8111/8168 (`10EC:8168`)
- Audio: Realtek ALC897 (`10EC:0897`)
- Wi-Fi: TP-Link USB adapter (`2357:013F`); no matching driver is included

## Current contents

`EFI/` contains OpenCore 1.0.8, the AMD Vanilla kernel patches with the four
physical-core replacements set to 6, `SSDT-EC-USBX-DESKTOP.aml`, and six
officially released kexts: Lilu, VirtualSMC, WhateverGreen, AppleALC, NVMeFix,
and RealtekRTL8111. USB port limits remain disabled. No unverified ALC897
layout, GPU spoof, or RX 7600 acceleration claim is configured.

## Required before attempting a boot

1. RX 7600 / Navi 33 is not supported by stock macOS graphics drivers according
   to the current Dortania GPU guide. This EFI cannot provide a usable desktop,
   display output, acceleration, or Metal on that card. The Ryzen 5 7500F has no
   iGPU to use as a fallback.
2. The config now has locally generated MacPro7,1 serial and MLB values and the
   Realtek Ethernet MAC as ROM. Check that the serial is not assigned to a real
   Mac before using Apple services, and keep the local profile private. Do not
   reuse these values on another computer.
3. Create and verify a USB port map for the A620M S2H. No USB map is included.
4. Confirm the ALC897 layout for this exact board revision before enabling audio.
5. Confirm firmware settings, OpenCore configuration, kext compatibility, and
   macOS installer requirements on the exact intended macOS release.

The current target is macOS Tahoe 26. The `MacPro7,1` SMBIOS and Zen 4 AMD
patches are a starting point only; they do not make the unsupported RX 7600
functional. This folder is separate from any existing boot EFI and has not
modified a disk, USB device, or firmware setting.

## Rebuilding this staging folder

`prepare_efi.py` assembles the local EFI from the release archives under
`_downloads/` and reads this computer's values from `smbios.local.json`. It
refuses to overwrite an existing `EFI/` tree. Keep `smbios.local.json` private;
the source release archives and AMD patch plist are retained for provenance.

The Cobalt64 RX 7600 kext is separate: it is a read-only PCI preflight survey
prototype and is not included in this OpenCore config. It has not been built
because this Windows host does not have Xcode or MacKernelSDK.
