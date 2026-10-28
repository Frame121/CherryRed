# PSP 13.0.10 bring-up (M2)

Source of truth: `psp_v13_0.c` and `amdgpu_psp.c` in the Linux kernel (MIT). State of the card after the VBIOS is in [ARCHITECTURE.md](ARCHITECTURE.md): bootloader ready (`C2PMSG_35` bit 31), sOS not running (`C2PMSG_81` = 0).

## Firmware file

`psp_13_0_10_sos.bin` (360352 bytes) has header v2.0 and nine components. `tools/parse_psp_fw.py` lists them:

| Component | Size |
|---|---|
| KDB | 7488 |
| SYS_DRV | 61696 |
| SOS | 109216 |
| SOC_DRV | 94464 |
| INTF_DRV | 33024 |
| DBG_DRV | 28928 |
| TOC | 2304 |
| RAS_DRV | 16640 |
| RL | 6336 |

## Load sequence (`psp_hw_start`, skipped if `C2PMSG_81` is non-zero)

For each component in this order: KDB, SPL (the KDB image again, command `LOAD_TOS_SPL_TABLE`; only if the file has an SPL component), SYS_DRV, SOC_DRV, INTF_DRV, DBG_DRV, RAS_DRV, then SOS last:

1. Wait for `C2PMSG_35` == `0x80000000` (bootloader idle).
2. Copy the component into a buffer in **VRAM** aligned to 1 MB (`fw_pri`).
3. Write `C2PMSG_36` = buffer MC address `>> 20`.
4. Write `C2PMSG_35` = bootloader command (`PSP_BL__LOAD_*`).
5. Wait for `C2PMSG_35` == `0x80000000` again. For SOS instead: sleep 20 ms, then wait for `C2PMSG_81` to change.

Afterwards the driver creates the GPCOM ring (`C2PMSG_64`, `_67`, `_69`, `_70`, `_71`) and sends TMR setup, which needs more of the memory controller than the probe has touched.

## Values now known

- Bootloader commands (`amdgpu_psp.h`): `LOAD_SYSDRV` 0x10000, `LOAD_SOSDRV` 0x20000, `LOAD_KEY_DATABASE` 0x80000, `LOAD_SOCDRV` 0xB0000, `LOAD_DBGDRV` 0xC0000, `LOAD_INTFDRV` 0xD0000, `LOAD_RASDRV` 0xE0000, `LOAD_TOS_SPL_TABLE` 0x10000000, `LOAD_SPDMDRV` 0x20000000.
- VRAM as the GPU sees it (probe stage 3, `docs/logs/ioreg-rednavi-probe3.txt`): `MMHUB_FB_LOCATION_BASE` = 0x8000, `_TOP` = 0x83FF, `FB_OFFSET` = 0, in units of 16 MB. So the MC address of VRAM starts at `0x8000 << 24` = 0x80_0000_0000 and spans 16 GB, matching the VRAM size. A buffer at VRAM offset `X` has MC address `0x8000000000 + X`, and `C2PMSG_36` takes that value `>> 20`. AGP is unused (all zero).

## What is still unknown

- Whether the card accepts this sequence from a cold VBIOS state without the TMR and the rest of the driver init that Linux does first. Linux does ASIC init (`atom` tables) before PSP; we have not.
- This is the first step that **writes** to the GPU and can hang it. Run only with the UHD 630 as the display.

## Result of the first write (probe 4, 2026-10-28)

With `rednavi_psp_kdb=1` the probe copied the 7488-byte KDB (version 0x2003200) to VRAM offset 1 GB, read it back without mismatches, wrote `C2PMSG_36` and the `LOAD_KEY_DATABASE` command, and `C2PMSG_35` read `0x80000000` (ready, no error bits) afterwards. The machine did not hang and the card is still readable (`docs/logs/ioreg-rednavi-probe4.txt`).

Caveat: Linux accepts the same signal, but `0x80000000` is also the value before the command, so this alone does not prove the bootloader consumed the image. The probe should read `C2PMSG_35` and `_36` again a few milliseconds later, and the next component (SYS_DRV) will show whether the chain advances.

## Result of the full chain (probe 4 extended, 2026-10-28)

With `rednavi_psp_load=1` the probe loaded KDB, SYS_DRV, SOC_DRV, INTF_DRV, DBG_DRV, RAS_DRV and SOS in that order. All **7 of 7 components were accepted** (`psp-chain-done` = 7), with no error bits in `C2PMSG_35` and no hang. After the SOS step `C2PMSG_81` (the sOS sign-of-life register, 0 before) read `0x0136947c`. Linux treats any non-zero value as "sOS alive". Dump: `docs/logs/ioreg-rednavi-probe5-chain.txt`. The `mailbox-regs` property in that dump was read before the load, so it still shows 0 there.

So the PSP bootloader accepts our staging buffer and the card runs the secure OS. What this does not show yet: that the sOS accepts commands. That needs the GPCOM ring (`C2PMSG_64`, `_67`, `_69`, `_70`, `_71`) and the first ring command (TMR setup), which in turn needs more of the memory controller than we have touched.

A normal macOS reboot resets the card: after the next reboot `C2PMSG_81` was 0 again and the whole chain loaded a second time (the sign-of-life value differs between boots: `0x0136947c`, then `0x01433e66`, so it is not a fixed constant). The test is therefore repeatable without a power-off.

## Result of the ring (probe stage 5, 2026-10-28)

With `rednavi_psp_ring=1` the probe waited for `C2PMSG_64` to show the ready flag, wrote a 4 KB ring at VRAM +0x40200000 (MC `0x8040200000`) to `C2PMSG_69`/`_70`/`_71`, and sent the create command (`PSP_RING_TYPE__KM << 16`) to `C2PMSG_64`. The response was `0x80C20000`: bit 31 (response) set and the status field (low 16 bits) zero, which is the success test amdgpu uses (`mask 0x8000FFFF`). The bits in between are not used by amdgpu and we do not interpret them. Dump: `docs/logs/ioreg-rednavi-probe6-ring.txt`.

So the secure OS accepts commands. The ring is left allocated at that address when the kext stops; the card forgets it at the next reset.

## Result of the first command (probe stage 6, 2026-10-28)

With `rednavi_psp_cmd=1` the probe put one frame in the ring: a `psp_gfx_cmd_resp` with `GFX_CMD_ID_BOOT_CFG` and `BOOTCFG_CMD_GET`, a 4 KB command buffer at VRAM +0x40210000 and a fence buffer at +0x40220000, then moved the write pointer (`C2PMSG_67`) from 0 to 16. Result (`docs/logs/ioreg-rednavi-probe7-cmd.txt`): the **fence reached 1** and the response `status` was `0xFFFF000A` (`TEE_ERROR_NOT_SUPPORTED`), `boot_cfg` 0.

So the whole round trip works: the sOS read our frame from VRAM, executed it and wrote the fence back. The boot-config query itself is simply not supported by this PSP, which is harmless (amdgpu only uses it for GECC on some ASICs). We have a working command channel.

## What amdgpu does next, and what DCN 3.2 needs (read from `amdgpu_psp.c`, `amdgpu_dm_dmub.c`)

- After the ring, `psp_hw_start` calls `psp_tmr_init` (which sends **LOAD_TOC** to learn the TMR size) and `psp_tmr_load` (**SETUP_TMR**). **Correction (2026-10-28):** an earlier version of this note said Linux skips SETUP_TMR on this PSP. That was a misreading: `psp_early_init` sets `boot_time_tmr = false` for MP0 13.0.10, so `psp_skip_tmr()` is false and SETUP_TMR **is** sent. The Linux reference log from this very card confirms it: `reserve 0xa700000 from 0x83e0000000 for PSP TMR`.
- `amdgpu_dm_dmub.c` has two paths to get the DMCUB code in. With `load_type == AMDGPU_FW_LOAD_PSP` the PSP loads the DMCUB `inst_const` part (`AMDGPU_UCODE_ID_DMCUB`, via `LOAD_IP_FW`). Otherwise the driver copies it into the DMUB framebuffer window itself ("backdoor load", `hw_params.load_inst_const = true`). **Correction:** on this ASIC the backdoor load does not work (the instruction memory is not loadable from the host), so the PSP path with SETUP_TMR is required, see DMUB_BRINGUP.md.
- Confirmed: the DMCUB file for DCN 3.2.0 is `amdgpu/dcn_3_2_0_dmcub.bin` (`FIRMWARE_DCN_V3_2_0_DMCUB`).
- The DMUB window setup also copies the card's **VBIOS image** (obtained, see below) into the framebuffer (`DMUB_WINDOW_3_VBIOS`, from `adev->bios`). We will need the VBIOS bytes: from the PCI expansion ROM, the ACPI VFCT table, or VRAM.

## Result of LOAD_TOC (probe stage 7, 2026-10-28)

With `rednavi_psp_toc=1` the probe copied the 2304-byte TOC component to the staging buffer and sent `GFX_CMD_ID_LOAD_TOC`. Response (`docs/logs/ioreg-rednavi-probe8-toc.txt`): fence ok, `status` = 0, `tmr_size` = `0xA700000` (167 MiB). The PSP parsed the TOC and accepted it.

## VBIOS image (probe stage 8, 2026-10-28)

The PCI expansion ROM is readable: the probe enables the ROM decode bit, maps it and publishes the first (legacy x86) image as the `vbios` property. Result: **57856 bytes**, signature `0xaa55`, PCIR structure with vendor `1002` / device `747e`, ATOM header present, byte checksum 0. `tools/check_vbios.py` checks a dump. The image is vendor firmware, so it is **not committed**; `docs/logs/ioreg-rednavi-probe9-vbios.txt` has the property cut out. This is the same source as `amdgpu_read_bios`.

## Linux reference run on the same card (2026-10-28)

`docs/logs/linux-reference/` holds the output of a Linux live system (kernel with amdgpu 3.64.0, DMUB firmware 0x07002F00, SOS 0x00321717 as ours) and `tools/linux_dmcub_dump.py`. Findings:

- **The DMCUB registers with Linux's DMCUB running match what our probe programs.** `CW3` to `CW6` offsets are `0x83fe8e5500`, `...fe8f3700`, `...fe8f7700`, `...fe907800`: block base `0x83fe800000` plus exactly the window offsets `tools/dmub_layout.py` computes (0xe5500, 0xf3700, 0xf7700, 0x107800). `REGION5_TOP` 0x8001003f, `OUTBOX0` base 0xa0000010 size 0x10030, `INBOX1` base 0x64000000 size 0x2000, `OUTBOX1` base 0x64002000: identical to ours. The layout, the MC-address mode and the window programming are right.
- **`CW0` and `CW1` read 0 also under Linux**, `SEC_CNTL` 0, `TMR_AXI_SPACE` 0. So these are PSP-managed, as suspected.
- **`SCRATCH0` = 3** (dal_fw and mailbox_rdy), `SCRATCH1` = 0x07002f00 (the firmware version), `FAULT_ADDR` 0, timer running, `CNTL` = 0x001900c6, inbox pointers 0x1600/0x1600.
- **Order of events in dmesg:** VBIOS fetched (from VFCT), GART enabled (512 MB), DMUB marked "via PSP", PSP initialised with `reserve 0xa700000 from 0x83e0000000 for PSP TMR`, SMU initialised, then `DMUB hardware initialized` about 2 ms after SMU. The TMR is placed at the very top of VRAM; VRAM is 16368 MB (the last 16 MB are reserved) and Linux sees BAR0 as 16 GB while macOS gives us 256 MB.
- **The cause of our halted DMCUB (confirmed by the next test):** we never sent **SETUP_TMR**, and when we did, the TMR had to sit at the top of VRAM. `LOAD_IP_FW` then returned `fw_addr` `0xfee00000`, an address that is not inside any TMR we configured, and the PSP could not place the code where the core boots from. `rednavi_psp_tmr=1` sends SETUP_TMR 32 MB into VRAM (inside the 256 MB visible BAR) and is accepted, but the DMCUB still does not start; `rednavi_psp_tmr=2` puts the TMR at VRAM offset `0x3e0000000` like Linux, and that works.
- Also: SMU initialisation precedes the DMUB init in Linux; it may matter for clocks. Linux's `smu fw version 0x00505600`; ours is the IFWI one.

## What came next

SETUP_TMR with the TMR at the top of VRAM, then `LOAD_IP_FW` for the DMCUB, started the display firmware (see DMUB_BRINGUP.md). Optional PSP follow-ups, not needed so far: `FB_FW_RESERV_ADDR` query, SMU firmware load (`psp_load_smu_fw`).
