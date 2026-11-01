# PSP/SMU mailbox survey stage (navi33psp=1)

Status: implemented and host-tested (2026-10-31); NOT yet run on the RX 7600.
This stage is the first BAR-window access of the bring-up. It stays inside the
scaffold's zero-write contract and adds live PSP/SMU state to the Stage 4
kernel-survey evidence.

## What it does

On top of the existing read-only survey, `navi33psp=1` (cumulative with
`navi33bringup=1`) makes the kext:

1. Read the PCI command register. If PCI memory decode is OFF, it aborts
   rather than enabling it — the kext never writes config or MMIO space.
2. Map the MMIO register window (**BAR5**, 1 MiB on this card) and publish
   `Navi33Bringup,PspStage,RegisterBAR*`.
3. Wait, bounded at 2 s (the bound RedNavi verified on Navi 32), for
   `MP0_C2PMSG_33` bit 31 (IFWI init done), publishing the observed wait.
4. Read the pinned mailbox register set and publish it as
   `Navi33Bringup,PspMailboxRegs`, with decoded status properties.
5. Publish `Navi33Bringup,PspStage,HardwareWrites=false` and
   `RegisterWrites=false` — reads only, no firmware load.

## Register provenance

- BAR index 5 is the MMIO register window (RedNavi maps the same BAR on Navi 32).
- MP0 (hw_id 255) and MP1 (hw_id 1) instance-0 `base[0]` = `0x16000` dwords
  come from THIS card's captured IP Discovery table
  (`tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin`,
  SHA-256 `4b809b32…`). Discovery bases are dword units: byte offset =
  `(base + dword) * 4`. The 5th base (`0x243FC00` dwords ≈ 578 MiB) is outside
  the 1 MiB window and is skipped, never read through a hole.
- Mailbox dword offsets are the shared PSP/SMU 13 register file (Linux
  `mp_13_0_0_offset.h`, `smu_v13_0.c`); PSP 13.0.7 / SMU 13.0.7 use the same
  file. The decode bits mirror RedNavi's M1.5 stage verified on a real
  RX 7800 XT. **The Navi 33 VALUES are unverified until this stage runs.**

| Register | Decode |
|---|---|
| `MP0_C2PMSG_33` | bit 31: IFWI init done |
| `MP0_C2PMSG_35` | bit 31: bootloader ready |
| `MP0_C2PMSG_81` | nonzero: secure OS alive |
| `MP1_C2PMSG_90` | SMU response: 0 busy, 1 = OK, 0xFF = refused |

## Deliberately out of scope (next stages)

- Reading the LIVE IP Discovery table from VRAM: RedNavi does that through the
  MM_INDEX/MM_DATA window, which requires writing the index register. That
  breaks the zero-write contract, so this stage uses the captured table's
  bases instead. Enabling index/data reads is a separate, explicitly
  authorized stage.
- Anything that writes: PSP commands, SMU messages, firmware loading. Those
  are Stage 5.3 proper and stay behind their own gates.

**Implemented after this stage:** the first write stage exists — see
`PSP_LOAD_STAGE.md` (`navi33psp_load=1` = KDB only, `navi33psp_chain=1` = the
full KDB → SPL → SYS_DRV → SOS chain). It runs AFTER this survey on the same
boot so the pre-load mailbox state is recorded first.

## Build and install

- CI (`.github/workflows/build-kext.yml`) builds the kext on a macOS runner
  from the pinned linux-firmware commit, hash-verifies the 14 blobs against
  `FIRMWARE-PROVENANCE.md`, runs the host suites, and uploads
  `Navi33Bringup.kext` as an artifact.
- Install (Windows side, while the machine is booted to Windows): copy the new
  `Navi33Bringup.kext` over `E:\EFI\OC\Kexts\Navi33Bringup.kext` after backing
  the old one up. `config.plist` already carries `navi33bringup=1
  navi33psp=1` and the Kernel/Add entry.
- Boot-arg absence is fail-closed both ways: without `navi33bringup=1` the
  kext refuses to start; with only `navi33bringup=1` it publishes
  `Navi33Bringup,PspStage = disabled` and behaves exactly like the Stage 3
  scaffold.

## Verification

- Host: `python -B src/navi33-bringup/tests/test_bringup_safety.py`,
  `python -B src/navi33-bringup/tests/test_psp_survey_safety.py`, and the
  `navi33_psp_mailbox_tests` CTest target (offsets, bounds, decodes, table
  pins) — all pass on Windows/MSVC.
- Target: after one reboot into macOS, the Stage 3 verify script
  (`stage3_verify_after_reboot.sh`) captures the node including the new
  `PspStage` / `PspMailboxRegs` properties. Record the readback in
  `macday-stage3/evidence/` and compare the SMU/PSP status against the Linux
  `smu_mailbox_*.txt` captures before ANY later write stage is considered.
