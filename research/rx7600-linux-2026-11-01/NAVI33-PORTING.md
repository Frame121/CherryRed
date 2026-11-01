# Navi 33 bring-up and Linux-to-macOS porting map

Status: research/profile layer only. This document does not claim RX 7600 macOS acceleration.

## Hardware initialization model

Linux `amdgpu` is the reference implementation for the hardware sequence. CherryRed is a separate XNU kext and must provide equivalent OS services; it cannot directly link Linux driver objects. Keep hardware facts, Linux reference behavior, and macOS validation as separate evidence classes.

| Initialization area | Linux reference area | CherryRed area | RX 7600 evidence / remaining work |
|---|---|---|---|
| PCI discovery and BAR mapping | `amdgpu_pci`, `amdgpu_device` | `kern_probe.cpp`, `rn_device` | PCI `1002:7480`, BARs and IP discovery observed on Linux; macOS RX7600 not yet validated |
| IP block enumeration | `amdgpu_discovery` | `tools/decode_discovery.py`, `gpu_ip_profile.h` | GC 11.0.2, SDMA 6.0.2, PSP/SMU 13.0.7, DCN 3.2.1 observed |
| VBIOS and PSP | `amdgpu_bios`, `psp_v13_0` | probe PSP stages, firmware registry | Linux VBIOS/firmware state recorded; do not reuse Navi 32 PSP sequence without validating 13.0.7 |
| SMU and clocks | `smu_v13_0` | `smu13.cpp`, compat layer | Navi 33 SMU is 13.0.7; message set, firmware boot and clock policy remain unvalidated on macOS |
| Memory/GPUVM | `gmc_v11_0`, `gfxhub_v3_0` | `gfx11.cpp`, `rn_device` | RX7600 memory facts captured; Navi 33 uses GC 11.0.2/gfxhub_v3_0, not the Navi32 11.0.3 special case. Audit register headers and page-table assumptions before writes |
| Graphics engine | `gfx_v11_0`, `mes_v11_0` | `gfx11.cpp`, future MES/queue code | Firmware/IP versions identified; no RX7600 graphics-ring test on macOS |
| SDMA | `sdma_v6_0` | `gfx11.cpp` SDMA path | SDMA 6.0.2; Navi32 SDMA 6.0.3 is not a safe substitute |
| Display | `amdgpu_dm`, DCN 3.2 | `RedNaviFB`, DCN replay | DCN 3.2.1 and Linux display evidence; macOS path/connector/firmware sequence not validated |
| Userspace submission | libdrm `amdgpu` winsys | planned libdrm shim | Requires BO/VA/fence/doorbell/user-client contracts; not implemented for Navi33 |
| Vulkan and Metal | RADV/ACO then Metal translation layer | planned userspace stack | No Navi33 Vulkan or Metal acceleration |

## Initialization dependency order

1. Match PCI identity; map BARs and establish safe read-only access.
2. Parse IP discovery and build per-IP base-address/version tables. Never infer offsets solely from the marketing model.
3. Read/validate VBIOS and derive board-specific memory and connector facts.
4. Establish PSP boot chain and load only the exact firmware family declared for the detected IP versions.
5. Bring up SMU and required clocks/power domains.
6. Initialize memory controller, GPUVM/GART, IH and required synchronization primitives.
7. Initialize GC/RLC/CP, SDMA and MES; validate a bounded hardware command before exposing user submission.
8. Initialize DCN/DMUB and display path independently of 3D acceleration.
9. Expose kernel/user-client buffer, VA, queue, fence and reset contracts; only then attempt RADV.
10. Implement Metal translation and WindowServer acceleration as separate milestones.

The order is a dependency model, not a claim that all blocks must always be initialized in exactly this sequence. Display firmware and graphics firmware dependencies differ by ASIC and Linux driver revision.

## Navi 33 vs current Navi 32 path

The RX 7600 capture identifies GC 11.0.2, SDMA 6.0.2, PSP/SMU 13.0.7, DCN 3.2.1, NBIO 4.3.1, MMHUB 3.0.2, OSSSYS 6.0.2 and HDP 6.0.1. Current `gfx11.cpp` and `gc_regs.h` are tied to a Navi 32 GC 11.0.3 register set and contain an explicit 11.0.3 L2 default override. Therefore this profile records versions but intentionally grants no Navi 32 register/firmware authorization to Navi 33.

## macOS driver architecture mapping

- IOPCIDevice/IOService replaces Linux PCI device and driver binding.
- `rn_device` and the compat layer replace the subset of Linux `amdgpu_device`, MMIO, allocation, locking, workqueue and firmware APIs that have been ported.
- `RedNaviProbe` is currently the hardware bring-up boundary; `RedNaviFB` is a separate display-facing IOFramebuffer component.
- A future accelerator/user-client layer must expose memory objects, GPU virtual addresses, command queues, fences, interrupts and lifecycle/reset semantics to userspace. A framebuffer alone is not a Metal device.
- RADV expects libdrm_amdgpu/winsys semantics. The proposed macOS shim must preserve those semantics over a CherryRed user client; simply linking RADV to IOKit is not equivalent.
- Metal requires a separate Metal API implementation/translation layer over the working Vulkan path. Apple's AGX/M1 Metal evidence is useful as an API/behavior reference only, not as AMD register or firmware evidence.

## Validation policy

`GpuIpProfile` is descriptive. Its booleans explicitly record whether IP discovery, register offsets, firmware sequencing, display initialization and graphics submission have been validated on the target. A version match alone never flips a validation gate. Do not enable Navi 33 write paths until each relevant gate is supported by a reproducible RX7600 macOS hardware test.
