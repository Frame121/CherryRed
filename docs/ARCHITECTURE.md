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

So GC is 11.0.3, not 11.0.1. The decoder labels a few hardware IDs as `hwNN` because its name table is a subset of `soc15_hw_ip.h`.

**Still to check:** firmware file names for these versions in linux-firmware.

PSP initialises before display and graphics: the DMUB firmware used by the display engine is loaded through it.

## Milestone M3 (native framebuffer) in practice

1. Map BARs, read IP discovery (M1).
2. Compatibility layer sufficient to run the needed amdgpu code (M2).
3. PSP bring-up, SMU bring-up, load DMUB firmware.
4. Port DC for DCN 3.2, set a mode on one connector.
5. `IOFramebuffer` subclass publishing the display to WindowServer.
