# Architecture

## Target stack

```
Metal app -> Metal plugin -> translator (AIR -> SPIR-V) -> RADV (Mesa Vulkan + ACO) -> libdrm_amdgpu shim -> user client -> kext (amdgpu: DC, PSP, SMU, GFX11, MES, GPUVM)
```

## What we learned from NullMoth (NVIDIA)

Read from their repository (`docs/HOW-IT-WORKS.md`, `plugin/`, `nvk/nvk-macos.patch`, `kexts/`):

- The Metal plugin does **not** use the standard Vulkan API. It calls a private `nvmtl_vk_*` layer that uses NVK internal types (`nvk_buffer`, `nvk_image`, `nvk_cmdbuf`, `nvk_pipeline`) and exposes Metal-specific features (bindless, tessellation, ray tracing). So for AMD the plugin has to be rewritten on top of RADV; it cannot be swapped in.
- The shader translator (`translator/`, LGPL-3.0, based on metal2vulkan) is GPU independent and can be reused as a component.
- NVK reaches the kernel through its `nvkmd` abstraction; their patch disables the nouveau/libdrm path and adds an `nvrm` backend that talks to the kext over IOKit. RADV's equivalent seam is the `amdgpu` winsys (`src/amd/vulkan/winsys/amdgpu`), which sits on libdrm_amdgpu. To be confirmed by reading the real call surface.
- Kernel side: the vendor driver runs inside a kext through an OS layer (`os-xnu*.cpp`). Separate kexts provide the framebuffer (`IOFramebuffer`), the accelerator (`IOAccelerator`, shared surfaces for WindowServer) and the AGDC policy. They load from `/Library/Extensions` via the Auxiliary Kernel Collection, not from OpenCore.

## Hardware facts

Confirmed: RX 7800 XT is Navi 32, gfx1101, PCI `1002:747E`. Linux handles it through the generic IP-discovery path, not a dedicated ID entry.

IP block families (from `amdgpu_discovery.c`): GC 11.0.x, MES 11, SDMA 6.x, PSP 13.0.x, SMU 13.0.x, DCN 3.2.x, VCN 4.0.x.

**Measured on the card (M1 probe, 2026-10-27, Sapphire RX 7800 XT, macOS 15.8.2).** The IP discovery table is binary version 1.2, 1556 bytes, checksum OK, read from VRAM at `VRAM size - 64 KB`. Raw dump: `docs/logs/ioreg-rednavi-747e.txt`; decode it with `tools/decode_discovery.py`.

| Block | Version |
|---|---|
| GC | 11.0.3 |
| SDMA0, SDMA1 | 6.0.3 |
| MP0 (PSP), MP1 (SMU) | 13.0.10 |
| DMU (DCN) | 3.2.0 |
| NBIF | 4.3.0 |
| OSSSYS (IH) | 6.0.0 |
| MMHUB | 3.0.0 |
| HDP | 6.0.0 |
| VCN | 4.0.0, 4.0.1 |
| SMUIO | 13.0.6 |
| PCIE | 7.4.0 |

So GC is 11.0.3, not 11.0.1.

## Firmware for this card

File names come from the `MODULE_FIRMWARE` lines in the Linux driver and were checked against `WHENCE` in linux-firmware. All of them exist there with the licence "Redistributable. See LICENSE.amdgpu". The licence text itself still has to be read before we decide whether the repo may ship or mirror the files; until then they are fetched at runtime.

| Block | File in `amdgpu/` |
|---|---|
| PSP | `psp_13_0_10_sos.bin`, `psp_13_0_10_ta.bin` |
| SMU | `smu_13_0_10.bin` |
| Display (DMCUB, DCN 3.2.0) | `dcn_3_2_0_dmcub.bin` |
| GC | `gc_11_0_3_pfp.bin`, `_me.bin`, `_mec.bin`, `_rlc.bin`, `_imu.bin` |
| MES | `gc_11_0_3_mes_2.bin`, `gc_11_0_3_mes1.bin` (`gc_11_0_3_mes.bin` is also declared) |
| SDMA | `sdma_6_0_3.bin` |
| VCN | `vcn_4_0_0.bin` |

For M3 (native framebuffer) only the PSP and DMCUB files matter, plus the SMU file if the clocks must be brought up first. The DMCUB name is inferred from the DMU version 3.2.0; it is not confirmed by reading `amdgpu_dm.c`, which could not be fetched.

## Probe findings that shape M2

- The register window works: `MM_INDEX`/`MM_DATA` reads of VRAM return valid data, and `RCC_CONFIG_MEMSIZE` gave a size that placed the table correctly. M2 can read VRAM the same way before any aperture mapping exists.
- The IP table lists base addresses per block (for example GC `0x1260`, `0xa000`, `0x1c000`, `0x2402c00`; DMU `0x12`, `0xc0`, `0x34c0`, `0x9000`, `0x2403c00`). The compatibility layer can build its register-offset tables from these instead of hard-coding them.
- DF shows 5 instances and several blocks (CLKA, FUSE, THM, UMC) show instances 1 to 4 with other versions. These are most likely the four memory-cache dies (MCDs) of Navi 32 next to the graphics die; this is inferred from the four matching base-address groups, not checked against AMD documentation.

- **PSP and SMU state after the VBIOS (probe stage 2, 2026-10-28, raw dump in `docs/logs/ioreg-rednavi-probe2.txt`).** `MP0_C2PMSG_33` and `MP0_C2PMSG_35` read `0x80000000` (IFWI init done, bootloader ready for commands). `MP0_C2PMSG_81` is 0, so no PSP sOS is running yet: M2 has to load `psp_13_0_10_sos.bin` through the bootloader. `MP1_C2PMSG_90` is 1, so the SMU is alive and answered its last message OK; its message and argument registers are 0. All other PSP ring registers are 0. The ioreg values print as `18446744071562067968` because the probe stores them as sign-extended 32-bit numbers; that is `0x80000000`.

PSP initialises before display and graphics: the DMUB firmware used by the display engine is loaded through it.

## Milestone M3 (native framebuffer) in practice

1. Map BARs, read IP discovery (M1).
2. Compatibility layer sufficient to run the needed amdgpu code (M2).
3. PSP bring-up, SMU bring-up, load DMUB firmware.
4. Port DC for DCN 3.2, set a mode on one connector.
5. `IOFramebuffer` subclass publishing the display to WindowServer.
