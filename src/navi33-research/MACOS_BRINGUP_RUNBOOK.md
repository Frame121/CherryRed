# RX 7600 (Navi 33) macOS bring-up runbook (Mac day)

Date: 2026-10-25
Status: a checklist for the session when a macOS host + RX 7600 are available. It turns the
next session into steps, not research. Every stage is evidence-gated with a PASS/FAIL/SKIP
result and a rollback; a stage must not be enabled on compile success alone.

> This runbook does not claim the RX 7600 works on macOS. "Works on macOS" is reserved for
> reproducible target-hardware evidence (see `MACOS_BRINGUP_ROADMAP.md` completion criteria).
> Do not load experimental kernel extensions on a machine you cannot afford to restart. Keep a
> known-good boot entry (the existing EFI) before any step.

## 0. Prerequisites (verify before starting)

- [ ] A macOS host with Xcode Command Line Tools (`xcrun`, `clang`, `clang++`, `make`, `codesign`, `python3`).
- [ ] An RX 7600 (`1002:7480`, subsystem `148c:2421`) installed and recognized by the BIOS.
- [ ] A **known-good boot entry** that boots macOS without the Navi 33 kext (your current EFI).
- [ ] A backup / recovery path (the machine must be recoverable if a kext hangs it).
- [ ] MacKernelSDK (from the RDNA4FB clone) and a linux-firmware checkout (for the RX 7600 blobs).

## 1. Preflight (read-only)

```sh
sh src/navi33-research/tools/macos_preflight.sh
```

Reports toolchain prerequisites. Does not install software or change security settings. A
passing preflight is not a kext build. **PASS** = toolchain present; otherwise stop.

## 2. Build the Navi 33 bring-up scaffold kext

```sh
tools/fetch-firmware-navi33.sh <path-to-linux-firmware-checkout>   # copy the 14 RX 7600 blobs
make -C src/navi33-bringup MKSDK=/path/to/RDNA4FB/MacKernelSDK all
```

- `check-firmware` fails early if any required blob is missing.
- The scaffold is **read-only / fail-closed**: no register writes, no firmware load.
- Host safety test (already run on the dev host): `python -B src/navi33-bringup/tests/test_bringup_safety.py`.
- **PASS** = `build/Navi33Bringup.kext` produced and ad-hoc signed; **FAIL** = build/link error.

## 3. Load the read-only survey / bring-up scaffold

- Place `Navi33Bringup.kext` in `EFI/OC/Kexts/` and add it to the OpenCore kext-injection list
  (mirroring how the Navi 48 kext is injected). Keep the known-good boot entry available.
- Boot with the opt-in argument: `navi33bringup=1` (the kext refuses to start otherwise).
- Capture IOKit logs (`log show --predicate 'eventMessage CONTAINS "Navi33Bringup"'`).

**PASS** = the service starts, publishes PCI identity/BAR/PCIe/ReBAR, and reports
`FirmwareLoaded=false`, `HardwareWrites=false`. **FAIL** = crash/hang → roll back to the
known-good entry.

## 4. Compare PCI/BAR/ReBAR/IP-Discovery against the Linux evidence

- Diff the captured macOS PCI identity, BAR metadata, PCIe link, and ReBAR data against
  `RX7600-Linux-Evidence/pci-resource-survey.md` and the IP Discovery artifact
  (`amdgpu_discovery.bin`, SHA-256 `4b809b32…`).
- Confirm the IP versions match the manifest (`navi33-firmware-manifest.json`): GC 11.0.2,
  SDMA 6.0.2, VCN 4.0.4, DCN 3.2.1, PSP 13.0.7, SMU 13.0.7.
- **PASS** = macOS observations agree with the Linux evidence. **FAIL/SKIP** = discrepancy →
  record it; do not proceed to register writes.

## 5. Staged hardware bring-up (one subsystem at a time)

Each stage: logs + reproducible steps + failure rollback + a clear PASS/FAIL/SKIP. Do not enable
the next stage on compile success alone. (This is where the Navi 33 kext grows beyond the
scaffold, reusing the Navi 48 stack where it is generation-agnostic.)

1. **Device match + safe service lifecycle** — probe/start/stop, opt-in, exact board match.
2. **Resource discovery without register writes** — BAR mapping read-back, VRAM size (HMM reported
   ~8176 MB on Linux).
3. **Firmware delivery + PSP/SMU startup** — load `psp_13_0_7_*`, `smu_13_0_7`, `gc_11_0_2_*`
   (gfx1102) only after the documented prerequisites; verify PSP/SMU init.
4. **VRAM/GMC/GART + interrupt handling** — memory manager, MSI/MSI-X, interrupt dispatch.
5. **Command submission + synchronization** — GFX/SDMA rings, fences, ring reset.
6. **Display-engine init + scanout (DCN 3.2.1)** — `src/dcn321/dcn321_regs.h` is **already
   generated** from the real DCN 3.2.1 headers (see its provenance line; regenerate with
   `tools/dcn321/gen_regs.py` + `--check` if the source moves). On the live card: read the DCN
   segment base from BAR5 and confirm `seg[BASE_IDX] + offset` (BASE_IDX 2) is inside the window
   before any register access, then load `dcn_3_2_1_dmcub.bin` and bring up clock → link → hub →
   OTG → DML → scanout. **First visible output on the RX 7600.**
7. **Acceleration API + Metal** — expose the user-client ABI to the Metal bundle
   (`tools/native/`), AIR→SPIR-V (`src/xlat12/`), Mesa RADV (`mesa-patches/`); run Metal workloads.

## 6. Rollback

At any stage: stop the kext, remove it from the OpenCore injection list, and boot the known-good
entry. The `autoarm`-style fail-safe pattern (a kill-switch file, e.g. `navi33-off`) is the model
for leaving a CPU-rendered desktop if a step fails.

## Evidence to record per stage

- macOS version, Xcode/CLT version, SDK path, arch.
- The exact build command and the kext UUID (`dwarfdump --uuid`).
- IOKit / unified-log excerpts for the stage.
- The PASS/FAIL/SKIP result and any discrepancy vs. the Linux evidence.
