# DMUB / DMCUB bring-up for DCN 3.2 (M3 plan)

Source of truth: `drivers/gpu/drm/amd/display/dmub/` (`dmub_srv.c`, `dmub_dcn32.c`, `dmub_dcn31.c`) and `amdgpu_dm/amdgpu_dm_dmub.c` in the Linux kernel (MIT). Nothing in this document has been run on the card yet; the layout below is a dry run (`tools/dmub_layout.py`).

## What the firmware file contains

`dcn_3_2_0_dmcub.bin` (284432 bytes): header v1.0, firmware version `0x07003600`, `inst_const` 284176 bytes, no separate `bss_data`. The instruction region is wrapped by a 256-byte PSP header (skipped) and a 256-byte PSP footer (dropped), leaving 283664 bytes of code. A 64-byte meta block just before the footer (magic `0x444D5542`) gives the sizes of the firmware-state region (47296), the trace buffer (65552) and the shared state.

## Window layout (all in VRAM)

`python3 tools/dmub_layout.py dcn_3_2_0_dmcub.bin 57856` reproduces `dmub_srv_calc_region_info` with our VBIOS size:

| Window | Offset | Size |
|---|---|---|
| INST_CONST | 0x0 | 283712 |
| STACK (128 KiB stack + 512 KiB context) | 0x45500 | 655360 |
| BSS_DATA | 0xe5500 | 0 |
| VBIOS | 0xe5500 | 57856 |
| MAILBOX (inbox + outbox, 8 KiB each) | 0xf3700 | 16384 |
| TRACEBUFF | 0xf7700 | 65600 |
| FW_STATE | 0x107800 | 47296 |
| SCRATCH_MEM | 0x113100 | 1024 |
| IB_MEM | 0x113500 | 3200 |
| SHARED_STATE | 0x114200 | 1536 |
| LSDMA_BUFFER | 0x114800 | 65536 |
| CURSOR_OFFLOAD | 0x124800 | 14272 |

Total 1184 KiB, one contiguous block; each window is 256-byte aligned. The struct sizes behind IB_MEM, SHARED_STATE and CURSOR_OFFLOAD (3148, 1536, 14232 bytes) were obtained by compiling `dmub_cmd.h` against stub headers.

amdgpu allocates this block in **GTT** (system memory) on DCN 3.2, with a comment that it limits the choice for latency reasons; other ASICs allow VRAM too. We can put it in **VRAM** and avoid needing GART page tables.

## Hardware init sequence (`dmub_srv_hw_init`, DCN 3.2 uses the dcn31/dcn32 hooks)

1. `reset`: put the DMCUB in reset.
2. Set up code windows CW0 (instructions) and CW1 (stack), then **backdoor load**: the host copies the instruction region into window 0 and the hardware pulls it in (`dmub_dcn31_backdoor_load`). The host first reads the whole region back so no write is still pending.
3. Windows CW2 to CW6 (bss, VBIOS, mailbox, trace, firmware state) at fixed DMUB-side bases `0x60000000`, `0x61000000`, `0x63000000` ... `0x66000000`, plus the inbox/outbox/outbox0 ring setup.
4. `reset_release`, then wait for the firmware to report ready (auto-load flag in the scratch registers).

The register addresses are `regDMCUB_*` in the DCN 3.2 register headers, offset from the DMU base address already in the discovery table.

## What is still unknown

- The exact DCN 3.2 `regDMCUB_*` offsets and which of them need DMU instance bases (read `dmub_dcn31.c`, `dmub_dcn32.c` and the DCN 3.2 `dcn_3_2_0_offset.h`).
- Which clocks and power domains must be up before releasing reset; the VBIOS left some running, but it is not known how much (the SMU is alive, see ARCHITECTURE.md).
- Whether the DMCUB can reach VRAM at `0x8000000000 + offset` without our setting up the MMHUB/GART context (the PSP did, but the DMCUB uses its own VMID 0 view).

## Result of the DMCUB register read (probe stage 9, 2026-10-28)

Read-only, `docs/logs/ioreg-rednavi-probe10-dmcub.txt` (registers at DMU base index 2):

| Register | Value |
|---|---|
| `DMCUB_CNTL` | 0x001900c6 |
| `DMCUB_SEC_CNTL` | 0x0 (`DMCUB_SEC_RESET` clear: not held in reset) |
| `DMCUB_SCRATCH0` (boot status) | 0x2: `mailbox_rdy` set, `dal_fw` **clear** |
| `DMCUB_SCRATCH1` | 0x07008748 |
| `DMCUB_INBOX1_WPTR` / `_RPTR` | 0x440 / 0x440 |
| `DMCUB_OUTBOX1_WPTR`, `_CNTL2`, `_SCRATCH2`, `_GPINT_DATAOUT`, `_REGION3_CW0_OFFSET` | 0 |

Reading: the **VBIOS already started a DMCUB** and it is idle and healthy. Its mailbox is ready, and the inbox read and write pointers are equal at 0x440, i.e. 17 commands of 64 bytes were consumed. `dal_fw` is clear, so it is the boot-time firmware and not the DAL firmware that amdgpu loads; `dmub_dcn31_is_hw_init` would say "not initialised" because it requires `dal_fw`. `SCRATCH1` (0x07008748) is a version-like value newer than the file we have (0x07003600), which fits a firmware shipped with the card's IFWI. The `DMCUB_CNTL` field layout was not decoded.

Consequences: reading DCN registers is safe on this card (no hang). amdgpu would reset this DMCUB and load the DAL firmware over it; we can do the same, or first try to talk to the running one. Loading ours needs the reset-and-backdoor sequence below, which writes to the card.

## First attempts to load our DMCUB (probe stage 10, 2026-10-28)

`rednavi_dmub_load=1` runs the Linux sequence: STOP_FW to the running DMCUB, soft reset, build the 1184 KiB block in VRAM at +0x50000000 (code and VBIOS copied, readback of the code verified), program windows CW0/CW1 and CW3 to CW6, the REGION5 alias, inbox/outbox and outbox0, release reset, wait for `SCRATCH0` bits `dal_fw` and `mailbox_rdy`. The register offsets and field positions are from `dcn_3_2_0_offset.h` / `_sh_mask.h`.

| Attempt | Outcome |
|---|---|
| Backdoor load (CW0/CW1 programmed by the driver) | The old DMCUB came back as before: `SCRATCH0` = 2 (mailbox ready, **not** `dal_fw`), `SCRATCH1` = 0x07008748, `CNTL` = 0x1900c6. Our code did not run; the instruction memory is evidently not loadable from the host on this ASIC (it is PSP-owned, which is why amdgpu uses `AMDGPU_FW_LOAD_PSP` here). |
| `rednavi_dmub_psp=1`: signed image (PSP header + code + footer, 284176 bytes) to the sOS with `GFX_CMD_ID_LOAD_IP_FW`, type `GFX_FW_TYPE_DMUB` (51), then the Linux hw_init without the backdoor load | **The sOS accepted it**: fence ok, `status` 0, `fw_addr` `0xfee00000` (inside the TMR). After release: `CNTL` = 0x900c6 (enabled), `SCRATCH0` = 0, `DMCUB_TIMER_CURRENT` = 0, `DMCUB_UNDEFINED_ADDRESS_FAULT_ADDR` = 0xfffffff0, no mailbox traffic. The DMCUB is enabled but not executing. |
| Same with `rednavi_dmub_mode=1` (data windows addressed by VRAM offset instead of MC address) | Identical result. |

Logs: `docs/logs/ioreg-rednavi-probe11-dmub-load.txt`. The desktop is not harmed (UHD 630 is the display, a reboot resets the card); the DMCUB just ends up halted.

Reading: the fault address 0xfffffff0 and the stopped timer say the core faulted at its reset vector, i.e. no instruction window was mapped when it started. In PSP mode amdgpu never programs CW0/CW1 itself (that is only done in `backdoor_load`), so either the PSP sets them during `LOAD_IP_FW` and our reset sequence or ordering undoes it, or the PSP load must target a different place or happen after some step we skipped. Open questions for the next session:
- Does amdgpu send `LOAD_IP_FW` for the DMCUB before or after the display hw_init resets the block, and is the TMR copy reached through CW0 with some register we have not set (for example `DMCUB_REGION3_TMR_AXI_SPACE`, 0x21a, default 3 = FB physical)?
- Read the CW0/CW1 and `DMCUB_SEC_CNTL` registers right after `LOAD_IP_FW`, before our reset, to see what the PSP programmed.
- Read `amdgpu_dm_dmub.c`'s hw_init caller and `dmub_dcn32_*` for any step in PSP mode that this probe still skips.

## Snapshots of the DMCUB windows (probe, 2026-10-28)

`docs/logs/ioreg-rednavi-probe12-dmcub-snapshots.txt` has three snapshots of the window, security and control registers: at boot (VBIOS DMCUB running), right after `LOAD_IP_FW`, and at the end of our sequence.

- **At boot, with the VBIOS DMCUB running:** `CW0` and `CW1` (base, top, offset) all read **0**. `CW3` to `CW6` are programmed with full MC addresses near the top of VRAM (`CW3_OFFSET` 0x83fec20000, CW4 `...fec40000`, CW5 `...fec50000`, CW6 `...fec60000`, tops `0x8301ffff`/`0x8400ffff`/`0x8500ffff`/`0x8600ffff`), i.e. a firmware-owned area of about 1 MB. `TMR_AXI_SPACE` reads 0, `SEC_CNTL` 0, `TIMER` is ticking. So the running firmware does not fetch code through CW0.
- **After `LOAD_IP_FW`:** the PSP stopped the DMCUB: `CNTL` 0x800c6 (`DMCUB_ENABLE` clear), `CNTL2` 1 (soft reset asserted), `TIMER` 0. CW0/CW1 still read 0 (they may be PSP-protected and read as zero for us).
- **After releasing the reset, with or without our window programming** (`rednavi_dmub_min=1` releases only): `CNTL` 0x900c6, `CNTL2` 0, but `TIMER` stays 0, `SCRATCH0` never changes and `DMCUB_UNDEFINED_ADDRESS_FAULT_ADDR` reads **0xfffffff0**. So the core is released but faults on its first instruction fetch, and our window programming is not the cause.

**Update:** a Linux reference run (see `docs/PSP_BRINGUP.md`, `docs/logs/linux-reference/`) shows the same window programming works there, and that Linux sends SETUP_TMR before loading the DMCUB, which we did not. That is now the main suspect; test pending.

The unanswered question was therefore what makes the PSP-loaded code reachable at the reset vector. Candidates, none tested: a PSP step Linux performs earlier that we skip (the full `psp_load_non_psp_fw` list, `SETUP_TMR`-equivalent state, `psp_update_fw_reservation`); a register in `DMCUB_SEC_CNTL` (`MEM_SEC_LVL`, `MEM_UNIT_ID`) or `MEM_CNTL` that the PSP expects the driver to set; clocks (`DMCUBCLK`, DCCG) that the SMU/DC init in amdgpu brings up before this point. A clean way to narrow it down is a trace of the same sequence on Linux (`umr` register dump of `DMCUB_*` right after `dm_dmub_hw_init`) on the machine's Windows/Linux side, or a read of the SMU-side clock state.

The probe boot-args for this stage (`rednavi_dmub_load`, `rednavi_dmub_psp`, `rednavi_dmub_mode`, `rednavi_dmub_min`) have been removed from the desktop; the PSP stages stay enabled.

## The DMCUB runs (2026-10-28)

With **SETUP_TMR sent before `LOAD_IP_FW`, and the TMR placed where Linux places it** (`rednavi_psp_tmr=2`: VRAM offset `0x3e0000000`, size `0xa700000` from LOAD_TOC), our DMCUB firmware starts:

| Register | Value |
|---|---|
| `SCRATCH0` | 0x3 (`dal_fw` and `mailbox_rdy`) |
| `SCRATCH1` | **0x07003600**, the version of the file we loaded (the VBIOS firmware reported 0x07008748) |
| `CNTL` / `CNTL2` | 0x1900c6 / 0 |
| `FAULT_ADDR` | 0 |
| `TIMER` | running |

Dump: `docs/logs/ioreg-rednavi-probe13-dmcub-running.txt`. `LOAD_IP_FW` now returns `fw_addr` `0x83fee00000`, a real address near the top of VRAM.

What it took, in order: LOAD_TOC, **SETUP_TMR** (status 0), `LOAD_IP_FW` of the signed image, then the unchanged `dmub_dcn32` sequence with the windows of `tools/dmub_layout.py`.
- TMR at 32 MB into VRAM (`rednavi_psp_tmr=1`): SETUP_TMR accepted, but `fw_addr` came back as `0x80fee00000` and the core still faulted at `0xfffffff0` with the timer stopped.
- TMR at the top of VRAM, as the Linux reference run: starts. The PSP evidently requires the TMR in that region.
- Without SETUP_TMR: `fw_addr` came back as `0x00fee00000` and the core faulted.

Not yet done: send a DMUB command through the inbox and read the reply, to prove the mailbox works end to end; the SMU was not initialised by us (Linux does that before the DMUB init and it did not matter here).

## Next code step

Send a first DMUB command through inbox1 and read the response (for instance `DMUB_CMD__REG_SEQ_READ_MODIFY_WRITE`-free ones such as the firmware boot options or a no-op), then look at what DC needs to program a mode on one connector.
