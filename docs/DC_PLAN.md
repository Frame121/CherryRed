# Plan: from a running DMCUB to a picture (DC part of M3)

State on 2026-10-28: the PSP is up, the DMCUB runs our firmware and answers inbox commands (see `PSP_BRINGUP.md`, `DMUB_BRINGUP.md`). Nothing drives the display pipeline yet. This document is the survey and the plan for that part. It is based on reading the Linux amdgpu display code (MIT); it has not been run. Items marked *(unverified)* are my reading, not something tested.

## What the firmware told us

`DMUB_CMD__QUERY_FEATURE_CAPS` returned (bytes of `struct dmub_feature_caps`): PSR version 2, FW-assisted mclk switch version 1, SubVP PSR supported, GECC off, Replay not supported, ABM aux backlight supported, LSDMA in DMU not supported. Nothing here blocks a plain mode set.

## What "a picture" needs, as DC does it

Linux does a mode set through `dc_commit_streams` -> `dc_commit_state_no_check` -> `hwss.apply_ctx_to_hw`. For DCN 3.2 the hardware sequencer table (`hwss/dcn32/dcn32_init.c`) has about 100 operations; a single-stream, single-plane mode set touches a small part of them. Resource pool construction (`resource/dcn32/dcn32_resource.c`) instantiates: HUBBUB, HUBPs, DPPs, MPC, OPPs, OPTCs (timing generators), DSCs, stream and link encoders (DIO and HPO), DCCG, clock manager, clock sources, AUX/I2C engines, audio, panel control, writeback.

Rough size of the files that matter most (lines, as fetched; the rest of DC, and DML2 which does the bandwidth maths, were **not measured**):

| File | Lines |
|---|---|
| `core/dc.c` | 8400 |
| `core/dc_resource.c` | 6336 |
| `hwss/dce110/dce110_hwseq.c` | 3695 |
| `hwss/dcn20/dcn20_hwseq.c` | 3352 |
| `resource/dcn32/dcn32_resource.c` | 3115 |
| `link/link_dpms.c` | 2835 |
| `hwss/dcn32/dcn32_hwseq.c` | 1915 |
| `link/protocols/link_dp_training.c` | 1828 |
| `link/link_detection.c` | 1814 |
| `dio/dcn10/dcn10_link_encoder.c` | 1525 |
| `hwss/dcn30/dcn30_hwseq.c` | 1352 |
| `clk_mgr/dcn32/dcn32_clk_mgr.c` | 1249 |
| `dccg/dcn32/dcn32_dccg.c`, `optc/dcn32/dcn32_optc.c` | 450, 380 |

Dependencies a port has to supply or replace:
1. **Register access.** DC uses `REG_READ`/`REG_UPDATE` macros whose bases come from per-block tables. We would build those from our IP discovery table (done in the probe: `ipBase`). Field shifts and masks come from the `dcn_3_2_0_sh_mask.h` / `_offset.h` headers (MIT, 24 MB for the masks alone).
2. **OS services** (`dm_services.h`: allocation, delays, logging, atomics, IRQ, I2C/AUX helpers). A shim, same idea as the planned LinuxKPI-style layer.
3. **VBIOS parser** (`bios/bios_parser2.c`): connectors, encoders, DDC and HPD lines, clock sources, firmware info. We have the VBIOS image. The command tables the link encoder uses for PHY control may need the **ATOM interpreter** (`amdgpu_atom.c`) *(unverified)*.
4. **Clocks.** `dcn32_clk_mgr` sets DISPCLK, DPPCLK, DCFCLK, DPREFCLK, DTBCLK by messaging the SMU. We have seen `MP1_C2PMSG_66/82/90` alive but have not sent a message. Linux initialises the SMU before the display; the VBIOS already left usable clocks, and our DMCUB started without any SMU work, so a first picture may not need new clocks *(unverified)*.
5. **Bandwidth validation (DML2).** Heavy, and it uses `float`/`double`. Linux wraps it in `kernel_fpu_begin()`. Whether a macOS kext may use SSE/FP safely, and how, needs research *(open risk)*. Options: keep DML out of the kext (compute in a user-space helper), or use fixed, precomputed settings for one known mode.
6. **DP link training and AUX.** AUX goes through hardware AUX engines (or DMUB). EDID is read over I2C-over-AUX or DDC.

## Two ways to get there

| | A: port DC | B: a minimal DCN 3.2 sequence for one fixed mode |
|---|---|---|
| Size | Large (tens of thousands of lines plus register headers and DML) | Small (a few thousand lines) |
| First picture | Late | Early |
| Maintainable, can follow upstream | Yes | No, one hard-coded mode |
| Risk | FP in kernel, build system, OS shim | Easy to get a register order wrong and have no reference except DC's own code |
| Value for contributors | High (they can fix and extend) | A proof of concept |

Recommendation: do B as a **spike** to find out what really has to be programmed and whether the pipe lights up, reusing DC's own register writes as the reference, then grow into A with real information about the hardware. This keeps the "no timelines" promise: each step below is testable and can be handed to a contributor.

## Steps (each testable, in order)

1. **VBIOS objects, read-only** (`tools/`): parse the display object table of the 57856-byte VBIOS we already read. Result: which connectors exist, which encoder/DIG and DDC/HPD line each uses. Linux reported four connectors on this board (DP-2, DP-3, HDMI-A-3, HDMI-A-4); the mapping is what we need. No hardware, can start now.
2. **HPD and EDID, read-only:** with a monitor plugged into one port of the 7800 XT, read the HPD status registers; then read the EDID through the AUX/DDC engine. Needs a monitor on the card (see `TEST_MACHINES.md`); the UHD 630 stays the main display.
3. **Display pipe bring-up without a monitor-visible effect:** DCCG, HUBBUB, DMCUB-assisted init as `dcn32_hw_init` does; confirm by registers.
4. **Test pattern:** program one OTG/timing generator and the display pattern generator (`set_disp_pattern_generator`) on the chosen connector, with the DP link trained. Colour bars on the screen with no framebuffer involved: the first visible result.
5. **One plane:** HUBP -> DPP -> MPC -> OPP -> OPTC reading a scanout buffer in VRAM. A solid colour or a loaded image.
6. **`IOFramebuffer`:** publish that scanout to WindowServer (`getPixelInformation`, `getApertureRange`, `setDisplayMode`, ...), one display mode first, then EDID-driven modes.
7. Grow into A where it pays off (clock management through the SMU, DSC, multiple outputs, hot-plug).

## Questions to settle before step 3

- Does the VBIOS-started DMCUB, or ours, already power-sequence the display block, so that registers read sensibly? (Step 2 shows.)
- FP/SSE inside a macOS kext: allowed state and cost.
- Is the ATOM interpreter needed for PHY power-up on DCN 3.2, or does DMUB (`DMUB_CMD__DPIA`, `DMUB_CMD__ENABLE_DISP_POWER`-style commands) cover it?
- Which output to use first: DP is better documented in DC; HDMI 2.1 FRL needs the HPO encoders.
