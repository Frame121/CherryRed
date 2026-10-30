# CherryRed macOS diagnostics

These utilities collect read-only host evidence. They do not implement or load a GPU driver.

## Collect macOS GPU visibility

From the repository root on macOS:

```sh
python3 -B tools/diagnostics/collect_macos_gpu_inventory.py --output tools/ipdisc/evidence/macos_gpu_inventory_2026-10-24.json
```

The collector refuses to overwrite an existing output file. It records the macOS version, PCI identity reported by IORegistry, the filtered Graphics/Displays summary, related loaded extension names, IOAccelerator service count, and device count from the public Metal API. It does not store display serial numbers, registry dumps, or kernel-extension UUIDs.

The inventory marks IP Discovery as not collected. It is Linux AMDGPU sysfs data and is not available through this macOS collector. The script does not map BARs, read GPU registers, load or unload extensions, or change EFI or system security.

## Test the inventory parser

```sh
python3 -B tools/diagnostics/test_collect_macos_gpu_inventory.py
```

Tests use synthetic text and JSON fixtures; they do not query hardware.

## Run an actual Metal compute smoke test

From the repository root on macOS with Xcode installed:

```sh
swift tools/diagnostics/macos_metal_compute_smoke.swift
```

This compiles a tiny Metal shader, dispatches four integer increments on every
Metal device exposed to the process, and checks the results. Exit status 0
means the Metal compute/readback path worked for the listed device(s). Exit
status 2 means macOS exposed no Metal devices; exit status 1 means a device
was listed but shader setup or execution failed. A passing test proves only
the Metal path for the listed device; it does not identify the physical GPU
unless its registry entry is correlated with PCI evidence. It cannot create,
install, or load a graphics driver.


## Audit Apple AMD driver PCI support

```sh
python3 -B tools/diagnostics/audit_apple_amd_gpu_support.py
```

This read-only audit parses the installed X6000 accelerator, framebuffer, and
HW-services Info.plists and checks whether their PCI match personalities
include RX 7600 `1002:7480`. It also reports Metal device enumeration,
IOAccelerator service presence, and loaded AMD-related extensions. It does
not modify system files, activate extensions, write registers, or reset the
GPU. A missing PCI match is not fixed by spoofing an ID; a real Navi 33 driver
implementation is still required.
