# Mac-day Stage 3 pack (2026-10-31)

Executed on the target machine itself (macOS 26.7.1 Tahoe x86_64, RX 7600
`1002:7480` subsystem `148c:2421` rev `0x00cf`). Stage 0–2 of
`src/navi33-research/MACOS_BRINGUP_RUNBOOK.md` are complete; this directory
holds what Stage 3 needs beyond the built kext.

## Contents

- `stage3_apply.sh` — the single sudo step: mounts the ESP, backs up the whole
  EFI folder twice (local + a writable USB volume when present), installs
  `Navi33Bringup.kext` into `EFI/OC/Kexts/`, appends `navi33bringup=1` to the
  boot-args, adds the Kernel/Add entry (idempotent), lints config.plist, and
  supports `--rollback`.
- `stage3_verify_after_reboot.sh` — run WITHOUT sudo after rebooting: captures
  the `Navi33Bringup` IORegistry node and kernel-log excerpts, runs the Stage 4
  comparison against the Linux evidence, and re-runs the Metal probe.
- `STAGE3_HANDOFF.md` — the operator runbook for the above two scripts
  (Thai/English).
- `evidence/` — userspace facts recorded before Stage 3:
  - `STAGE4_PRE_COMPARISON_2026-10-31.md` — identity/revision/subsystem/PCIe
    Gen4 x8/VRAM agree with `RX7600-Linux-Evidence/pci-resource-survey.md`;
    BAR4/BAR5/BAR6 match byte-for-byte; the two 64-bit prefetchable BARs
    differ exactly as ReBAR-programmable BARs are expected to (macOS BAR0
    256 MiB / BAR2 2 MiB vs Linux 8 GiB / 256 MiB).
  - `userspace-pci-capture-2026-10-31.txt`, `userspace-gpu-node-2026-10-31.txt`
    — raw ioreg captures backing the comparison.

## Build provenance

The kext this pack installs was built on the Tahoe target from exactly the
`src/navi33-bringup/` sources in this tree (see the two commit history
entries: the refinements record and the first-macOS-build fixes), with the 14
hash-pinned firmware blobs from `FIRMWARE-PROVENANCE.md` embedded. Result:
x86_64, min macOS 11.0, ad-hoc signed, UUID
`6F34DC52-AE44-3656-8562-23B8D9C1CFEF`. The machine-local build workspace
(including the built kext and the ESP backups) lives outside the repository.

## Status boundary

Stage 3 had not been executed at the time of this commit: the ESP cannot be
mounted without sudo, so the injection step waits for operator action. No
kext was installed, activated, or loaded; no EFI/config/NVRAM/SIP/AMFI change;
no reboot; no MMIO/VRAM access. RX 7600 hardware acceleration remains NOT
achieved and is not claimed: Stages 5.3–5.7 (PSP/SMU startup, VRAM/GTT,
command submission, DCN 3.2.1 scanout, IOAccelerator/Metal) are unimplemented,
no IOAccelerator service exists for this GPU, and `MTLCopyAllDevices()` is
empty on this host.

## Machine-specific facts (2026-10-31, Tahoe target)

- OpenCore does NOT boot from the internal ESP (`disk0s1` "NO NAME" holds the
  Windows boot files only). The live OpenCore boots from the FAT32 USB stick
  mounted at `/Volumes/1401`, OC root `/Volumes/1401/EFI/OC`.
- That volume is user-writable, so the install step needs NO sudo on this
  machine — only the reboot does. `stage3_apply.sh` accepts the OC root as an
  explicit argument (`sudo sh stage3_apply.sh /Volumes/1401/EFI/OC`) but the
  identical steps were executed directly on 2026-10-31:
  backups `EFI_BACKUP_CHERRYRED_STAGE3_20261031-005806` (USB) and
  `stage3/EFI-backup-20261031-005806` (local), kext installed to
  `Kexts/Navi33Bringup.kext` (ad-hoc signature verified on the copy),
  boot-args extended to
  `-v debug=0x100 keepsyms=1 revpatch=sbvmm -radvesa navi33bringup=1`,
  `Kernel/Add` gained the Navi33Bringup entry (Enabled), `plutil -lint` OK.
- Recovery property: with the USB stick removed the firmware falls back to
  the internal Windows boot; re-plugging the stick restores OpenCore.
- `Misc.Security.SecureBootModel` is Disabled and SIP has kext signing
  disabled; ad-hoc kext injection matches how Lilu 1.7.3/WhateverGreen 1.7.2
  already load on this exact setup.
