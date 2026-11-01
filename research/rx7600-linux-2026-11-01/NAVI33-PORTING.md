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

## Implemented bring-up scaffolding (2026-11-01)

The following work is now present in the `rednavi/contrib-navi33-pci-profile` branch. The readiness labels below describe source/host preparation only; the hardware-validation booleans for the RX 7600 macOS path remain false.

### GC register map

- `RedNavi/gc_regs_navi33.h` is generated from Linux `gc_11_0_0_offset.h`, `gc_11_0_0_sh_mask.h` and `gc_11_0_0_default.h` at upstream commit `cbbddb6b08cc9900a87cccaa9b22bbe06aa4f50a`.
- The generated map is distinct from the existing Navi 32 GC 11.0.3 map. Its L2 CNTL3 initialization default is `0x80120007` (GC 11.0.0 family); the Navi 32 GC 11.0.3 path retains its `0x80100007` special case.
- `gfx11.cpp` can be compiled into separate Navi 32 or Navi 33 symbol sets. GC/RLC/CP/SDMA register accesses in the probe now use the selected register definitions and IP-discovery base segments instead of several Navi 32 absolute addresses.
- Host tests check 17 GC offset/mask/default fields. This is a source-map verification, not a macOS register readback from the RX 7600.
- Regenerate the tracked map with `bash tools/build_navi33_gc_regs.sh`.

### Firmware wrappers and board profile

The local generator `tools/build_navi33_firmware_headers.sh` decompresses temporary inputs from the installed `linux-firmware/amdgpu` directory and runs the existing header generators with these exact RX 7600 families:

| Wrapper | Firmware input family | Header/runtime value verified |
|---|---|---|
| `psp_fw_navi33.h` | `psp_13_0_7_sos.bin` | SOS `0x00330024` |
| `smu_fw_navi33.h` | `smu_13_0_7.bin` | SMU `0x00526100` (82.97.0) |
| `gc_fw_navi33.h` | `gc_11_0_2_*` + `sdma_6_0_2.bin` | GC CP/RLC/IMU/MES and SDMA 6.0.2; 26 manifest entries |
| `dmub_fw_navi33.h` | `dcn_3_2_1_dmcub.bin` | DMCUB `0x07003600`; VBIOS window length 57,856 bytes |

`tools/verify_navi33_firmware_headers.py` checks the generated wrapper metadata and required manifest entries. It passed on the installed RX 7600 firmware set. **This verifies the local generated headers only.** It does not load firmware into the PSP, boot SMU/GC/SDMA, or prove that any macOS firmware sequence works.

The PSP 13.0.7 `ta.bin` package is not currently wrapped or loaded; RAS/HDCP and other Trusted Application services are outside this first GPU-core bring-up path. The secure OS, firmware TOC/TMR, SMU, GC/SDMA, and DMUB payloads are the current scope.

The generated `*_navi33.h` headers embed AMD firmware payloads. They are ignored by Git, must remain local to the build, and must not be committed or redistributed. Regenerate them with `bash tools/build_navi33_firmware_headers.sh` on a Linux build machine with `zstd` and the matching `linux-firmware-amdgpu` files installed.

`RedNavi/navi33_board.h` records this specific RX 7600 board's observed facts: PCI `1002:7480`, 8 GiB VRAM, MC base `0x8000000000`, VBIOS length 57,856 bytes, PSP TMR VRAM offset `0x1fc000000` and size `0x01300000`. The TMR address/size are board-capture values, not universal Navi 33 constants. The code checks that the TMR region fits the BAR0 aperture and refuses `SETUP_TMR` if the loaded TOC reports a different size.

### Isolated build and safety gates

- `make NAVI33=1` selects `build/<Config>-Navi33`, defines `RN_BUILD_NAVI33`, compiles the Navi 33 `gfx11.cpp` object against `gc_regs_navi33.h`, and selects only the Navi33-specific generated firmware wrapper names.
- The default `NAVI33=0` build remains the original Navi 32 path. A build-target/PCI identity mismatch returns to the read-only path.
- The Navi 33 experimental build does not execute GPU write stages unless the master boot argument `rednavi_navi33_enable=1` is present. Existing per-stage arguments remain required. GFX/SDMA additionally requires `rednavi_navi33_gfx=1`; DP setup/training additionally requires `rednavi_navi33_display=1` and the existing DP stage arguments.
- Navi 32 `dcn_replay` and unsafe full DCN dump are skipped in the Navi 33 build. Navi 33 framebuffer publication is disabled pending a Navi 33-specific DCN pipe capture and validated scanout path.
- Do not enable any of these boot arguments on a daily-use or recovery-dependent machine. The kernel probe writes to real hardware and can hang the machine; a bootable backup EFI and an independent display/recovery route are required.

### Current validation results

| Area | Source/host preparation | RX 7600 macOS hardware validation |
|---|---|---|
| GC register offsets | `HOST_TESTED` (17 offset/mask/default checks); separate Navi 33 GFX object compiles | Not validated |
| Firmware preparation | `HOST_TESTED` (local PSP/SMU/GC/SDMA/DMUB wrappers and version checks) | Not validated; no N33 firmware was loaded from macOS |
| Display initialization | `SOURCE_MAPPED` (IP version, DMUB family and Linux display path recorded) | Not validated; no Navi 33 macOS DP link or scanout test |
| Graphics submission | `HOST_TESTED` (GFX11 ring packet encoding and source compile) | Not validated; no N33 SDMA/GFX packet was submitted on macOS |

Cross-target syntax checks using the cloned MacKernelSDK and `x86_64-apple-macosx12` target passed for `kern_probe.cpp` with Navi 33 firmware headers, Navi 33 and Navi 32 `gfx11.cpp`, `smu13.cpp`, and all six compatibility source units. `kern_probe.cpp`, Navi 33 `gfx11.cpp`, `smu13.cpp` and all six compatibility units also emitted Mach-O kernel-target objects with `-mkernel`. This is not a full kext link or a hardware test. The MacKernelSDK directory and the generated firmware wrappers remain local/ignored.
