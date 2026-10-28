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

## DP link training succeeded (2026-10-28)

`rednavi_dp_train=1` (probe stage 14) enables the transmitter and trains the link on path 0 against the monitor, over AUX4:

1. DPCD: `SET_POWER` D0 (0x600), `LINK_BW_SET` 0x14 (HBR2) and `LANE_COUNT_SET` 4 with enhanced framing (0x100/0x101), no downspread, 8b/10b.
2. Source: `DIG_MODE` = DP, `DP_UDI_LANES` = 3, scrambler advance; **transmitter ENABLE via `DMUB_CMD__VBIOS`** (phy id 4, 4 lanes, symbol clock 540000 kHz, HPD 5) - consumed by the DMCUB.
3. Clock recovery with TPS1 (`DP4_DP_DPHY_TRAINING_PATTERN_SEL` = 0, PHY bypass off, DPCD 0x102 = 0x21), source drive set through `TRANSMITTER_CONTROL` / `SETUP_VSEMPH`, adjusting from the sink's requests (DPCD 0x206/0x207).
4. Channel equalisation with TPS3 (sink supports it), DPCD 0x102 = 0x23.
5. Training pattern off, `DP_LINK_TRAINING_COMPLETE` set.

Result, read from the sink (`docs/logs/ioreg-rednavi-probe16-dp-training.txt`): clock recovery done after 4 tries, equalisation done after 4 tries, **DPCD 0x202/0x203 = 0x77 0x77** (all four lanes `CR_DONE`, `CHANNEL_EQ_DONE`, `SYMBOL_LOCKED`), **0x204 = 0x01** (inter-lane alignment done), 0x205 = 0x01 (receive port in sync). The sink's last drive requests (0x206/0x207) were 0x22 0x22, i.e. voltage swing level 2, pre-emphasis 0.

So a real DisplayPort link at HBR2 x4 comes up from our kext: PHY, DIG back end, AUX and the training handshake are right. There is still no video on it: no stream encoder, no timing generator, no pixel data.

## Next sub-steps, each checkable without a picture

1. Pixel clock for the stream (the PHY PLL is up at the symbol rate; the stream pixel clock is a separate DTO).
2. **Link training [done]**, see above.
3. Stream setup, OTG timing, DPG pattern, unblank: the picture.

Open risks: the exact clock state needed for the transmitter and DPG (the clock manager and SMU are not driven yet), the DISPCLK/DPPCLK the VBIOS left, and whether the OPP/OPTC clocks are enabled without the DCCG sequence.

## Lesson: do not read the whole DCN register list from macOS (2026-10-28)

`rednavi_dcn_dump=1` (probe stage 14) read all 4366 registers of `tools/dcn32_reglist.txt` after link training. **The machine froze** (the verbose console stopped with old lines on screen, then partial scrolling). The two following boots showed the log stopping at `RedNavi: DMUB: before ...`, which looked like a hang in the DMUB stage; it was not. The console lags the kernel, so the last visible line was stale. After removing the `rednavi_*` boot-args and re-enabling the stages one by one (PSP only, then DMUB, then EDID and link training) everything worked again, including link training (`dp-train`: CR/EQ done, DPCD 0x202/0x203 = 0x77 0x77).

Reading registers of blocks that are clock- or power-gated (unused pipes, DSC, DWB, parts of the hub) can lock the bus. The dump stage now needs `rednavi_dcn_dump_unsafe=1`. Linux powers those blocks, so `tools/linux_dcn_dump.py` is the safe way to capture the reference state. From macOS, read only registers of blocks that are known to be live (the pipe in use, DIG/DP 4, DCCG), and log per step with a pause so the last line is believable.
