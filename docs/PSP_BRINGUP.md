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

## What is still unknown

- The `PSP_BL__*` command values and the MC address of VRAM (framebuffer base from the MMHUB registers). Both must be read from the amdgpu headers before any write.
- Whether the card accepts this sequence from a cold VBIOS state without the TMR and the rest of the driver init that Linux does first. Linux does ASIC init (`atom` tables) before PSP; we have not.
- This is the first step that **writes** to the GPU and can hang it. Run only with the UHD 630 as the display.

## Next code step

Probe stage 3, still read-only: read the MMHUB framebuffer location registers to get the VRAM MC base, so a buffer address can be computed. Only after that, a separate opt-in kext flag that writes KDB.
