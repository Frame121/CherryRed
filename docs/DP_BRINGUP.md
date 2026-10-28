# DisplayPort bring-up on path 0 (steps 3 and 4 of `DC_PLAN.md`)

Goal: a 1920x1080@60 test pattern on the Samsung monitor attached to VBIOS path 0 (DisplayPort, UNIPHY2, AUX4, HPD4). This note records what was read in the Linux display code (MIT) and what has been tried on the card. Nothing past step "transmitter INIT" is implemented yet.

## Key finding: the VBIOS command tables can be run by our DMCUB

DC does not need an ATOM interpreter for the PHY. When `dc->debug.dmub_command_table` is set, `dc/bios/command_table2.c` wraps the VBIOS command-table parameters in a `DMUB_CMD__VBIOS` command (type 128) and the DMCUB runs the table itself (it has the VBIOS in window 3). The sub-types that matter, with the 64-byte layout (header 4 bytes, then the ATOM parameter struct):

| Sub-type | Value | Parameter struct (size) |
|---|---|---|
| `DIGX_ENCODER_CONTROL` | 0 | `dig_encoder_stream_setup_parameters_v1_5` (12): `digid` +0, `action` +1, `digmode` +2, `lanenum` +3, `pclk_10khz` +4, `bitpercolor` +8 |
| `DIG1_TRANSMITTER_CONTROL` | 1 | `dig_transmitter_control_parameters_v1_6` (16): `phyid` +0, `action` +1, `digmode`/`dplaneset` +2, `lanenum` +3, `symclk_10khz` +4, `hpdsel` +8, `digfe_sel` +9, `connobj_id` +10 |
| `SET_PIXEL_CLOCK` | 2 | `set_pixel_clock_parameter_v1_7` (16) |
| `ENABLE_DISP_POWER_GATING` | 3 | small |

These sizes and offsets were measured by compiling `dmub_cmd.h` and `atomfirmware.h` against stub headers.

## Mapping for path 0

From the VBIOS parser (`docs/logs/vbios-display-objects.txt`) and DC's helpers: connector DisplayPort (object id 19), encoder UNIPHY2 enum 1 = transmitter **E** = ATOM phy id **4**, HPD pin 5 = hpd select 5, AUX engine 4, DDC id 0x94. The other paths: UNIPHY enum 1 = A (0), UNIPHY enum 2 = B (1), UNIPHY1 enum 2 = D (3). This mapping is my reading of `translate_encoder_to_transmitter`; it is not yet confirmed on the other ports.

## Sequence DC uses for a DP stream (from `link_dpms.c`, `dcn10_link_encoder.c`, the DCN32 hardware sequencer)

1. Clocks: DISPCLK/DPPCLK/DCFCLK through the clock manager (SMU), pixel clock through the pixel-clock command table or the DTO.
2. Transmitter: `DIG1_TRANSMITTER_CONTROL` action `INIT` (hw_init), then `ENABLE` with lanes and symbol clock.
3. Link training: DPCD 0x100/0x101 (rate, lanes), training pattern in DPCD 0x102 plus the source pattern through `DIGX_ENCODER_CONTROL` actions `DP_LINK_TRAINING_PATTERN1/2/3`, drive settings through `TRANSMITTER_CONTROL` `SETUP_VSEMPH` and DPCD 0x103 to 0x106, status in DPCD 0x202 to 0x204, then `LINK_TRAINING_COMPLETE`.
4. Stream encoder: `DIGX_ENCODER_CONTROL` `STREAM_SETUP` / `ENABLE_DIG`, DP MSA and video stream registers.
5. Timing generator: OPTC `OTG_H_TOTAL` and friends from the EDID detailed timing, then `OTG_CONTROL` enable.
6. Test pattern: OPP display pattern generator (`opp2_set_disp_pattern_generator`: `DPG_CONTROL`, `DPG_COLOUR_*`, `DPG_DIMENSIONS`), fed to the OPTC.
7. Unblank the stream.

Steps 1, 3 (the register side), 5 and 6 are direct DCN register programming and are the part that cannot be delegated to the firmware.

## Done on the card (2026-10-28)

- `rednavi_dp_init=1`: a `DMUB_CMD__VBIOS` / `DIG1_TRANSMITTER_CONTROL` command with action INIT for path 0 (phy id 4, DP, 4 lanes, HPD 5, connector id 19). The DMCUB **consumed it** (inbox read pointer caught up), the reply slot holds the command as sent, and the AUX channel, HPD sense and EDID read still work afterwards. There is no visible side effect yet; INIT is a setup action. This proves the DMUB-VBIOS path and the plumbing (`dmubSend`) only.

## Next sub-steps, each checkable without a picture

1. Pixel clock and transmitter ENABLE (4 lanes, HBR2 symbol clock), check that HPD and AUX stay up.
2. **Link training**: DPCD writes over AUX plus the source pattern and drive settings via DMUB; success is `CR_DONE`, `CHANNEL_EQ_DONE`, `SYMBOL_LOCKED` and inter-lane align in DPCD 0x202 to 0x204. This is a verifiable result of its own.
3. Stream setup, OTG timing, DPG pattern, unblank: the picture.

Open risks: the exact clock state needed for the transmitter and DPG (the clock manager and SMU are not driven yet), the DISPCLK/DPPCLK the VBIOS left, and whether the OPP/OPTC clocks are enabled without the DCCG sequence.
