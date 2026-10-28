# Navi33Survey

**Status: first macOS-side bring-up component; not a functional graphics driver.** This opt-in, read-only IOKit service targets the evidenced AMD Radeon RX 7600 board (`1002:7480`, subsystem `148c:2421`). It publishes PCI identity and OS-assigned BAR metadata as `Navi33Survey,*` IORegistry properties, and reports PCIe link and Resizable BAR status. It does not map BARs, access GPU registers, load firmware, initialize the GPU, drive display output, or enable acceleration/Metal. The runtime match is narrower than the plist's vendor/device personality: `probe` and `start` also verify the captured subsystem IDs and refuse other RX 7600 board variants.

## Build prerequisites

Build only on a macOS host with Xcode Command Line Tools and a compatible, legally obtained MacKernelSDK containing `Headers/IOKit/IOService.h` and x86_64 `libkmodc++.a`/`libkmod.a`. This repository checkout does not contain that SDK. Linux host builds are not kext builds.

```sh
make -C src/navi33-survey test
make -C src/navi33-survey all MKSDK=/absolute/path/to/MacKernelSDK
```

The build target intentionally fails early if the SDK or required libraries are absent. It targets x86_64 / macOS 11 kernel ABI and must be reviewed against the exact target macOS version before use. A successful link alone does not prove that the kernel will load the bundle.

## Safety and validation

The service requires the exact boot argument `navi33survey=1` and accepts only `1002:7480`, subsystem `148c:2421`. Run static tests with `make -C src/navi33-survey test`; these tests do not compile or load a kext. Do not install or load an unvalidated bundle on the daily system. Validate first on a disposable macOS VM; note that the current QEMU VM exposes `vmware-svga`, not the physical RX 7600, so it cannot validate RX 7600 hardware behaviour.

On a successful start, IORegistry exposes the PCI identity fields and one address/length/register tuple for each BAR macOS reports, plus PCIe link and Resizable BAR capability data. It marks zero-length or overflowing address ranges invalid and treats an empty BAR inventory as incomplete. These are host-assigned resource facts only. The service does not explicitly open the PCI provider, change PCI command bits, or map the resources.

## Not implemented

- RX 7600 GPU initialization, firmware and memory management
- Interrupts, command submission, VRAM management
- Display engine / scanout / HDMI/DisplayPort output
- Hardware acceleration, OpenGL/Metal integration

These require separate implementation and hardware-specific evidence. This project must not label the survey bundle as a completed RX 7600 driver.
