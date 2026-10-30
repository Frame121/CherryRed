# navi33-survey

Read-only PCI survey kext for the captured AMD Radeon RX 7600 board (Navi 33, RDNA 3, PCI `1002:7480`, subsystem `148c:2421`).

**Status: build and link validated on the connected x86_64 macOS host; never loaded or tested against hardware.** This remains a read-only PCI survey kext, not a graphics driver.

## What it does
- Matches only PCI `1002:7480` / subsystem `148c:2421`, and only when the boot-arg `navi33survey=1` is set.
- Logs (via `IOLog`): PCI ids/revision/subsystem, BAR physical addresses and sizes,
  PCIe link speed/width, Resizable BAR capability and current size.

## What it does NOT do
- No register BAR mapping, no MMIO reads or writes, no VRAM access, no firmware, no display.
- It does not initialise the GPU and gives no acceleration.

## Build check (build only)

The build uses Xcode and the public [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK).
From the repository root, place that dependency under the ignored `build/deps` directory
and run:

```sh
git clone --depth 1 https://github.com/acidanthera/MacKernelSDK.git build/deps/MacKernelSDK
make -C src/navi33-survey
```

The ad-hoc signed bundle is written to `build/navi33-survey/Navi33Survey.kext`.
The Makefile only compiles, links, and validates the bundle. It does not install,
activate, or load the kext, edit EFI, or change security settings.

The service is opt-in with `navi33survey=1`. It reads PCI identity/configuration
and assigned BAR metadata, then logs PCIe link and ReBAR information. It does not
map BARs, read/write GPU registers, load firmware, initialize the GPU, create
Metal/IOAccelerator services, or provide graphics acceleration. A successful
build is not hardware validation.
