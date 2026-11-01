# macOS Apple Driver Match Audit — RX 7600

Date: 2026-10-24  
Host: macOS Tahoe 26.7.1 (build 25G241), x86_64  
GPU: AMD Radeon RX 7600, PCI vendor/device `1002:7480`, revision `0xcf`, subsystem `148c:2421`, PCIe x8

## Result

This boot does not have RX 7600 graphics acceleration. The GPU appears on PCI, but macOS has not attached an accelerator driver or exposed it as a Metal device.

Read-only checks on the connected host found:

- `system_profiler` reports 7 MB VRAM and no loaded GPU kernel extension for the RX 7600.
- `kmutil showloaded` includes Apple `AMDSupport`, plus Lilu and WhateverGreen; this does not mean an RX 7600 accelerator is attached.
- The GPU's IORegistry branch has no `IOAccelerator` service. The Metal device probe returns zero devices.
- The installed Apple AMD driver personalities inspected do not match `0x74801002`:
  - `/System/Library/Extensions/AMDRadeonX6000.kext/Contents/Info.plist` lists Navi 10/12/14/21/23 accelerator personalities, but no Navi 33 / RX 7600 match.
  - `/System/Library/Extensions/AMDRadeonX6000Framebuffer.kext/Contents/Info.plist` lists Navi 10/12/14/21/23 framebuffer personalities, but no RX 7600 match.
  - `/System/Library/Extensions/AMDRadeonX6000HWServices.kext/Contents/Info.plist` has no RX 7600 PCI match.
  - `/System/Library/Extensions/AMD10000Controller.kext/Contents/Info.plist` lists older device IDs and has no `0x74801002` match.

These checks show that the Apple driver set on this installed OS does not bind the RX 7600. The presence of an AMD Metal driver bundle or the generic `AMDSupport` kext alone is not evidence of GPU support.

## What is still needed for a real driver

The project currently has a read-only PCI survey scaffold, not an accelerator. Before a driver implementation can be grounded in this board, CherryRed still needs verified Navi 33/IP discovery and firmware/ASIC evidence collected from a compatible Linux boot with the RX 7600 bound to `amdgpu`. macOS has no `/sys/bus/pci/devices`, so `tools/ipdisc/collect_linux_ip_inventory.py` must be run under Linux. The previous macOS invocation failed for that expected reason.

Even with Linux inventory, actual macOS acceleration would still require a substantial, platform-specific GPU driver and hardware validation; adding a PCI ID to a plist or reporting PCI detection would not provide acceleration. No EFI, kext, system security, or boot configuration was changed during this audit, and no unvalidated driver was loaded.

## Next evidence step

On Linux, with the RX 7600 bound to the kernel `amdgpu` driver, run:

```sh
python3 tools/ipdisc/collect_linux_ip_inventory.py --output /tmp/rx7600-ipd-snapshot
```

Then transfer the snapshot into `tools/ipdisc/evidence/` for review. The collector is read-only and refuses to overwrite an existing output directory.
