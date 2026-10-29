# Navi 48 → Navi 33 port map

Date: 2026-10-25
Purpose: a traceable table of the generation differences between the working Navi 48 (RDNA 4,
RX 9070 XT) stack and the target Navi 33 (RDNA 3, RX 7600), so the port is auditable. The Navi 48
stack is the template; each row names what changes and where.

> Ground truth for the RX 7600's IP versions comes from the project's own captured evidence
> (`firmware-dependency-audit.md`, `firmware-source-correlation.json`, `navi33-firmware-manifest.json`),
> not from generic RDNA 3 defaults.

## Generation differences

| Aspect | Navi 48 (RDNA 4, RX 9070 XT) | Navi 33 (RDNA 3, RX 7600) |
|---|---|---|
| PCI device ID | `1002:7550` / `1002:7551` | `1002:7480` (subsystem `148c:2421`) |
| Graphics IP (GC) | gfx1201 (GC 12.0.1) | **gfx1102 (GC 11.0.2)** |
| Display engine (DCN) | DCN 4.1 | **DCN 3.2.1** |
| SDMA | 7.0.1 | **6.0.2** |
| VCN | (n/a in Navi 48 set) | **4.0.4** |
| PSP | 14.0.3 | **13.0.7** |
| SMU | 14.0.3 | **13.0.7** |
| MES | `gc_12_0_1_uni_mes` | `gc_11_0_2_{mes,mes1,mes_2}` |

## Firmware blob set

| Navi 48 (`src/navi48-bringup/firmware/`) | Navi 33 (`src/navi33-bringup/firmware/`) |
|---|---|
| `gc_12_0_1_{imu,me,mec,pfp,rlc}.bin` | `gc_11_0_2_{imu,me,mec,pfp,rlc}.bin` |
| `gc_12_0_1_uni_mes.bin` | `gc_11_0_2_{mes,mes1,mes_2}.bin` |
| `psp_14_0_3_{sos,ta}.bin` | `psp_13_0_7_{sos,ta}.bin` |
| `sdma_7_0_1.bin` | `sdma_6_0_2.bin` |
| `smu_14_0_3.bin` | `smu_13_0_7.bin` |
| — | `vcn_4_0_4.bin` |
| — | `dcn_3_2_1_dmcub.bin` |

Fetch: `tools/fetch-firmware.sh` (Navi 48) → `tools/fetch-firmware-navi33.sh` (Navi 33).

## Files that encode the generation

| Concern | Navi 48 location | Navi 33 location / change |
|---|---|---|
| Kext match set (Info.plist) | `src/navi48-bringup/Info.plist` (`0x75501002 0x75511002`) | `src/navi33-bringup/Info.plist` (`0x74801002`) — separate kext, never shares IDs |
| Kext build (Makefile) | `src/navi48-bringup/Makefile` | `src/navi33-bringup/Makefile` (mirrors build system, Navi 33 FW set) |
| Firmware embed | `src/navi48-bringup/tools/embed-firmware.py` (shared) | reuses the same tool; Navi 33 blob set |
| Display register layer | `src/dcn41/` (DCN 4.1, generated from `dcn_4_1_0_*`) | `src/dcn321/` (DCN 3.2.1, generated from `dcn_3_2_1_*`) + `tools/dcn321/` |
| GPU bring-up (PSP/SMU/GFX/SDMA/MES) | `src/navi48-bringup/src/amd/` | future Navi 33 bring-up (gfx1102, DCN 3.2.1) — scaffold only for now |
| IP Discovery parsing | `src/navi48-bringup/src/ipdiscovery.cpp` | Navi 33 must be gated on separate format evidence (different IP versions) |
| Metal / Vulkan userspace | `tools/native/` (Metal bundle), `src/xlat12/`, `mesa-patches/` | shared stack; gfx1102 target + DCN 3.2.1 display bring-up are the Navi 33-specific parts |

## What is shared (no change)

- The overall architecture: `Metal apps → Metal plugin → AIR→SPIR-V (metal2vulkan) → Mesa RADV
  (Darwin port) → IOAccelerator → kext → GPU`.
- The MacKernelSDK build system, codesign, and the fail-safe/auto-arm boot pattern.
- The evidence-gated bring-up discipline (staged, each stage PASS/FAIL/SKIP).

## Hard rule

The Navi 48 kext's match IDs (`1002:7550` / `1002:7551`) are never changed to include Navi 33.
Navi 33 is a separate kext (`com.navi33.bringup`) matching only `1002:7480`.
