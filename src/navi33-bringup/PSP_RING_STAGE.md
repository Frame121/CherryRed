# PSP GPCOM ring + LOAD_TOC + SETUP_TMR stage (navi33psp_ring=1)

Status: **implemented + host-tested (2026-10-31); NOT yet run on the RX 7600.**
This stage completes M2a on the Navi 33 track: the secure-OS command channel
through which every later firmware load travels (SMU via `LOAD_IP_FW`, DMCUB,
GC PFP/ME/MEC/RLC). It is the direct Navi 33 port of the flow RedNavi verified
on a real RX 7800 XT (PSP 13.0.10); the Navi 33 values are UNVERIFIED until
this stage runs on the card.

## Gates

- `navi33bringup=1` + `navi33psp=1` (existing opt-ins; the read-only mailbox
  survey still runs first and records the pre-ring state).
- `navi33psp_ring=1` — ring create → LOAD_TOC → SETUP_TMR.
- Optional `navi33psp_tmr=N`: **1** (default) = VRAM top − 512 MiB (the Linux
  placement RedNavi confirmed on the 16 GiB card); **2** = the 32 MiB
  diagnostic placement inside the CPU-visible BAR window (RedNavi's first
  test).

The stage REQUIRES the secure OS to be alive (`C2PMSG_81 != 0`) and aborts
with evidence otherwise — in practice it runs in the same boot as
`navi33psp_chain=1`.

## What it does

1. Read-only PCI command check; maps BAR5 and verifies the window covers the
   pinned register set (same rule as the load stage).
2. Requires `C2PMSG_81 != 0` (sOS alive); aborts otherwise.
3. Reads live FB_LOCATION → MC base and VRAM size (published).
4. **Ring create** (`psp_v13_0_ring_create`): waits for
   `(C2PMSG_64 & 0x8000FFFF) == 0x80000000`, zeroes the 4 KiB ring at
   VRAM +0x40200000, writes `C2PMSG_69/70/71` = ring MC lo/hi/size, writes
   `C2PMSG_64 = PSP_RING_TYPE__KM << 16`, sleeps 20 ms, waits again. RedNavi's
   real response was `0x80C20000` (bit 31 + status 0).
5. **LOAD_TOC**: stages the TOC bin of the embedded `psp_13_0_7_sos.bin`
   (2,304 bytes, see PSP_LOAD_STAGE.md) at VRAM +0x40230000 with dword
   read-back verification, then submits one 64-byte frame: cmd buffer MC at
   frame +0/+4, fence buffer MC at +12/+16, fence value at +20, write pointer
   `(wptr + 16) % 0x400` in `C2PMSG_67`. Command buffer: `cmd_id` at +8,
   args `{toc_mc_lo, toc_mc_hi, toc_size}` at +28, response read from +864:
   `resp[0] = status`, `resp[4] = tmr_size`. RedNavi's Navi 32 answer:
   status 0, `tmr_size = 0xA700000` (167 MiB).
6. **SETUP_TMR**: only with a usable `tmr_size` and a valid placement
   (`tmrOffsetForMode`, no open-coded address). Args
   `{tmr_mc_lo, tmr_mc_hi, tmr_size, flags = 2 (virt_phy_addr),
   tmr_pa_lo, tmr_pa_hi}` where the CPU PA = BAR0 physical + the same offset
   (`amdgpu_gmc_vram_pa`). RedNavi's answer: status 0.
7. Publishes every intermediate value (`RingReadyBefore`, `RingMboxBefore/After`,
   `RingMc`, `LoadTocFenceOk/Status`, `TmrSize`, `TmrMc/TmrOffset/TmrMode`,
   `SetupTmrFenceOk/Status`) and a final stage string.

The writes are confined to C2PMSG_64/67/69/70/71 plus the shared MM window;
PCI config space is read, never written.

## TMR placement — what is known and what is not

- Navi 32 (RedNavi, verified): Linux reserved the TMR at
  `0x83e0000000` = VRAM top − 512 MiB; SETUP_TMR at that address accepted and
  the DMCUB booted from it. The 32 MiB diagnostic placement was accepted but
  `LOAD_IP_FW`'s `fw_addr` fell outside it, so the DMCUB did not start.
- Navi 33: **unknown** — the captured Linux evidence does not include the
  card's own `reserve ... for PSP TMR` dmesg line, so mode 1 uses the same
  formula as UNVERIFIED evidence to be confirmed. If mode 1's TMR is rejected
  or a later `LOAD_IP_FW` address falls outside it, capture the RX 7600's
  dmesg line on Linux and set the placement from that (mode selector exists).

## Risk and rollback

- Same risk class as the load stage: writes to a real GPU from the kernel. The
  ring/TOC/TMR writes were non-destructive on Navi 32 and a normal reboot
  clears them. Keep SSH/RDP reachable and a clean EFI USB at hand.
- Rollback = remove the boot-arg and reboot.

## Verification

- Host: `python -B src/navi33-bringup/tests/test_psp_ring_safety.py` and the
  `navi33_psp_ring_tests` CTest target (pinned mailbox byte offsets, ring
  geometry, frame layout, wptr arithmetic/wrap, response decode, TMR placement
  rules incl. the Navi 32 reference value) — pass on Windows/MSVC.
- Target (after the chain stage PASS): reboot with
  `navi33psp=1 navi33psp_chain=1 navi33psp_ring=1`. PASS =
  `RingCreated=true`, `LoadTocFenceOk=true` with `LoadTocStatus=0` and a
  plausible `TmrSize`, `SetupTmrFenceOk=true` with `SetupTmrStatus=0`, no hang.
- Next stages (SMU 13.0.7 via `LOAD_IP_FW`, GC 11.0.2 firmware, DMCUB) stay
  evidence-gated behind this PASS.
