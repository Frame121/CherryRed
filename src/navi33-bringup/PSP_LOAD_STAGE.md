# PSP 13.0.7 firmware-load stage (navi33psp_load=1 / navi33psp_chain=1)

Status: **implemented + host-tested (2026-10-31); NOT yet run on the RX 7600.**
This is the first WRITE stage of the compute-first plan (C1). Everything before
it — the Stage 3 PCI survey and the `navi33psp=1` mailbox survey — is read-only;
this stage is the point where the kext first writes to the card, and every
write is confined to the bootloader mailbox and the MM_INDEX/MM_DATA window.

## Gates

- `navi33bringup=1` — the whole kext (existing opt-in).
- `navi33psp=1` — the read-only mailbox survey runs first and records the
  pre-load PSP/SMU state as evidence.
- `navi33psp_load=1` — **KDB only**: one component, the smallest possible first
  write. No SOS is started; the machine's display path is not touched.
- `navi33psp_chain=1` — **full chain**: KDB → SPL → SYS_DRV → SOS, then waits
  for the sOS sign-of-life register and records the sOS version.

Boot-arg absence is fail-closed at every level: without the flags the kext
stays at the read-only scaffold (Stage 3) or the read-only survey.

## What it does

1. Reads (never writes) the PCI command register; aborts if memory decode is off.
2. Maps the MMIO register BAR (**BAR5**) and verifies the window covers every
   pinned register (largest need: `MMHUB_FB_LOCATION_TOP` at byte `0x6A3B4`).
3. Pre-conditions, all live reads: `MP0_C2PMSG_33` bit 31 (IFWI init done),
   `MP0_C2PMSG_35` == `0x80000000` (bootloader ready), `MP0_C2PMSG_81` == 0
   (secure OS NOT running — loading over a live sOS is refused).
4. Reads live VRAM topology: `MMHUB_FB_LOCATION_BASE/TOP` (24-bit, 16 MiB
   units) and `RCC_CONFIG_MEMSIZE`; publishes them, then computes the MC base
   (`base << 24`). Implausible values abort.
5. Probes the MM_INDEX/MM_DATA window by reading the IP Discovery signature at
   `VRAM top - 64 KiB` (expected `0x28211407`). This proves the window works
   before any firmware byte is written.
6. Parses the embedded `psp_13_0_7_sos.bin` v2.0 header (bounds-checked) and
   builds the load plan (below). An invalid header aborts.
7. Per component: waits for the bootloader, stages the payload into VRAM at
   offset `0x40000000` (1 GiB) through the MM window, **reads every dword back**
   and counts mismatches, writes `C2PMSG_36 = (mcBase + offset) >> 20`, writes
   `C2PMSG_35 = command`, waits for the response (bit 31), decodes the error
   status (bits[7:0]).
8. After SOS: `mdelay(20)`, then polls `C2PMSG_81` for a CHANGE from its
   pre-load value (Linux's `psp_wait_for(..., CHANGED)`), then reads
   `C2PMSG_58` (sOS version).
9. On a full-chain success (`sOS alive`) publishes
   `Navi33Bringup,FirmwareVerified = true` — the firmware leg of the
   accelerator's compute gate (Navi33Accelerator). `MemoryVerified` /
   `QueueVerified` stay unset until C2/C3.

## Component table of the real blob (header metadata, read 2026-10-31)

`psp_13_0_7_sos.bin` (248,496 bytes, header v2.0, ucode `0x330024`): 5 bins —
offsets are relative to `ucode_array_offset_bytes` (256), sizes are contiguous
and end exactly at `ucode_size_bytes` (248,240):

| # | Type | Version | Offset | Size |
|---|---|---|---|---|
| 1 | SYS_DRV (2) | `0x330024` | 0 | 141,200 |
| 2 | SOS (1) | `0x330024` | 141,200 | 99,184 |
| 3 | TOC (4) | `0x00000c` | 240,384 | 2,304 |
| 4 | KDB (3) | `0x04003300` | 242,688 | 4,880 |
| 5 | SPL (5) | `0x010033c0` | 247,568 | 672 |

Note this differs from RedNavi's PSP 13.0.10 file (9 bins incl. SOC_DRV,
INTF_DRV, DBG_DRV, RAS_DRV, RL). The load plan therefore runs
**KDB → SPL (the KDB image, `LOAD_TOS_SPL_TABLE`) → SYS_DRV → SOS**; the
absent driver components are skipped exactly like Linux's `psp_hw_start` does
(`bin_desc.size_bytes == 0`). TOC is not bootloader-loaded; it is used later
by the ring-based `LOAD_TOC` command (next stage).

## Register provenance

- Mailbox dwords: `mp_13_0_2_offset.h` (the file `psp_v13_0.c` includes; shared
  by the 13.0.x family — this is the same provenance Navi33PspMailbox.hpp
  already pins): `C2PMSG_35=0x063`, `_36=0x064`, `_58=0x07a`, `_81=0x091` from
  MP0 base `0x16000` dwords → bytes `0x5818C/0x58190/0x581E8/0x58244`.
- MMHUB 3.0.2 base[0] = `0x1A000` dwords (this card's IP discovery):
  `FB_LOCATION_BASE=0x8EC` → `0x6A3B0`, `FB_LOCATION_TOP=0x8ED` → `0x6A3B4`.
- MM_INDEX/MM_DATA/MM_INDEX_HI at BAR5 bytes `0x00/0x04/0x18` (NBIF base 0),
  encoding `index = vramByte | 0x80000000`, `hi = vramByte >> 31` — the
  sequence RedNavi verified on a real RX 7800 XT. **The Navi 33 values are
  unverified until this stage runs on the card.**
- Load commands: `LOAD_KEY_DATABASE 0x80000`, `LOAD_TOS_SPL_TABLE 0x10000000`,
  `LOAD_SYSDRV 0x10000`, `LOAD_SOSDRV 0x20000` (amdgpu_psp.h).

## Risk and rollback

- This stage can hang or crash the machine (it writes to a real GPU from the
  kernel). On FramePC the RX 7600 IS the display: keep Remote Login (SSH) /
  Remote Desktop reachable, and a USB copy of the OpenCore EFI **without**
  these boot-args, before the run.
- `navi33psp_load=1` (KDB only) is the smallest possible first write and does
  not start any firmware on the card; RedNavi's equivalent KDB step on Navi 32
  was accepted without a hang.
- Rollback = remove the boot-arg and reboot (the card resets; RedNavi verified
  the chain is repeatable across reboots on Navi 32).
- A normal reboot clears everything: the sOS does not persist.

## Verification

- Host: `python -B src/navi33-bringup/tests/test_psp_load_safety.py` and the
  `navi33_psp_load_tests` CTest target (parser bounds, plan order/commands,
  pinned offsets, window encoding, status decodes) — both pass on Windows/MSVC.
- Target (Mac-day, evidence-gated order):
  1. Reboot with the new kext + `navi33psp=1` ONLY → capture the mailbox survey
     (Stage 5a). Review against the Linux captures.
  2. Reboot with `navi33psp=1 navi33psp_load=1` → KDB step. Record
     `PspLoad,*` properties + `log show --predicate 'eventMessage CONTAINS
     "PSPLOAD"'`. PASS = `StepsAccepted=1`, no hang.
  3. Reboot with `navi33psp=1 navi33psp_chain=1` → full chain. PASS =
     `ChainComplete=true`, `SecureOsAlive=true`, a plausible `SosVersion`,
     `FirmwareVerified=true` on the bring-up node.
- Do not proceed to the ring/TOC/TMR/SMU stage until the chain PASS is
  recorded; every stage stays evidence-gated.
