# PSP 13.0.10 bring-up (M2 plan)

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

## Next code step

Use the channel for what amdgpu does next: `GFX_CMD_ID_LOAD_TOC` with the TOC component of the SOS file (2304 bytes, gives the TMR size), then `GFX_CMD_ID_SETUP_TMR`, then load the DMCUB firmware through the PSP or directly, as the display path requires. Before that, check in `amdgpu_psp.c` which of these steps DCN 3.2 really needs for a native framebuffer.
