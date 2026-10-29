# RX 7600 (Navi 33) — DCN 3.2.1 display dependency map

Date: 2026-10-25
Purpose: scope the display-engine port for the macOS bring-up. This card's Display Core
is **DCN 3.2.1** (per the project's own captured evidence — `firmware-dependency-audit.md`
boot journal: "Display Core initialized on DCN 3.2.1"; firmware `dcn_3_2_1_dmcub.bin`).

> NOTE: This is **not** DCN 3.1.4. The generic "RX 7600 = DCN 3.1.4" assumption is wrong for
> this captured board; the card's own evidence is authoritative.

## Confirmed variant in the reference source

The Arch kernel clone (`RX7600-Linux-Evidence/source-investigation/arch-linux-v7.2.11`,
commit `64a90897d`) selects the DCN 3.2.1 resource pool:

- `drivers/gpu/drm/amd/display/dc/core/dc_resource.c:76` — `#include "dcn321/dcn321_resource.h"`
- `dc_resource.c:366` — `res_pool = dcn321_create_resource_pool(init_data, dc);`
- `drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c:138` —
  `#define FIRMWARE_DCN_V3_2_1_DMCUB "amdgpu/dcn_3_2_1_dmcub.bin"`

## Block structure (DCN 3.2.1)

The display core is composed of these blocks (mirrors the `src/dcn41/` organization used for
DCN 4.1, which is the existing in-repo template):

| Block | Role | DCN 3.2.1 source (upstream amdgpu/display) |
|---|---|---|
| OPTC | Output Timing Controller (scanout) | `dc/optc/dcn321/dcn321_optc.c` |
| OTG | Output Timing Generator (timing) | `dc/optc/dcn321/` (otg timing) |
| HUBP | Hub Pipeline (plane/blend) | `dc/hwss/dcn321/` + `dc/inc/hw/hubp.h` |
| DML | Display Mode Library (bandwidth/timing) | `dc/dml/dcn321/dcn321_fpu.{c,h}` |
| DCCG | Display Clock Controller | `dc/dccg/dcn321/dcn321_dccg.c` |
| DIO | Display IO (link/encoder) | `dc/dio/dcn321/dcn321_dio_link_encoder.{c,h}` |
| CLK_MGR | Display clock manager | `dc/clk_mgr/dcn321/` |
| HWSS | Hardware sequencing (init/hwseq) | `dc/hwss/dcn321/dcn321_hwseq.c`, `dcn321_init.c` |
| DMCUB | Display Microcontroller (firmware) | firmware `dcn_3_2_1_dmcub.bin` |

## Locally available vs. must-fetch (in the `blob:none` clone)

The clone is a **partial** (`blob:none`) checkout. For DCN 3.2.1 specifically:

**Materialized locally (present on disk):**
- `dc/dml/dcn321/dcn321_fpu.{c,h}`
- `dc/resource/dcn321/dcn321_resource.{c,h}`
- `dc/dio/dcn321/dcn321_dio_link_encoder.{c,h}`
- 29 shared register headers under `dc/inc/hw/` (hubp, mpc, dpp, dwb, gpio, dmcu, clk_mgr,
  link_encoder, mem_input, abm, audio, aux_engine, dccg, dchubbub, ipp, mcif_wb, …)

**NOT materialized (must be fetched before the display port can be built):**
- The DCN 3.2.1 register-map header `dc/inc/hw/dcn321.h` (block register offsets) — not tracked
  in the clone.
- `dc/hwss/dcn321/` (hwseq/init), `dc/optc/dcn321/`, `dc/clk_mgr/dcn321/`, `dc/dccg/dcn321/`.

Fetch options (Mac-day or on demand):
1. `git -C <clone> checkout <commit> -- drivers/gpu/drm/amd/display/dc/inc/hw/dcn321.h` (materialize
   the specific blobs from the existing clone), or
2. source the DCN 3.2.1 register map + block files from the upstream `amd/amd-staging-dkms` /
   `linux` display tree at a matching commit.

## Init order (bring-up sequence, display portion)

Mirrors the Navi 48 display bring-up (`src/dcn41/`): clock setup (CLK_MGR/DCCG) → link/encoder
(DIO) → hub pipeline (HUBP) → output timing (OPTC/OTG) → DML bandwidth/timing validation →
DMCUB firmware load (`dcn_3_2_1_dmcub.bin`) → scanout enable. Each step is gated and logged;
failure rolls back to the CPU-rendered desktop (fail-safe, per the `autoarm` pattern).

## Evidence limits

- This map is a **port scope**, not proof the display works on macOS.
- DCN 3.2.1 register offsets must come from the (not-yet-materialized) `dcn321.h`; the 29 shared
  headers present locally cover the block-agnostic registers.
- The `dcn_3_2_1_dmcub.bin` firmware is Linux-side evidence; its macOS loadability and licensing
  are separate gates (see `navi33-firmware-manifest.json`).
