# macOS GPU Driver Reference Architecture

Reviewed: 2026-10-24

This is an architecture comparison for CherryRed's RX 7600 research. It is not a driver implementation or a support claim.

## The link points to two different projects

The `nullmoth/1401` project is a macOS installer/setup tool. Its README points to the separate `nullmoth/nvidia-macos-driver` repository for the actual NVIDIA driver.

The NVIDIA driver README describes a complete, multi-layer stack:

```text
Metal / WindowServer
  -> NVMTLDriver.bundle
  -> Apple AIR to SPIR-V translation
  -> Mesa NVK / NAK
  -> NVIDIA open GPU kernel modules ported to XNU
  -> NVIDIA GSP firmware
```

Display support has additional framebuffer, accelerator, and graphics-policy kexts. The project says it supports NVIDIA Turing and newer, and reports its main end-to-end testing on an RTX 5060. The code and firmware are NVIDIA-specific: NVK, NVRM, NVIDIA GSP, and the NVIDIA PCI support table cannot bind or initialize AMD device `1002:7480`.

The useful lesson for CherryRed is the separation of responsibilities and the need to validate every layer. It is not a source of an AMD driver. No NVIDIA source code was copied into CherryRed.

## Suggested NootRX reference

NootRX was excluded from implementation research because its repository license prohibits AI use of the work. No NootRX source or implementation details were inspected, copied, or used in CherryRed.

## The closer AMD reference

The archived `lemonade-sdk/mac-amdgpu` project is a more relevant AMD example. Its README describes a DriverKit PCI driver and userspace HSA compute runtime, validated with Radeon AI PRO R9700 (`gfx1201`) on Apple Silicon over Thunderbolt 5. It explicitly describes Metal/display integration and Mesa/Vulkan as separate future work.

That project is not a drop-in for this machine: this host is x86_64 with an RX 7600/Navi 33 (`1002:7480`), while its reported hardware validation is Apple Silicon plus an R9700. Its documented development setup also requires DriverKit entitlements and SIP disabled. Those system changes are outside the current CherryRed task constraints, so the installer/activation procedure is not being run.

## Apple's public PCI driver route

Apple documents PCIDriverKit for custom PCI drivers on both Intel and Apple Silicon Macs. A DriverKit service receives an `IOPCIDevice` provider and can access PCI configuration space and device MMIO. This confirms a public low-level PCI route exists on the host architecture; it does not by itself provide a GPU driver.

Inference from the documented API scope: PCI configuration/MMIO access alone does not implement Navi 33 firmware startup, GPU memory management, queues and command submission, display integration, IOAccelerator services, or a Metal device. Those layers still need an AMD GPU implementation and validation. DriverKit also requires Apple-issued DriverKit and PCI transport entitlements, plus packaging and activation as a System Extension.

The connected Mac now has Xcode 26.5 and the DriverKit 25.5 SDK. `src/navi33-research/driverkit-pci-probe` is a build-only PCI identity stub: Apple IIG generates its interface, Apple Clang compiles both x86_64 DriverKit translation units, and `plutil` validates the match personality for PCI `1002:7480` / subsystem `148c:2421`. The build produces an unsigned `.dext` bundle, but no host app is created and it is not signed, installed, activated, or run against the GPU. The current source opens the matching PCI provider and queries BAR index, type, and size metadata, then closes the provider. It does not map BAR apertures or read/write GPU MMIO registers. An installable extension still requires a host app, Apple-approved DriverKit and PCI transport entitlements, signing, and explicit activation; even then, this probe would not provide graphics acceleration.

## CherryRed's current boundary

- `src/navi48-bringup` is the existing AMD kernel-driver research line, but its personality and firmware requirements target Navi 48 / GFX12 (PCI IDs `1002:7550` and `1002:7551`).
- `src/navi33-research` contains a portable identity/evidence model, lifecycle mock, and the build-only DriverKit PCI identity stub. It has no production macOS GPU backend.
- `src/navi33-survey` is a read-only PCI survey scaffold; it does not initialize the RX 7600.
- The 2026-10-24 post-Xcode inventory at `tools/ipdisc/evidence/macos_gpu_inventory_2026-10-24_post_xcode.json` still records PCI `1002:7480` but zero IOAccelerator services, zero Metal devices, and 7 MB reported VRAM.

Adding the RX 7600 ID to the Navi 48 path would therefore be an unsafe generation mismatch, not a driver port.

## What transfers into an AMD/Navi 33 plan

| Layer | NVIDIA reference | CherryRed RX 7600 work |
|---|---|---|
| Device and ASIC identification | NVIDIA PCI table and chip support gates | Keep `1002:7480` on a separate Navi 33 route; bind claims to captured evidence |
| Kernel transport and initialization | XNU port of NVIDIA's open resource manager | Implement or port an AMD-specific transport/backend; prove PCI ownership and firmware bring-up before exposing services |
| Firmware | NVIDIA GSP | Navi 33's own AMD PSP, SMU, GC, SDMA, and scheduler firmware; record exact provenance and compatibility |
| Memory, queues, and command submission | NVK/NVRM | AMD GFX11 memory-management, queue, synchronization, reset, and power paths |
| Metal and display | NVIDIA Metal plugin plus framebuffer/accelerator kexts | Separate AMD Metal/user-space and display/compositor integration; neither is present in the current Navi 33 prototype |
| Validation | Host regression tests plus per-card boot/render evidence | Keep host tests separate from target-card initialization, Metal enumeration, rendering, and stability evidence |

The NVIDIA project shows that a third-party macOS GPU stack can be built when its kernel, firmware, user-space, and display layers are all implemented. The AMD DriverKit project shows an additional compute-only route on a different platform. Neither example removes the RX 7600-specific work.

## Safe next work on this Mac

The first platform-bound prototype now compiles against Apple's DriverKit 25.5 SDK, but it does not attach to the card. Further work still needs verified AMD/Navi 33 initialization and firmware evidence, an app host, Apple-approved entitlements, and an independently safe activation/rollback plan before hardware validation. Keep the PCI/Metal inventory and distinguish generic GFX11 hypotheses from board-verified facts. Do not copy the RX 7600 ID into the Navi 48 driver or describe a PCI match as acceleration.

A working RX 7600 Metal device still requires the AMD/Navi 33 equivalents of the transport, firmware initialization, memory and queue services, Metal integration, and target-hardware validation. The current boot does not expose the device to Metal. This note does not change EFI, install or load a kext/system extension, or change system security.

## References

- [1401 installer project](https://github.com/nullmoth/1401)
- [NullMoth NVIDIA macOS driver](https://github.com/nullmoth/nvidia-macos-driver)
- [NullMoth driver architecture](https://github.com/nullmoth/nvidia-macos-driver/blob/main/docs/HOW-IT-WORKS.md)
- [NullMoth card support and validation](https://github.com/nullmoth/nvidia-macos-driver/blob/main/docs/CARD-SUPPORT.md)
- [NootRX license (excluded from source use)](https://github.com/ChefKissInc/NootRX/blob/master/LICENSE.md)
- [Apple PCIDriverKit](https://developer.apple.com/documentation/pcidriverkit)
- [Apple IOPCIDevice](https://developer.apple.com/documentation/pcidriverkit/iopcidevice)
- [Apple DriverKit entitlement guidance](https://developer.apple.com/system-extensions/)
- [mac-amdgpu](https://github.com/lemonade-sdk/mac-amdgpu)
