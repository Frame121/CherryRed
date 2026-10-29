# DisplayPort bring-up on path 0 (steps 3 and 4 of `DC_PLAN.md`)

Goal: a 1920x1080@60 test pattern on the Samsung monitor attached to VBIOS path 0 (DisplayPort, UNIPHY2, AUX4, HPD4). This note records what was read in the Linux display code (MIT) and what has been tried on the card. Since then the link has been trained, a test pattern and an image from VRAM have been shown, and an `IOFramebuffer` publishes the scanout (see the sections below, newest last).

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

## What was left at that point

The pixel clock for the stream (the PHY PLL is up at the symbol rate; the stream pixel clock is a separate DTO), stream setup, OTG timing, the pattern generator and unblanking. These were done by the register replay described below.

Open risks at the time: the exact clock state needed for the transmitter and DPG (the clock manager and SMU are not driven yet), the DISPCLK/DPPCLK the VBIOS left, and whether the OPP/OPTC clocks are enabled without the DCCG sequence.

## Lesson: do not read the whole DCN register list from macOS (2026-10-28)

`rednavi_dcn_dump=1` (probe stage 14) read all 4366 registers of `tools/dcn32_reglist.txt` after link training. **The machine froze** (the verbose console stopped with old lines on screen, then partial scrolling). The two following boots showed the log stopping at `RedNavi: DMUB: before ...`, which looked like a hang in the DMUB stage; it was not. The console lags the kernel, so the last visible line was stale. After removing the `rednavi_*` boot-args and re-enabling the stages one by one (PSP only, then DMUB, then EDID and link training) everything worked again, including link training (`dp-train`: CR/EQ done, DPCD 0x202/0x203 = 0x77 0x77).

Reading registers of blocks that are clock- or power-gated (unused pipes, DSC, DWB, parts of the hub) can lock the bus. The dump stage now needs `rednavi_dcn_dump_unsafe=1`. Linux powers those blocks, so `tools/linux_dcn_dump.py` is the safe way to capture the reference state. From macOS, read only registers of blocks that are known to be live (the pipe in use, DIG/DP 4, DCCG), and log per step with a pause so the last line is believable.

## Replay of a lit Linux pipe (2026-10-28)

**Linux reference.** `tools/linux_dcn_pipe_dump.py` read 982 registers in six groups without any stall while amdgpu lit the monitor (`docs/logs/linux-reference/dcn_pipe_linux.txt`). It shows the whole path for 1080p60 on this board: `OTG0` timing (`H_TOTAL` 0x897, `V_TOTAL` 0x464, blank and sync as in the EDID), `ODM0` source OPP0, `OPP_PIPE0` clock on, `MPC_OUT0_MUX` 0x100, `DIG4_DIG_FE_CNTL` 0x01000100 and `DIG4_DIG_BE_CNTL` 0x40001000 (front end 4, HPD 4), `DP4` stream registers (`VID_N` 0x8000, `MSA_TIMING_PARAM1..4`), `DP_DTO0_PHASE/MODULO` 0x046cf710 / 0x2ab77490, `OTG_PIXEL_RATE_DIV` 0xdb2, `SYMCLKE_CLOCK_ENABLE` 0x401.

**Replay.** `tools/gen_dcn_replay.py` turns 57 of those configuration registers into `RedNavi/dcn_replay.h`, in nine groups in the order DC programs them. `rednavi_dcn_replay=N` applies groups 1 to N after the DP link is trained, logging each group and reading it back. Results on the card:
- The VBIOS had already lit OTG0 at 1080p60 (`H_TOTAL` 0x897, `V_TOTAL` 0x464) with the DP video stream enabled but `DIG4_DIG_FE_CNTL` = 0 and `DP_VID_M` = 0, so the DIG front end was not connected. The replay therefore stops the DP video stream and disables the OTG before reprogramming (`optc1_disable_crtc` equivalent).
- Groups 1 to 3 (clocks, OPTC clocks, OTG timing) and 1 to 6 (plus OPP, MPC and the DIG/DP stream setup) ran without a stall.
- All nine groups ran: the OTG is enabled and counting (`OTG_CONTROL` 0x11201, position changes between reads), `DP_VID_STREAM_CNTL` 0x10001 (stream enabled and reporting active), DP link still trained (`DP_LINK_CNTL` 0x110).
- Read-back differences are status or hardware-owned bits (`OTG_CLOCK_CONTROL` bit 16, `FMT_CONTROL`, `MPC_OUT0_CSC_MODE` bit 7, `DIG_FIFO_CTRL0/1`, `OTG_CONTROL` bit 16) except `DP4_DP_VID_M`, which stays 0 (Linux reads 0x7595): the hardware has not measured a stream clock against the link clock, so the picture may not be reaching the sink yet.

**Result: the monitor shows the colour-square test pattern** (confirmed by eye on the Samsung F24G3xTF, DisplayPort input, 1920x1080@60). `DP4_DP_VID_M` still reads 0 in our run, so it is not a reliable indicator that the stream is flowing; the picture is. This is the first image produced by the RedNavi kext.

## What the Linux plane dump says (2026-10-28)

`docs/logs/linux-reference/` now has the plane groups (`dcn_plane_linux_tiled.txt`), DC's own state log (`dtn_log_tiled.txt`), the DRM state (`drm_state_active.txt`), the DP link (`dp_link.txt`), clocks (`pp_dpm.txt`) and the PCI BARs (`lspci_gpu_bars.txt`). What matters:

- **Linux does not use a linear plane.** GNOME's framebuffer is `XR30` (XRGB2101010) with the AMD modifier `0x200000020867b04`: tiled (`sw_mode` 0x1b) **and DCC-compressed** (`dcc_en` 1, meta surface at `0x83f7870000`). The surface is at MC `0x83f7000000`, pitch 7680 bytes. Replaying these `HUBP` values would need a tiled, compressed buffer, so the `HUBP`, `HUBPREQ` DLG/TTU and request-size registers cannot be copied for a plain linear buffer. A second capture is needed with a **linear** scanout (a text console: fbcon uses a linear framebuffer).
- **Linux trains the link at RBR x4**, not HBR2: `link_settings` current `4 lanes, rate 0x6`, verified/reported `4 x 0x14`. The replay table (`DP_DTO0_PHASE/MODULO`, `VID_M` 0x7595, `VID_N` 0x8000, symbol clock registers) was therefore captured at 162 MHz while our probe trained at 540 MHz. The pattern appeared anyway, but the state was inconsistent. The probe now trains at RBR by default (`rednavi_dp_rate=0x14` for HBR2).
- **Clocks while a 1080p60 plane runs** (`dtn_log_tiled.txt`): DISPCLK 150.877 MHz, DPPCLK 149.565 MHz, DCFCLK 199 MHz, FCLK 601 MHz. DSC and HPO (128b/132b) are off, MPCC0 uses DPP0/OPP0 in mode 2, watermarks as in the HUBBUB regs.
- **The VRAM aperture registers are missing from the dump:** `HUBPREQ0_DCN_VM_SYSTEM_APERTURE_*` read 0 and the framebuffer location is in `DCN_VM_FB_LOCATION_BASE/TOP/OFFSET` (offset 0x475 and following), which the list did not contain. It now has a `dcnvm` group.
- **BAR0 is 16 GB under Linux (Resizable BAR on) and 256 MB under macOS.** Everything the macOS probe does in VRAM uses `MM_INDEX`/`MM_DATA`, which does not depend on it; a CPU-written scanout buffer must sit inside the first 256 MB unless the BAR is resized.

## A plane reads an image from VRAM (2026-10-28)

With `rednavi_dcn_replay=14` the monitor shows the **test image the probe wrote into VRAM** (red/green gradient with a blue checker, white 8 pixel frame, eight colour bars along the bottom): a linear XRGB8888 1920x1080 surface at VRAM `0x60000000` (MC `0x8060000000`; later moved to `0x08000000`, see the IOFramebuffer section), pitch 7680, scanned out by HUBP0 -> DPP0 -> MPCC0 -> OPP0 -> OTG0 -> DIG4 -> the DisplayPort link trained at RBR x4. It is the first picture whose pixels come from memory the kext filled, not from the pattern generator. Dump: `docs/logs/ioreg-rednavi-probe17-plane.txt`.

How it was built: the register state of Linux's text console (linear `XR24`, `docs/logs/linux-reference/dcn_plane_linux_linear.txt`) replayed in five more groups on top of the pattern pipe: 10 DCHUBBUB watermarks and the VRAM aperture, 11 DPP clock and conversion blocks, 12 the MPCC0 tree, 13 the HUBP surface (the primary surface address replaced by ours), 14 pattern generator off. The tiled and DCC-compressed capture from GNOME was not usable for this; a linear surface needed its own capture. Boot-arg: `rednavi_dcn_replay=14`. Note the kext takes longer to load with the image fill (a few seconds), so the desktop needs a few minutes before SSH answers.

At that point it was not yet a framebuffer: the image was a fixed test image, the mode was the replayed 1080p60, and nothing was visible to WindowServer. The IOFramebuffer section below covers the next step.

## IOFramebuffer (RedNaviFB)

Result (2026-10-28): the macOS desktop is drawn on the Samsung monitor through our scanout, with the Intel iGPU switched off (`-wegnoigpu`). With the iGPU on, macOS lists the AMD display as an extra display at x = -1920 but WindowServer does not draw into it (only the in-kernel software cursor lands in the buffer), so the simplest working setup is the card as the only display.

How it works:

- The scanout surface sits at VRAM offset `0x08000000`, inside the 256 MB macOS maps through BAR0, so the kernel and WindowServer can write it with the CPU. The replay table in `RedNavi/dcn_replay.h` points the display plane at MC address `0x8008000000`.
- `RedNaviProbe` sets `fb-scanout-phys` (CPU physical address, BAR0 + offset) and then `rednavi-fb-ready` on its own node, with boot-args `rednavi_fb=1` and `rednavi_dcn_replay=14`.
- `RedNaviFB` (`RedNaviFB/kern_fb.cpp`) is an `IOFramebuffer` subclass that matches that property: one mode, 1920x1080 at 60 Hz, XRGB8888, pitch 7680, the surface as aperture (write-combined), the EDID the probe read through AUX (`edid` property) via `getDDCBlock`.
- It is a separate kext because IOGraphicsFamily is in the system kernel collection and OpenCore only links injected kexts against the boot collection (`Prelinked injection ... - Invalid Parameter` in the OpenCore log).

Installing `RedNaviFB.kext` (sibling of the probe, not injected by OpenCore):

1. SIP off: `csr-active-config` `FF0F0000` in OpenCore's `config.plist` (and not in `NVRAM > Delete`).
2. `sudo cp -R RedNaviFB.kext /Library/Extensions/ && sudo chown -R root:wheel /Library/Extensions/RedNaviFB.kext`
3. `sudo codesign -f -s - /Library/Extensions/RedNaviFB.kext` (without it: "Bad code signature")
4. `sudo kmutil load -p /Library/Extensions/RedNaviFB.kext`, then reboot. (`kmutil install --update-all` fails without a KDK.)

Lessons:

- `IODisplay::readFramebufferEDID` calls `getAttributeForConnection` with a NULL value pointer; writing through it panics inside WindowServer's open call.
- A faulty kext in the auxiliary collection hangs the boot; recover from a backup EFI stick and delete `/Library/Extensions/RedNaviFB.kext`.
- The stock `IONDRVFramebuffer` on `GFX0/.Display_boot` (from the firmware boot framebuffer) shows up in System Information as a second, nameless "Display" with the same serial number.
- System Information takes the GPU name and VRAM size from the `model` and `VRAM,totalMB` properties of the PCI node, which the probe now sets.
- The `fb-diag` property of the `RedNaviFB` node shows WindowServer's calls and sampled scanout pixels.

## Modes from the EDID (issue #3, step A, 2026-10-29)

`RedNavi/edid.cpp` parses the EDID the probe reads over AUX: detailed timings (the first is the preferred mode), CTA-861 video data blocks (a table of common progressive VICs), the established timings that have a fixed DMT definition (640x480, 800x600 and 1024x768 at 60 Hz) and the standard timings, which are generated with CVT reduced blanking. Interlaced modes are skipped. The probe publishes the result as `modes` and `monitor-name` on the `RedNaviProbe` node. For the Samsung F24G3xTF it lists 17 modes, from 640x480@60 to 1920x1080@144 (325.07 MHz). `rn_min_dp_link_rate` gives the lowest link rate that carries a mode on 4 lanes at 24 bpp (RBR is enough up to 216 MHz, HBR up to 360 MHz, HBR2 up to 720 MHz).

Host tests (`make test`) run the parser on the monitor's real EDID with the serial number removed and check CVT-RB against the VESA values (1080p60: 138.5 MHz, 720p60: 64.0 MHz, 1024x768: 56.0 MHz).

Not done yet: programming any mode other than the replayed 1080p60. That needs the list of registers that depend on the mode (OTG timing, the DP stream and MSA registers, the pixel-rate divider and DP DTO, the plane viewport and pitch, MPC and DSCL sizes), which will be derived from Linux register captures at other modes, see `tools/linux_dcn_pipe_dump.py`.

### Programming other modes (issue #3, step C, 2026-10-29)

`rednavi_mode=WxH@R` (a mode the monitor lists; default is the EDID's preferred one) makes the probe light that mode. `RedNavi/mode_regs.cpp` recomputes the registers of the replay table that depend on the mode; the base mode (1920x1080@60, VIC 16) keeps the captured values untouched. The formulas were derived from three Linux captures of the same card (`docs/logs/linux-reference/modes/`, 1280x720@60, 1920x1080@60 and 1920x1080@144) and are checked against them in `tests/host/mode_regs_test.cpp`:

- exact: OTG horizontal and vertical timing, sync widths and polarity, DP MSA timing parameters (`vtotal | htotal << 16`, blank ends, sync widths with the polarity in bit 15, active size), the DP stream clock DTO (`phase = pclk_kHz * 500`), plane viewport and DSCL sizes, pitch (width rounded up to 64 pixels), `HUBPREQ0_DST_DIMENSIONS` (`htotal * 12.8e6 / pclk_kHz`), `REF_FREQ_TO_PIX_FREQ` (`2^18 * 100000 / pclk_kHz`), `OTG_VSTARTUP_PARAM` (vertical blank lines minus one), the line-buffer partition count, the end of vertical blank in `BLANK_OFFSET_0`;
- scaling laws (within 1%): TTU values, per-line delivery, `DST_AFTER_SCALER`, taken from the linear 1080p60 baseline;
- kept from the baseline because no formula was found: the rest of the DML output (watermarks, prefetch, `GLOBAL_TTU_CNTL`, vblank parameters). These are the values to watch for underflow.

The link rate follows the mode: the lowest rate that carries it with 20% headroom (RBR up to about 216 MHz at 24 bpp, HBR2 above), as Linux does (it trains 2 lanes RBR for 720p, 4 lanes RBR for 1080p60 and 4 lanes HBR2 for 1080p144). We always use 4 lanes.

Result on the Samsung F24G3xTF: **1280x720@60 is stable, no flicker, no artefacts** (after fixing the vertical blank end in `BLANK_OFFSET_0`, which had left a black line at the top). When the resolution changes, the new display gets a new ID and macOS puts the firmware boot framebuffer (the nameless "Display") back as main; moving the origin of the RedNavi display to (0, 0) with `CGConfigureDisplayOrigin` fixes it.

**Modes verified on the Samsung F24G3xTF:** 1280x720@60, 1024x768@60 (negative sync polarity, 4:3) and 1920x1080@144. 1080p144 first came up as a grey screen: the VBIOS leaves `DENTIST_DISPCLK_CNTL` at `0x54195454` (DISPCLK about 165 MHz with the 4.3 GHz VCO and quarter-step dividers) and the 325 MHz pixel clock needs about 331 MHz. `rn_dispclk_register` lowers the divider code when the current clock is below the pixel clock plus 1.75% (Linux writes `0x341f3434`, we write `0x347f3434` after keeping the middle byte). With that the picture is stable at 144 Hz on HBR2 x4 and macOS reports 144.00 Hz. The register is only changed when needed, and only after the pipe is quiesced.

Open: web "refresh rate" tests in a browser report 30 fps. The kext has no vertical-blank interrupt (`registerForInterruptType` is not implemented), and WindowServer renders on the CPU, so display-link style timers probably fall back to a slow software tick.

### Mode list and live mode changes (issue #3, step D, 2026-10-29)

`RedNaviFB` now lists every mode of the EDID that the link can carry (up to HBR2 x4) and that fits the scanout area (`fb-modes`, published by the probe); System Settings shows them all. Choosing one makes `setDisplayMode` call the probe through the platform function `rednavi-set-mode` (an index into the EDID list). The probe stops the DP stream, the OTG and the DIG front end, retrains the link at the rate the mode needs (RBR up to ~216 MHz, HBR up to ~345 MHz, HBR2 above, 4 lanes, 20% headroom), raises DISPCLK if the pixel clock needs it, and replays the register table with the mode's values (without repainting the test image). If the DP video stream does not report active afterwards, the probe goes back to the previous mode and returns an error, so a bad mode should not leave a black screen.

Verified on the Samsung F24G3xTF by changing the mode 19 times in a row from System Settings (1080p at 50, 60, 120 and 144 Hz, 720p at 50 and 60 Hz, 1024x768, 1680x1050 at 60 and 120 Hz and others): no crash, no black screen. The mode macOS remembers is applied again at the next boot, after the probe has lit the preferred mode. Earlier a sequence of changes had hung the machine; that was before the DIG front end was switched off in the quiesce step and before the stream check and automatic revert, and the exact cause was not captured.

Limits: the DML-derived registers (watermarks, prefetch and so on) come from the 1080p60 baseline, so a mode with unusual blanking could underflow; the web refresh-rate test reports 30 because WindowServer renders on the CPU. The firmware boot framebuffer (the nameless second display, an `IONDRVFramebuffer` on the same PCI device) is registered after the RedNaviFB kext starts. Terminating it from the kext (`terminate(kIOServiceRequired)`, tried with a retrying timer) **panics the machine**: `IOFramebuffer::terminate` notifies `AppleUpstreamUserClient::fbNotificationOccurred`, which dereferences a null pointer (macOS 15.8.2). It is therefore left alone by default; boot-arg `rednavi_kill_boot_fb=1` re-enables the attempt for experiments. The extra display is harmless as long as the RedNavi display is the main one (`CGConfigureDisplayOrigin`, see the notes above).
