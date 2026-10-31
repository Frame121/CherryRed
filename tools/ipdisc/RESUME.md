# Recovery Checkpoint

- **CHECKPOINT: 2026-10-31 (session 4) — METAL-LAYER SKELETON WRITTEN AND
  INSTALLED: CLIENT CONTRACTS (C4) + SELF-GATING ACCELERATOR (C6 DIRECTION).**
  Per the operator's call to build the Metal layer ahead of the hardware
  stages, the parts of C4/C6 that do not depend on hardware were implemented
  as pure host-tested contracts plus a fail-closed kernel service, all in the
  bring-up kext:
  - `Navi33ClientContracts.hpp`: the versioned user-client ABI surface
    (policy-driven buffer/alignment/budget/handle limits — synthetic in
    tests until C2 derives real values), a fixed-capacity generation-tagged
    handle table (stale handles cannot address recycled slots), and the
    compute readiness gate, which deliberately requires only PCI transport +
    firmware + memory + queue submission (headless compute; display,
    IOAccelerator and Metal are C5/C6 milestones, not prerequisites).
  - `Navi33Accelerator` (second class/personality in the same kext): attaches
    to the Navi33Bringup node, evaluates the gate from published evidence
    properties, publishes exactly why Metal dispatch is unavailable, and
    performs zero hardware access in every state (source-level safety test
    enforces this). Its evidence property names —
    `Navi33Bringup,FirmwareVerified` / `MemoryVerified` / `QueueVerified` —
    are the contract Stages 5.3–5.5 must publish for the gate to ever open.
  - `Navi33IntegrationContracts.hpp` switched to `<stdint.h>` (freestanding-
    safe for the -nostdinc kernel TU); host-only users add `<cstdint>`.
  Host verification: client-contract unit tests + accelerator safety test +
  CTest **25/25** (Release and Debug, MSVC). CI green; the kext with both
  personalities (`Navi33Bringup`, `Navi33Accelerator`) was published to
  `kext-builds` and installed to `E:\EFI\OC\Kexts\Navi33Bringup.kext`
  (previous kext preserved as
  `EFI_BACKUP_KEXT_BEFORE_PSPSTAGE_20261031/Kexts/Navi33Bringup.psp-stage.kext`).
  **Honest boundary: Metal dispatch is still unavailable (0 devices) — the
  gate is closed at the firmware layer by design until Stage 5.3 runs on the
  card. On the next macOS boot the accelerator node will appear with
  `State=blocked`, `BlockedReason=gate-closed-firmware` — that is the
  correct, verified behavior.** The PSP mailbox readback (same reboot) is
  what feeds Stage 5.3.

- **CHECKPOINT: 2026-10-31 (session 3) — PSP-STAGE KEXT BUILT ON CI AND
  INSTALLED INTO THE LIVE OPENCORE EFI; ONE REBOOT RUNS STAGE 3 + PSP SURVEY
  TOGETHER.** The CI pipeline (`.github/workflows/build-kext.yml`) now builds
  `Navi33Bringup.kext` end-to-end on a macOS runner: pinned linux-firmware
  fetch + sha256 verification of all 14 blobs, host safety tests, the kext
  build (the C++ kmod runtime ships in `Library/universal` of the fresh
  acidanthera MacKernelSDK — the Makefile now accepts either layout), the
  full research-core CTest suite on Apple clang (which required pinning
  `cxx_std_17` on the SDK-less `ipdiscovery_bounds_tests` target), and a
  publish step that commits the zip to the `kext-builds` branch
  (`kext-latest/Navi33Bringup.kext.zip`) so the Windows side can pull it
  without a token. The CI-built kext (with the `navi33psp=1` mailbox survey
  compiled in — verified by its log strings) was installed to
  `E:\EFI\OC\Kexts\Navi33Bringup.kext` (previous kext and config backed up in
  `E:\EFI_BACKUP_KEXT_BEFORE_PSPSTAGE_20261031\`); the EFI config already
  carries `navi33bringup=1 navi33psp=1`. **Next action: reboot into macOS
  once and run
  `sh ~/Developer/cherryred-macday-20261030/stage3/stage3_verify_after_reboot.sh`
  — it proves Stage 3 injection AND captures the PSP/SMU mailbox readback in
  the same boot.** The mailbox VALUES stay UNVERIFIED until then; no write
  stage may be started from them. Metal/acceleration remain unimplemented.
  CI reached fully green at `600d4bb` (kext build + publish + full research
  CTest on Apple clang); the deliverable kext is refreshed on the
  `kext-builds` branch on every green build.

- **CHECKPOINT: 2026-10-31 (session 2) — PSP/SMU MAILBOX SURVEY STAGE
  IMPLEMENTED; CI KEXT BUILD WIRED UP.** The next bring-up stage
  (`navi33psp=1`, cumulative with `navi33bringup=1`) is implemented and
  host-tested: the kext maps the MMIO register window (BAR5, 1 MiB) and
  READS the PSP/SMU mailbox registers (MP0/MP1 `base[0]` `0x16000` dwords
  from the card's captured IP Discovery table, SHA `4b809b32…`; offsets from
  the shared PSP/SMU 13 register file; decodes mirror RedNavi's verified
  Navi 32 M1.5). Zero-write contract holds: the PCI command register is only
  read (memory decode is refused rather than enabled), every read is
  bounds-checked against the mapped BAR, the IFWI wait is bounded at 2 s,
  and the live IP-Discovery read (which would need MM_INDEX writes) is
  deliberately deferred. Files: `Navi33PspMailbox.hpp` (pure, host-tested),
  `Navi33PspSurvey.{hpp,cpp}` (the only translation unit allowed to map a
  BAR, under its own contract test), `Navi33Bringup` gains
  `pspStageEnabled()`; `Makefile` builds the new TU. Host verification:
  bring-up safety test + new PSP survey safety test + CTest **23/23** on
  MSVC. CI (`.github/workflows/build-kext.yml`) now builds
  `Navi33Bringup.kext` on a macOS runner from the pinned linux-firmware
  commit, hash-verifying all 14 blobs (fetch script pins upgraded from
  UNKNOWN to the recorded `FIRMWARE-PROVENANCE.md` values; mismatches are
  hard errors). `config.plist` on the OpenCore USB already carries
  `navi33psp=1`. Design notes: `src/navi33-bringup/PSP_MAILBOX_STAGE.md`.
  The Navi 33 mailbox VALUES remain UNVERIFIED until one reboot into macOS
  collects the readback; Stage 3 injection verification is still pending the
  same reboot.

- **CHECKPOINT: 2026-10-31 (Windows-side integration) — MAC-DAY BUNDLE MERGED;
  HOST SUITES RE-RUN ON MSVC; TWO COMMITTED DEFECTS FIXED.** The Mac-day git
  bundle (7 commits on `research/rx7600-linux-evidence-2026-10-23`, head
  `d9d932f`) was fetched from the USB handoff package and the local branch was
  fast-forwarded to it; the superseded pre-Mac-day working-tree draft was
  preserved as a stash. The RedNavi multi-target probe work (`gpu_target.h`:
  Navi 31/32/33 identity table, Navi 32-only register path) was committed on
  its own branch (`contrib/navi33-pci-profile`) after its host test passed
  under MSVC. Verification of the merged tree found two defects committed on
  the Mac-day side, fixed here: (1)
  `src/navi33-research/CMakeLists.txt` had an unterminated `if/else` block, a
  duplicate `add_test`, and a flags list omitting six pre-existing targets,
  which tripped the no-exceptions compile guard; (2)
  `src/navi33-survey/tests/test_survey_safety.py` still asserted the
  pre-rewrite opt-in form. After the fixes: MSVC Debug build clean, CTest
  **20/20 PASS**. Two Python suites are Windows-host-incompatible by
  environment, not by regression: the symlink rejection case needs elevated
  symlink privileges, and the Linux inventory tests create sysfs-like paths
  containing colons, which Windows forbids; both passed on the macOS target
  (recorded 2026-10-30). The target EFI carries boot-arg `navi33psp=1`, which
  no committed kext source reads — inert, left untouched. **Stage 3 reboot
  verification on the Tahoe target remains the next pending action; Metal and
  acceleration remain unimplemented (Stages 5.3–5.7) and are not claimed.**

- **LATEST CHECKPOINT: 2026-10-31 — MAC-DAY STAGES 0–2 COMPLETE ON TARGET;
  Navi33Bringup.kext BUILT+SIGNED; STAGE 3 PACKED AND WAITING FOR ONE SUDO
  COMMAND.** The bring-up scaffold was compiled for the first time on the
  Tahoe target (`~/Developer/cherryred-macday-20261030/`; two required source
  fixes recorded as patches there: `<stdint.h>`/`uint16_t` in
  `Navi33PciMatch.hpp`, `UInt8` offset in `surveyPcieLink`). OSBundleLibraries
  validated against the live BKC (pexpert no longer exists on Tahoe; declared
  set matches loaded Lilu/WEG practice). Firmware 14/14 hash pins re-verified.
  Full host suites all pass. Stage 3 (OpenCore injection + `navi33bringup=1`)
  is fully prepared in `stage3/` (`stage3_apply.sh` with double ESP backup and
  `--rollback`; `stage3_verify_after_reboot.sh` runs the Stage 4 comparison
  without sudo) and is BLOCKED only on the user's sudo password — the ESP
  cannot be mounted as user. Stage 4 pre-comparison (userspace ioreg vs Linux
  evidence) is recorded: identity/link/VRAM and the three non-ReBAR BARs match
  byte-for-byte; the two 64-bit prefetchable BARs differ exactly as
  ReBAR-programmable BARs are expected to. NEXT ACTION after reboot:
  `sh ~/Developer/cherryred-macday-20261030/stage3/stage3_verify_after_reboot.sh`
  then record the kernel-survey evidence in the repo. Do not claim Metal or
  acceleration: Stages 5.3–5.7 remain unimplemented and no IOAccelerator
  exists. Previous checkpoint (2026-10-30) below.

- **CHECKPOINT: 2026-10-30 — BOARD FIRMWARE MANIFEST + VBIOS IDENTITY
  INTAKE COMPLETE; SESSION RUNS DIRECTLY ON THE TAHOE TARGET.** This session
  executes on the target Mac itself (macOS 26.7.1 Tahoe x86_64, RX 7600
  enumerated), so no Remote Desktop hop was required. Work completed:
  1. Full baseline re-validated on the target: host C++ suites 35+32+21+16+18+
     18+10+22 checks, IPD bounds (8 fixtures + 512 malformed), Navi48
     isolation 4/4, DriverKit probe contract 4/4; Python suites 31+9+8+7;
     survey kext rebuilt (codesign/plutil OK, NOT installed/loaded);
     DriverKit probe rebuilt (build-only); Metal probes rebuilt — generic
     runtime exits 2 (`MTLCreateSystemDefaultDevice()` nil) and the exact
     RX 7600 probe exits 2 (`MTLCopyAllDevices()` empty). Metal dispatch
     remains BLOCKED, reported as such.
  2. Strict re-parse of the captured IP Discovery artifact: hash matches
     `4b809b32…b75e6c89`; 39 records; GC 11.0.2, MP0/MP1 13.0.7, SDMA0/1
     6.0.2, UMC 8.11.0, MMHUB 3.0.2, HDP 6.0.1, OSSSYS 6.0.2, DMU 3.2.1,
     SMUIO 13.0.8; the volume original was re-hashed and is byte-identical.
  3. The Cobalt64 evidence package on the mounted volume contained
     board-attributed records not yet in the repo. Imported byte-preserving
     copies: `evidence/rx7600-navi33-firmware-manifest-cobalt64.json` (14
     required firmware files, gfx_target gfx1102, linux-firmware 20260916-1,
     kernel clone `64a90897`) and a derived
     `evidence/rx7600-vbios-identity-2026-10-23.json` recording ATOM BIOS
     `113-EXT86626-001`, ver `022.003.001.027.000001`, 2023/04/25 from the
     board's own Linux boot journal, hash-bound to the journal
     (`db85fb97…c9818a`), plus runtime corroboration (8176M GDDR6 128-bit,
     SMU OK, DCN 3.2.384, DMUB 0x07003300, VCN ENC 1.24/DEC 9 rev 34, SE 2 /
     SH 2 / CU 8 / 32 active CUs). Identity strings only — NOT a VBIOS dump.
  4. New `verify_board_firmware_manifest.py` + 17 synthetic tests
     (`test_board_firmware_manifest.py`): cross-checks the manifest's firmware
     filename prefixes against the parsed artifact versions, requires role
     coverage, keeps HWIDs 282/286 explicitly unresolved, checks the VBIOS
     record's exact PCI identity and `authentication: NONE`, and binds the
     source journal hash. 17/17 pass; real-evidence run PASS with journal
     binding. Evidence doc:
     `evidence/RX7600_BOARD_FIRMWARE_EVIDENCE_2026-10-30.md`.
  - **Effect on the C0 gate:** the "board-attributed IP Discovery + VBIOS
    record" prerequisite is materially advanced (both artifacts now exist and
    cross-check). Still open: VBIOS byte payload, independent authentication
    of user-maintained records, per-blob hash/license review. No firmware blob
    was copied into the repo. No hardware access, kext load, firmware load,
    MMIO/VRAM, EFI/SIP/AMFI change, or reboot was performed. RX 7600 hardware
    acceleration remains NOT achieved and is not claimed.
- **Next gate:** independently authenticate the board records (e.g. a fresh
  authorized Linux capture), acquire and hash the actual firmware blobs and
  the VBIOS image under a reviewed license path, then begin the separately
  reviewed GFX11 (C1) implementation with a tested recovery path. Do not
  attempt hardware init, ID spoofing, or kext activation without them.

- **LATEST CHECKPOINT: 2026-10-21 — PHASE 25 BOARD PRESENCE VERIFIED; TWO
  UNCOMMITTED SOURCE FIXES RESTORED AND HEAD REGRESSIONS CLEARED.** `1002:7480`
  moved from `UNVERIFIED (hardware presence unknown)` to **presence and PCI
  identity VERIFIED** via a read-only Windows device-model query, not via
  substituted Linux tools:
  `PCI\VEN_1002&DEV_7480&SUBSYS_2421148C&REV_CF\6&EB99E4&0&00000009`,
  class Display, status OK, driver `32.0.31041.1004`. Established: device
  present, vendor `1002`, device `7480`, subsystem `148C:2421`, revision `CF`,
  driver bound. Not established: IP Discovery records/versions, firmware
  requests, loaded/running firmware, VBIOS identity, VRAM size, macOS anything.
  Subsystem vendor **brand name not asserted** — `pci.ids` is absent locally
  and a name lookup needs authorized network access. PCI config space, ROM BAR,
  MMIO, VRAM, firmware interface, and driver state were **not** accessed; no
  Linux command or sysfs path was simulated. An offline read-only search of the
  driver store found **no** pre-existing VBIOS/atom ROM image, so no local
  source of the embedded IP Discovery table exists; a ROM BAR read or a
  third-party dump tool would both need separate explicit authorization and
  **neither was attempted**.
- **Common root cause behind two HEAD regressions: source fixes from Phases
  14–24 were never committed.** At a clean `8eb0704`, two committed tests
  failed because the source changes they assert were left unstaged:
  1. `test_navi48_isolation.py` failed 1 of 4 —
     `test_start_revalidates_identity_before_hardware_setup`.
     `Navi48Bringup::start()` had **no**
     `configRead32(kIOPCIConfigVendorID)` re-check and went straight from
     `if (!pciDevice)` to `setMemoryEnable(true)` → `latchPciBars()` →
     `mapRegisters()`; the only identity read in the file was inside `probe()`.
     The guard the **Phase 14 checkpoint reported as added was never committed
     and is not present at HEAD**. Restored immediately before
     `setMemoryEnable`, matching `probe()`'s expression and types.
  2. `ipdiscovery_bounds_tests` failed 4 of 7 in **both** Debug and Release.
     `ipdiscovery.cpp/.hpp` last changed in `dc128be`, so the bounds work the
     **Phase 21 checkpoint described as passing was never committed either**.
     `regByteOffset()` computed `(base + dwordOffset) * 4` in `uint32_t` with
     no overflow check, returning a wrapped, valid-looking MMIO byte offset on
     overflow, and `ipAt()` walked records against the whole caller buffer
     rather than the declared `table_info` size, so a truncated table let
     records run into the bytes after it. Input is an untrusted injected ROM
     image, so both matter.
  Phase 21's own record lists `Navi48Bringup.cpp` as an *uncommitted* tracked
  working change at `d1f7bad` with "nothing staged", and `8eb0704` is
  "39 files changed, 9794 insertions(+)" with **zero deletions** — it captured
  only the new `src/navi33-research/` and `tools/ipdisc/` files. The test files
  landed without the source changes they assert. **Repository-hygiene defect
  alongside the code defects: stage source and test changes together.**
- **Fixes applied:** (a) the `start()` identity guard, placed before
  `setMemoryEnable`; (b) parser bounds — new `tableEnd` member set from
  `ipd + ipdSize` in `validateBinary()`, reset in `init()`, an
  `ipd > size || ipdSize > size - ipd` check, `ipAt()` requiring the 8-byte
  record header **and** `8 + 4 * nbase` base array to lie within `tableEnd`,
  and `regByteOffset()` rejecting addition and multiplication overflow **before**
  writing `byteOffset`. Thresholds keep `dwords == 0x3FFFFFFF` accepted
  (`byteOffset == 0xFFFFFFFC`); the rejection ceiling is ~4 GiB of MMIO byte
  offset, far above this hardware's BAR, so no legitimate request changes
  behavior. **No `1002:7480` match, no Navi33 change, no plist change, no IP
  version dispatch, and no firmware path change.**
- **Verification:** MSVC x64 Debug and Release rebuilds exit 0. `ctest` **4/4
  passed in each configuration** (0.22s each; before the parser fix it was 3/4
  failing in both). Navi48 isolation 4 passed (`0.007s`; was 1 failure). IP
  Discovery Python suite 30 passed (`1.577s`). `git diff --check` passed.
  **`Navi48Bringup.cpp` is still not compiled here** — it needs the macOS 11
  SDK, `xcrun`, and `MacKernelSDK`, absent on this host, and is outside the
  portable CMake target, so the `start()` guard's verification remains a static
  source-contract test only. `ipdiscovery.cpp` *is* compiled and covered by
  `ipdiscovery_bounds_tests`.
- **Still unverified:** every IP Discovery and firmware row in
  `RX7600_NAVI33_EVIDENCE.md` remains **UNVERIFIED**; macOS compile/link, kernel
  ABI, RX 7600 initialization, display acceleration, and Metal remain
  unverified. **Phase 4 dispatch implementation remains not justified.**
- **Next gate:** a provenance-documented IP Discovery artifact attributable to
  this board (a VBIOS image from the board is an acceptable equivalent source
  because the table is embedded in the atom firmware image) supplied or
  acquired under separately stated authorization. Do not probe ROM BAR or
  install a dump tool without that authorization, and do not infer IP records
  from generic Linux branches or PCI names.

- **LATEST CHECKPOINT: 2026-10-21 — PHASE 3 HARDWARE EVIDENCE BLOCKED BY
  WINDOWS HOST; BOARD NOT CHECKED.** Added
  `RX7600_NAVI33_EVIDENCE.md`. The Windows availability check
  `Get-Command lspci,dmesg,amdgpu-discover,atomtree -ErrorAction
  SilentlyContinue` returned no command entries. No Linux-only hardware
  commands or sysfs reads were attempted.
  1002:7480: UNVERIFIED — hardware presence unknown (non-Linux host; lspci/dmesg/amdgpu tools unavailable)
  All board-specific IP and firmware rows remain `UNVERIFIED`;
  no firmware request was observed in logs and no loaded/running firmware was
  observed because those logs/interfaces were unavailable. Generic Linux
  expectations remain conditional source inferences only. Phase 4 condition
  (board attribution) is not met.
- Phase 2 blockers: 2 — listed below as open items:
  - [OPEN] “No board-attributed evidence exists. The spec itself says no
    provenance-documented IP Discovery artifact, subsystem/VBIOS attribution,
    or physical-card report was available (lines 18-23 and 100-112). Thus
    every version and firmware example remains conditional and unverified for
    `1002:7480`; Phase 4 dispatch implementation is not justified.” — concerns
    spec sections “Scope and conclusion” and “Evidence gaps and next milestone”.
  - [OPEN] “This Windows host cannot perform the requested Linux hardware
    check. `lspci`, `dmesg`, and `/sys` capture must not be simulated or
    inferred from missing Windows tools. The device's absence is not
    established. Phase 3 requires an authorized, suitable Linux host with the
    target present and read-only capture access, or an already-existing
    provenance-documented capture supplied for offline review.” — concerns
    spec sections “Scope and conclusion” and “Evidence gaps and next milestone”.
- Pre-existing lint debt: 14 trailing-space lines across 7 older Markdown docs — intentionally preserved, not a regression from this change.
- **Next gate:** Requires an authorized Linux read-only capture or existing
  provenance-documented RX 7600 evidence. Do not simulate records, infer
  hardware absence, or implement dispatch without board evidence.

- **LATEST CHECKPOINT: 2026-10-21 — SPEC REVIEW COMPLETE; TARGET EVIDENCE
  GATE BLOCKED BY HOST ENVIRONMENT.** Added
  `RX7600_NAVI33_SPEC_REVIEW.md` as a review-only record; the dispatch spec
  itself was not edited. Pinned source review confirms its central limitation:
  generic Linux dispatch branches do not attribute IP versions or firmware to
  `1002:7480`. Review findings flag missing fine-grained citations for some
  dispatch cases, the IH 6.1.x switch, PSP TA helper, GFX/RLC conditional
  details, explicit firmware evidence categories, and source/kernel/blob/doc
  licensing distinctions. No code or tests changed.
- **Phase 3 limitation:** This host is Windows; no Linux `lspci`, `dmesg`, or
  `/sys` hardware capture was attempted. RX 7600 presence/absence is unknown,
  not absent. No evidence file should label the board absent. Required next
  input is an authorized Linux read-only capture or existing
  provenance-documented board evidence. Do not implement dispatch without it.

- **LATEST CHECKPOINT: 2026-10-21 — PINNED LINUX DISPATCH SPECIFICATION
  RECORDED; BOARD INVENTORY STILL UNRESOLVED.** Added
  `RX7600_NAVI33_IP_DISPATCH_SPEC.md` after public, pinned-source review of
  Linux commit `d0c647a6aae2ce577372594a176c82733e1ed267`. It records the
  generic version-based GC/GFX, GMC, IH, PSP, SMU, SDMA, VCN, display and MES
  routes and firmware request conditions as conditional paths only. Linux
  source does not map `1002:7480` to a complete Navi33 IP record set; no
  board-attributed artifact, subsystem/VBIOS attribution, exact firmware
  selection, firmware binary provenance/license, or macOS suitability was
  established. No Linux source was copied. No code or parser tests were run;
  this was a documentation-only research milestone.
- **Next milestone:** Offline strict-parse an already-existing, user-supplied
  and provenance-documented RX 7600 artifact, then cross-check only its
  observed IP versions against the pinned commit. Do not probe hardware to
  acquire missing evidence. Navi33 initialization, macOS build, display
  acceleration and Metal remain unverified.

- **LATEST CHECKPOINT: 2026-10-21 — MACOS BUILD FEASIBILITY INVENTORIED;
  PORTABLE X86_64 VALIDATION HANDOFF RECORDED.**
- Added `src/navi33-research/MACOS_VALIDATION_HANDOFF.md`. The Navi48
  Makefile's target is x86_64/macOS 11.0 and uses Apple Clang/xcrun, private
  kernel flags, MacKernelSDK Headers and `Library/x86_64` libraries
  (`libkmodc++`, `libkmod`), plus ten required firmware blobs. `Info.plist`
  lists KPI versions 20.0.0 and Navi48 PCI IDs only. Local BSD-3 LICENSE,
  NOTICE upstream attributions (RDNA4FB BSD-3, mac-amdgpu MIT pinned
  `3bdeed2de940504ad6bd1bac718d5de2f65ddb83`, Linux amdgpu reference-only)
  were inspected.
- Current Windows checkout lacks `src/RDNA4FB/MacKernelSDK/`, its expected
  headers/libraries, and `src/navi48-bringup/firmware/`; no Apple SDK/tools or
  make is available here. SDK/firmware licenses and provenance are therefore
  unverified. Portable diagnostic instructions compile/link the core and
  lifecycle tests as an ordinary macOS x86_64 executable; they do not use
  MacKernelSDK or prove kernel ABI. A Navi48 kext build is separate and cannot
  establish Navi33 support.
- **Verification:** Debug and Release builds succeeded; CTest 4/4 each
  (0.16s); IP Discovery Python suite 30 passed (`1.778s`); Navi48 isolation
  4 passed (`0.010s`); Debug and Release Windows audits completed with explicit
  no-macOS-inference banners; `git diff --check` passed.
- **Still unverified:** macOS compile/link, kernel module or Apple C++ ABI,
  RX 7600 initialization, display acceleration, and Metal. No hardware or
  system configuration was accessed or changed.

- **LATEST CHECKPOINT: 2026-10-21 — BORROWED VIEW LIFETIMES VERIFIED; DEBUG
  AND RELEASE BOUNDARIES PASS.**
- Navi33 core string views are limited to four input fields:
  `Navi33DeviceIdentity::architectureLabel`,
  `IpBlockDescriptor::blockName/evidenceSource`, and
  `FirmwareRequirement::identifier`. The three validators consume them
  synchronously and return `const char*` static diagnostics. The driver core
  stores only `DeviceValidation`, never an input view. `rx7600ResearchIdentity`
  uses static literal storage. No view parameter or return type exists in the
  core. Caller backing storage need only survive the synchronous call.
- Tests cover local-string-backed valid identity/IP/firmware inputs and inspect
  only returned static text after those strings expire; empty fields and
  malformed identity labels are rejected. Lifecycle continues and cleans up
  after the supplied identity text expires. Repeated prepare/start after
  backend failures does not repeat callbacks or cleanup. No dangling-view or
  undefined-behavior-dependent test was added.
- Existing CMake was reviewed and left unchanged: it builds portable C++17
  tests, not macOS/kext code. Audit output now explicitly states MSVC/DUMPBIN
  Windows-only and disclaims macOS compilation/linking inference.
- **Verification:** Debug and Release builds succeeded; CTest passed 4/4 in
  each (0.28s each); Python parser suite 30 passed (`1.623s`); Navi48 isolation
  4 passed (`0.007s`); Debug/Release audits completed; `git diff --check`
  passed. Known MSVC D9025 warnings remain.
- **Still unverified:** macOS compilation/linking and kernel ABI compatibility;
  RX 7600 initialization, display acceleration, and Metal. No hardware, driver,
  EFI, or security configuration was accessed or changed.

- **LATEST CHECKPOINT: 2026-10-21 — PORTABLE CORE STRING ALLOCATIONS REMOVED;
  WINDOWS OBJECT-LEVEL AUDIT AND TESTS PASS.**
- Replaced owning `std::string` inputs with borrowed `std::string_view` and
  result diagnostics with static `const char*`. Lifecycle transition text is
  unchanged; new regression assertions check exact diagnostics. State,
  callbacks, cleanup, and Navi48 separation are unchanged.
- Before the change, `dumpbin` attributed `operator new/delete`,
  `_CxxThrowException`, `std::_Xlength_error`, and `type_info` vftable
  undefined references to both `Navi33Research.obj` and
  `Navi33DriverCore.obj`. Shared `std::string` fields and result construction
  caused standard-library instantiations; the explicit lifecycle concat was
  `Navi33DriverCore.cpp`'s
  `std::string("Invalid lifecycle transition: ") + operation`. These symbols
  were static-object references, not final executable link errors.
- `audit_portable_core.py` now lists selected undefined symbol categories per
  Navi33 object, source references to owning `std::string`, and final test
  executable DLL imports separately. After refactor, the selected allocation,
  C++ throw, string-length-error, and RTTI-vftable categories are absent from
  both Debug and Release objects. `memcmp`/`strlen` remain in Release archive
  output; Debug has additional CRT diagnostics. Both Windows test executables
  link successfully. Windows results do not establish Apple kernel ABI
  compatibility; `std::optional`, `std::string_view`, and virtual callbacks
  remain.
- **Verification:** Visual Studio 2022 x64 Debug and Release builds succeeded;
  CTest 4/4 passed in each (`0.26s` Debug, `0.24s` Release). Python parser
  tests: 30 passed (`1.452s`); Navi48 isolation: 4 passed (`0.008s`);
  Debug/Release object audits completed; `git diff --check` passed. MSVC D9025
  override warnings remain.
- **Still unverified:** no macOS compile/link or kernel ABI validation, no RX
  7600 initialization, display acceleration, or Metal evidence. No hardware,
  EFI, security, or system settings were accessed or changed.

- **LATEST CHECKPOINT: 2026-10-21 — DECLARED C++ PARSER BOUNDS ADDED;
  WINDOWS CORE RUNTIME AUDIT COMPLETE.**
- The freestanding Navi48 IP Discovery parser now bounds the table to the
  declared binary and lazy record/base-array reads to the declared table.
  Five compiled C++ synthetic fixture cases pass, covering valid v1/v2 layouts,
  v1 table-list overlap, base-array/record overrun, and binary extent. Lazy
  validation and the 16-base output clamp remain. This is not RX 7600 format
  evidence.
- Added read-only `src/navi33-research/tools/audit_portable_core.py`.
  Debug/Release MSVC commands confirm `/GR-` and final `/EHs-c-` on all eight
  logged Navi33 compilation commands. Release `dumpbin` reports 16 undefined
  library symbols, including `operator new/delete`, `_CxxThrowException`,
  `std::_Xlength_error`, `type_info` vtable, and libc/string functions; the
  test executable imports MSVC runtime DLLs. Thus the existing `std::string`,
  `std::optional`, and virtual-callback API is not yet established as
  kernel-neutral. No threads, direct `new/delete` expressions, or dynamic
  global initializers were found in the reviewed core sources.
- Debug and Release builds succeeded; CTest 4/4 passed in each configuration.
  Final Debug and Release CTest reruns: 4/4 passed in each (`0.21s`).
  Parser Python suite: 30 passed (`1.782s`); Navi48 isolation: 4 passed
  (`0.008s`);
  `git diff --check` passed. Windows-only evidence; no macOS source was built.
- Next: on an authorized Mac with Xcode/Apple SDK and the exact licensed
  MacKernelSDK, link a portable-core-only probe using the kext flags and
  inspect unresolved symbols before designing a kernel-neutral boundary.
  No Navi33 IOKit service or `1002:7480` Navi48 match was added.

- **LATEST CHECKPOINT: 2026-10-21 — MSVC EXCEPTION/RTTI SETTINGS VERIFIED;
  MACOS KEXT LINK STILL BLOCKED.**
- Added a compile-time configuration guard to each Navi33 library/test
  translation unit. MSVC rebuilt all targets with `/GR-` and final `/EHs-c-`;
  compile-time checks confirm exception/RTTI macros are absent. Visual Studio
  supplies `/EHsc` earlier, so D9025 override warnings remain intentionally
  visible; no switch correction or warning suppression was made.
- Audited Navi48 parser/build boundary. The legacy C++ parser checks binary
  checksum and input-buffer reads, but not table checksum or declared
  binary/table extents during record walks; its source validation is for a
  Navi48 board ROM. The Python strict parser has declared extents checks, but
  neither proves Navi33 physical format compatibility.
- Repository-local `src/RDNA4FB/MacKernelSDK`, Apple SDK/tools
  (`xcrun`, Apple Clang, `make`, `codesign`, `dwarfdump`) are unavailable.
  The existing kext build also requires the macOS 11 x86_64 SDK, private
  kernel flags, `kmodc++`/`kmod`, and Navi48-specific firmware. No macOS
  source was compiled and no Navi33 IOKit service/personality was added.
- Validation: MSVC Debug build exit 0; CTest 3/3 passed (0.28s);
  `python -B test_ipdiscovery.py`: 30 passed (`1.643s`, `OK`);
  `git diff --check` passed. No RX 7600/macOS functionality is demonstrated.
- Next smallest gate: with an authorized macOS/Xcode host and licensed
  MacKernelSDK, attempt a separate portable-core-only kext link under the
  existing kernel flags, with no personality/hardware callbacks; inspect
  standard-library ABI and unresolved symbols before adding IOKit integration.

- **LATEST CHECKPOINT: 2026-10-21 — MACOS SERVICE DEFERRED; CORE COMPILES
  UNDER MSVC NO-RTTI/NO-EXCEPTIONS FLAGS.**
- Reviewed local platform boundary: the only Apple target is the separate
  Navi48 x86_64 IOKit kext (macOS 11 deployment; `IOService`/`IOPCIDevice`,
  private-kernel flags). Its plist remains `1002:7550`/`1002:7551` only.
  The external `src/RDNA4FB/MacKernelSDK` and Apple SDK/tools are absent on
  this Windows host, so a Navi33 kext service/link ABI cannot be validated.
  No new IOKit code or PCI personality was added.
- Added MSVC `/EHs-c- /GR-` and GNU/Clang `-fno-exceptions -fno-rtti` to the
  portable CMake library and tests, matching relevant C++ constraints of the
  current kext build. Windows configure/build exit 0; MSVC emits D9025 option
  override warnings. CTest: 3/3 passed; parser: `Ran 30 tests in 1.676s`,
  `OK`; isolation script: `Ran 4 tests in 0.010s`, `OK`;
  `git diff --check` passed. This is not an Apple SDK/kext build.
- Next platform gate: use an authorized macOS/Xcode CLI plus the external
  MacKernelSDK to build a separate, opt-in Navi33 IOKit target. Keep Navi33
  out of Navi48 matching. No RX 7600 macOS functionality is demonstrated.

- **LATEST CHECKPOINT: 2026-10-21 — PORTABLE NAVI33 C++ CORE COMPILED;
  CTEST 3/3 PASSED.**
- Newly available Visual Studio 2022 Build Tools had bundled CMake
  `3.31.6-msvc6` (not on PATH), MSVC x64 `19.44.35229.0`, toolset
  `14.44.35207`, and Windows SDK `10.0.26100.0`. No tools were installed.
- Configured with the user-provided args at
  `build/navi33-research-msvc`: Visual Studio 17 2022, x64. Configure exit 0;
  Debug build exit 0. C++ research executable: 26 passed; lifecycle
  executable: 28 passed. CTest exit 0, 3/3 passed (research C++, lifecycle
  C++, and Navi48 static routing isolation). Parser suite: 30 passed. Full
  commands/output and the corrected CTest executable lookup are in WORKLOG.
- Build output is Git-ignored. No macOS kext build or hardware validation was
  performed; the lifecycle has no production IOKit/hardware backend. No RX
  7600 macOS driver functionality is demonstrated.
- Git remains branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; prior user modifications and
  untracked files preserved; nothing staged or committed.

- **LATEST CHECKPOINT: 2026-10-21 — PORTABLE NAVI33 LIFECYCLE CORE ADDED;
  C++ BUILD NOT VERIFIED.**
- Added `Navi33DriverCore.hpp/.cpp`, reusing the existing identity validator.
  It supports `Uninitialized`, `Identified`, `Prepared`, `Running`, `Failed`,
  and `Stopped`; rejects illegal/repeated transitions; and requests one-time
  cleanup on backend failures, explicit stop, or destruction. Identity
  detection itself makes no platform callback. The only backend interface is
  abstract and the tests use a host mock; no production macOS/hardware backend
  exists. `Running` does not imply GPU operation.
- Added `test_Navi33DriverCore.cpp`, a separate CMake test executable, and a
  source-contract regression asserting the Navi33 core does not reference or
  link Navi48 initialization. Navi48 plist IDs and code were not changed.
- **Checks:** From repo root,
  `python -B src/navi33-research/tests/test_navi48_isolation.py` — exit 0,
  `Ran 4 tests in 0.006s`, 4 passed; from `tools/ipdisc/`,
  `python -B test_ipdiscovery.py` — exit 0, 30 passed; `git diff --check` —
  exit 0. No C++ compile/test or macOS build ran because the already verified
  compiler/CMake toolchain is absent.
- **Next gate:** Obtain approval for the Visual Studio 2022 Build Tools C++
  workload/MSVC x64, Windows SDK, and CMake installation already documented
  in WORKLOG; then run the separate CMake/CTest target. No RX 7600 macOS
  functionality is demonstrated.

- **LATEST CHECKPOINT: 2026-10-21 — PROVENANCE-BOUND OFFLINE REPORT
  IMPLEMENTED; C++ BUILD STILL NEEDS USER-APPROVED TOOL INSTALL.**
- `tools/ipdisc/dump.py` now accepts `--provenance SIDECAR.json` with `--json`.
  It validates required evidence fields and artifact class, rejects malformed
  or duplicate-key/oversized metadata, checks supplied SHA-256 against the
  parsed bytes, and reports input size/hash plus `STRUCTURE_ONLY` scope. The
  report explicitly labels sidecar metadata unauthenticated; it cannot certify
  provenance or board attribution. `README.md` documents the interface.
- **Checks:** `python -B test_ipdiscovery.py` from `tools/ipdisc/` — exit 0,
  30 passed; `python -B src/navi33-research/tests/test_navi48_isolation.py`
  from repo root — exit 0, 3 passed; `git diff --check` — exit 0. No C++ compile
  or CTest was possible; these Python checks do not validate C++.
- **Build decision:** CMake needs 3.16+, C++17 compiler, CXX-only; recommended
  practical Windows setup is Visual Studio 2022 Build Tools with Desktop
  development with C++/MSVC x64 and Windows SDK, plus CMake. Proposed install
  commands, not executed:
  `winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --interactive`
  and `winget install --id Kitware.CMake --exact --interactive`. User approval
  is required before installation. Then configure
  `cmake -S src/navi33-research -B build/navi33-research-msvc-phase16 -G "Visual Studio 17 2022" -A x64`,
  build, and run CTest as recorded in WORKLOG.
- Git remains branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; the prior two tracked
  modifications and all 31 untracked paths/cache artifacts were preserved.
  Navi48 initialization was not changed. No RX 7600 macOS functionality is
  demonstrated.

- **LATEST CHECKPOINT: 2026-10-20 — C++ RESEARCH COMPONENT REVIEWED;
  COMPILE/EXECUTION BLOCKED BY ABSENT TOOLCHAIN.**
- `src/navi33-research/` already has a standalone C++17 library and synthetic
  unit tests. The reviewed component validates explicitly supplied identity,
  IP-evidence, and firmware-evidence records; it has no hardware interface.
  This is source review only; the C++ library and tests have not been compiled
  or executed.
- Tool discovery found no `clang++`, `clang-cl`, `g++`, `cl`, CMake, Ninja,
  Make, or MinGW Make. Common Visual Studio/BuildTools, Windows Kits, LLVM,
  MSYS2, and MinGW locations were absent; `vswhere` and Visual Studio registry
  instances were absent. No compiler was installed.
- **Checks:** `python -B src/navi33-research/tests/test_navi48_isolation.py`
  — exit 0, 3 passed; `python -B test_ipdiscovery.py` from `tools/ipdisc/`
  — exit 0, 28 passed; `git diff --check` — exit 0. Python checks are not
  C++ compilation or execution.
- No C++ code/configuration was changed because the requested executable
  validation is unavailable. Navi48's previously added identity check remains
  intact. Next task: run the existing CMake build and CTest with an already
  authorized compiler environment, address actual failures, then reassess
  implementation. No RX 7600 macOS support is demonstrated.
- Git remains branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; preserve two tracked changes
  and 31 untracked paths/cache files. Nothing was staged, committed, or removed.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 14 SELECTED A PROVISIONAL
  X86_64 INTEGRATION SHELL; RX 7600 DRIVER PATH NOT DEMONSTRATED.**
- Architecture comparison in
  `tools/ipdisc/RX7600_MACOS_SOURCE_FEASIBILITY.md`: there is not enough
  board, macOS graphics-interface, or build evidence for a functioning
  Navi33 driver. If work proceeds on x86_64, isolate Navi33 in a separate
  IOKit service/class and generation-specific modules inside Cobalt64's
  existing x86_64 target; reuse no Navi48 IP routines. A standalone driver
  duplicates platform work. The arm64 DriverKit/RDNA4 mac-amdgpu project is
  not a drop-in. WhateverGreen is a BSD-3 patch layer for Apple's AMD stack,
  not an independent GPU implementation. NootedRed's published license
  prohibits AI use; no source was adapted.
- `Navi48Bringup::start()` now rechecks AMD VID and Navi48 IDs `0x7550`/
  `0x7551` before PCI memory enable, BAR access, or MMIO mapping. The existing
  static test file adds a regression check for this ordering, so `1002:7480`
  cannot enter Navi48 hardware setup through that lifecycle method.
- **Tests:** From repo root, `python -B
  src/navi33-research/tests/test_navi48_isolation.py` — exit 0,
  `Ran 3 tests in 0.006s`, `OK`. From `tools/ipdisc/`,
  `python -B test_ipdiscovery.py` — exit 0, `Ran 28 tests in 0.762s`, `OK`.
  `git diff --check`, edited-file whitespace and report-local-link checks
  passed. The route test is static source validation. No C++ build/CTest,
  macOS integration, or physical hardware validation took place.
- **Git/safety:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; two tracked modifications
  (including the pre-existing, preserved `src/navi48-bringup/NOTICE`
  change) and 31 preserved untracked paths including caches. No hardware, firmware,
  EFI/system settings, install, download, stage, commit, reset, clean, stash,
  or deletion occurred.
- **Next task/blocker:** Obtain provenance-documented RX 7600 IP Discovery
  data and independently compile a minimal x86_64 IOKit service in an
  authorized macOS SDK environment. No RX 7600 macOS functionality is
  demonstrated.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 13 RESOLVED mac-amdgpu REVISION;
  DRIVERKIT/IOKIT SPLIT CONFIRMED; NAVI33 STILL EVIDENCE-BLOCKED.**
- The prior `lemonade-sdk/mac-amdgpu@3bdeed2` reference resolves to full
  commit `3bdeed2de940504ad6bd1bac718d5de2f65ddb83` (tree
  `7101e9c18d41b22a77c5d06f452a99f50f5a5e6e`, v0.1.48, 2026-05-22); GitHub
  marks it unsigned. Its checked-in LICENSE is MIT. The pinned source is an
  arm64 DriverKit/PCIDriverKit project for R9700/RDNA4; modules include
  GFX12/GMC12/IH7/SDMA7/SMU14/PSP14. This does not provide Navi33 source.
- Apple `IOPCIDevice` and `IOService` docs confirm public DriverKit PCI,
  service lifecycle, and user-client interfaces, but no AMD GPU acceleration
  or Metal ABI. Cobalt64's separate Navi48 code remains an x86_64 IOKit kext
  with private-kernel build flags and Navi48-only ID guards. Local source
  files differ from the pinned upstream blob hashes; this is an adaptation,
  not a byte-identical source drop.
- Upstream mac-amdgpu status descriptions are inconsistent within the pinned
  revision: root README reports initialization stages and leaves PM4/BO ABI/
  Mesa winsys as future work; `dext/README.md` describes an earlier skeleton;
  the commit message claims SDMA ring submissions. Treat these as unsigned,
  project-authored statements, not independently verified tests. The NOTICE
  and existing feasibility report now record this distinction.
- **Tests:** `python -B test_ipdiscovery.py` from `tools/ipdisc/` — exit 0,
  `Ran 28 tests in 0.915s`, `OK`. Repository-root
  `python -B src/navi33-research/tests/test_navi48_isolation.py` — exit 0,
  `Ran 2 tests in 0.004s`, `OK`. `git diff --check` passed; edited-file
  whitespace scan passed. C++ compilation/CTest, integration and physical
  hardware validation were not performed.
- **Git/safety:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; preserve 31 existing
  untracked files/cache artifacts. The NOTICE and feasibility report were
  updated; no staging/commit/cleanup or hardware/system access occurred.
- **Next blocker/task:** Obtain provenance-documented RX 7600 IP Discovery
  evidence, then define a pure Navi33 dispatch slice from observed versions.
  Keep `1002:7480` research-only and outside Navi48 initialization.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 12 TYPED EVIDENCE CATEGORIES
  ADDED; COMPILATION AND BOARD EVIDENCE STILL BLOCKED.**
- The isolated `src/navi33-research/` model now tags IP and firmware records
  as synthetic, generic upstream, board-attributed, macOS-interface-review,
  or target-hardware-test evidence. Completeness requires a recognized kind;
  the board-attribution helper classifies the supplied label only and does
  not authenticate it. C++ regression cases were added but are uncompiled.
- No physical-card IP Discovery artifact or local mac-amdgpu checkout/archive
  was found. The `3bdeed2` source citation remains unresolved. Existing
  records of Linux commit
  `d0c647a6aae2ce577372594a176c82733e1ed267` establish only generic Linux
  source behavior, not board IP versions or macOS compatibility. The only
  project `.bin` assets found are Navi48 shader fixtures under
  `src/navi48-bringup/shaders/`.
- **Tests:** `python -B test_ipdiscovery.py` from `tools/ipdisc/` — exit 0,
  `Ran 28 tests in 0.788s`, `OK`. Repository-root command
  `python -B src/navi33-research/tests/test_navi48_isolation.py` — exit 0,
  `Ran 2 tests in 0.004s`, `OK`. `git diff --check` exited 0 but does not
  inspect these untracked files. C++ compilers, CMake, Ninja and Make were
  absent; C++ tests were not compiled or run.
- **Git/safety:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; 31 untracked paths, no tracked
  or staged changes. Existing artifacts and cache files remain untouched.
  No network, hardware, firmware, or system access occurred.
- **Next blocker:** Obtain a genuine provenance-documented RX 7600 artifact,
  resolve the exact mac-amdgpu source revision and license, and use a
  supported isolated C++/macOS SDK toolchain before implementation claims.
  Continue to keep `1002:7480` outside Navi48 initialization.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 11 OFFLINE SOURCE FEASIBILITY
  RECORDED; NAVI33 IMPLEMENTATION STILL EVIDENCE-BLOCKED.**
- `tools/ipdisc/RX7600_MACOS_SOURCE_FEASIBILITY.md` contains the source-backed
  subsystem matrix and dependency order. Local Cobalt64 source confirms the
  Navi48 IOKit route is limited to `1002:7550`/`1002:7551` and uses
  GFX12/GMC12/IH7/SDMA7/SMU14/Navi48 display logic. Navi33 remains a pure
  identity/evidence research model. DriverKit-to-IOKit and host-DMA
  differences are documented in checked-in source; they do not establish
  cross-generation compatibility.
- **Provenance:** Checked-in notices cite
  `lemonade-sdk/mac-amdgpu@3bdeed2` under MIT, but the short commit object is
  absent from local Git (`git cat-file -e '3bdeed2^{commit}'` failed); only
  Cobalt64 is configured as a remote and no network was authorized. Full
  hash/tree/original source provenance remain **UNVERIFIED**. Existing
  reports document narrow generic Linux claims at pinned commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`; not re-fetched this pass and
  not evidence of physical-board versions or macOS APIs.
- **Implementation choice:** No Navi33 implementation was justified:
  dispatch/version mapping needs board-attributed IP Discovery evidence, and
  upstream code reuse needs resolved source provenance/license review. The
  smallest credible next vertical slice is strict offline parsing of a
  genuine, provenance-documented artifact, then a pure version-dispatch model
  using observed records and pinned references.
- **Tests:** `python -B
  src/navi33-research/tests/test_navi48_isolation.py` from repo root — exit
  0, 2 passed. `python -B test_ipdiscovery.py` from `tools/ipdisc/` — exit
  0, 28 passed. `git diff --check` passed. CMake/C++ compile/CTest remain
  unavailable and unrun.
- **Safety/state:** No network or hardware access, no install or system
  modifications, no stage/commit/reset/clean/stash/delete. Existing
  untracked files and cache artifacts preserved. No board-specific evidence
  or RX 7600 macOS support established.
- **Next action:** Resolve full mac-amdgpu source provenance through an
  authorized authoritative source and await a real board-attributed IP
  Discovery artifact. Keep `1002:7480` research-only until evidence and a
  compatible macOS build environment support a further step.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 10 NAVI33/NAVI48 SEPARATION
  STATIC CONTRACT ADDED; DRIVER IMPLEMENTATION REMAINS BLOCKED.**
- **Verified from local source:** The four personalities in
  `src/navi48-bringup/Info.plist` list only `0x75501002` and `0x75511002`.
  `Navi48Bringup::probe()` in
  `src/navi48-bringup/src/Navi48Bringup.cpp` (around lines 7327-7339)
  independently requires the opt-in boot argument, AMD vendor ID, and
  device `0x7550`/`0x7551`. Its `start()` proceeds to PCI memory enable,
  BAR latching/mapping, IP/PSP/SMU surveys, and opt-in staged hardware
  bring-up. No physical operation was run.
- The N48 source contains generation-specific GFX12/GMC12/IH7/SDMA7/SMU14
  and Navi48 DCN code, while `src/navi33-research/` remains a pure
  identity/evidence model with no OS, memory, command, interrupt, display,
  firmware-loading, or power integration. Its evidence does not establish
  RX 7600 IP or firmware needs. N48 code (including its documented
  arm64-DriverKit to x86-IOKit adaptation) is not established as Navi33
  compatible. Local source comments cite upstream `3bdeed2`; this pass did
  not independently verify that upstream object.
- Added `src/navi33-research/tests/test_navi48_isolation.py` and registered
  it in CTest when Python 3 is present. It checks the plist's complete PCI
  match set and runtime probe source guards. This is a static source
  contract—not compiled/runtime validation or implementation of GPU
  support.
- **Tests:** Repository root:
  `python -B src/navi33-research/tests/test_navi48_isolation.py` — exit 0,
  `Ran 2 tests in 0.003s`, `OK` (2 passed). From `tools/ipdisc/`:
  `python -B test_ipdiscovery.py` — exit 0, `Ran 28 tests in 0.724s`,
  `OK` (28 passed). `git diff --check` passed. CMake/CTest/C++ build not
  run because build tools are unavailable.
- **State/safety:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; 30 untracked paths, preserving
  all previous files and cache artifacts. No network, hardware, firmware, or
  system access; nothing staged, committed, reset, cleaned, stashed, or
  deleted.
- **Next blocker:** Board-attributed Navi33 IP/firmware evidence and an
  available isolated macOS build/test environment are still absent. Keep
  `1002:7480` out of Navi48 initialization. Do not claim RX 7600 support.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 9 OFFLINE INPUT CHANGE DETECTION
  AND MALFORMED-INPUT COVERAGE IMPROVED.**
- Rechecked the current environment. `clang++`, `g++`, `c++`, `cl`, `cmake`,
  `ninja`, and `make` remain unavailable; Git Bash has no compiler/tool
  binaries available, and WSL is not installed. No toolchain was installed
  or downloaded. C++ build validation remains blocked.
- `tools/ipdisc/dump.py` now checks file size plus `st_mtime_ns` and
  `st_ctime_ns` on the opened regular file before/after bounded reading.
  Detected metadata changes fail as `InputChangedError` (CLI exit 2). This
  is best-effort only: a filesystem that does not reliably update timestamps
  or deliberately restored timestamps can evade detection.
- Added deterministic metadata-change regression coverage. Strengthened
  seeded malformed input testing to 512 synthetic inputs; every other input
  includes a signature at an aligned candidate position and confirms it is
  actually validated. Set `CXX_EXTENSIONS OFF` on both CMake targets to
  request standard C++17 without compiler extensions; this remains
  unconfigured because CMake is unavailable.
- **Regression:** From `tools/ipdisc/`, `python -B test_ipdiscovery.py`
  exited 0: `Ran 28 tests in 0.832s`, `OK` (28 passed, 0 failed, 0 skipped).
  Baseline before changes: 27 passed, 0 failed, 0 skipped. `git diff
  --check`, custom whitespace and local-link checks passed.
- **Git/safety:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, 29 untracked paths (including
  the two existing `__pycache__` files), all preserved. No staging, commit,
  reset, clean, stash, deletion, network, hardware access, or installation
  occurred.
- **Evidence boundary:** No physical RX 7600 artifact, board-specific
  evidence, or target test was obtained. The parser and C++ research model
  do not establish macOS support or a functioning driver.
- **Next action:** Run the documented CMake configure/build/CTest commands
  only when an already-installed CMake 3.16+ and C++17 compiler become
  available. Until then, do not present static review as build validation.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 8 C++ BUILD READINESS PREPARED;
  EXECUTABLE C++ VALIDATION BLOCKED.**
- Audited the isolated `src/navi33-research/` module, its API and synthetic
  tests. `1002:7480` selects only `Navi33ResearchOnly`; Navi 48 remains
  separate; evidence completeness is not authentication; no selection can
  initialize hardware. Added fail-closed tests for invalid architecture
  family and firmware evidence enum values. No production driver behavior
  changed.
- Added a local CMake 3.16+ project that builds only the research library and
  its test executable as C++17, with standard-library-only dependencies,
  warnings-as-errors, CTest registration, and build output directed to
  `build/navi33-research`. `src/navi33-research/README.md` documents CMake
  and direct clang++ commands.
- **C++ validation blocker:** `clang++`, `g++`, `c++`, `cl`, `cmake`,
  `ninja`, and `make` were unavailable. C++ configure/build/tests were not
  run. The source review and Python parser tests do not establish C++ build
  correctness.
- **Regression/checks:** From `tools/ipdisc/`, `python -B
  test_ipdiscovery.py` exited 0: `Ran 27 tests in 0.738s`, `OK` (27 passed,
  0 failed, 0 skipped; synthetic fixtures). `git diff --check` passed.
  README whitespace/local-link check passed (`Checked 1 Markdown files; 0
  local links`). No C++ executable validation occurred.
- **Git/safety:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; 29 untracked paths, including
  the two pre-existing `__pycache__` artifacts. All prior untracked paths
  were preserved. Nothing was staged, committed, reset, cleaned, stashed, or
  deleted. No toolchain was installed or network/hardware accessed.
- **Evidence boundary:** No board-specific evidence or target test was
  obtained. RX 7600 macOS support remains unestablished; the research model
  is not a functioning driver.
- **Navi 48 routing audit:** `src/navi48-bringup/Info.plist` lines 31-32,
  46-47, 68-69, and 85-86 declare only PCI matches `0x75501002` and
  `0x75511002`; a literal `1002:7480` match was not found in that subtree.
  This is a static declaration check, not a runtime test.
- **Next action:** Run the documented CMake configure/build/CTest sequence
  when an already-installed CMake 3.16+ and C++17 compiler are available.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 7 BOUNDED OFFLINE INPUT INTAKE
  IMPLEMENTED; PROJECT REMAINS PAUSED.**
- `dump.py` now caps CLI input at 1 MiB. It opens a regular file, checks size
  via the open descriptor before reading, reads in bounded chunks up to
  limit+1, and checks file size again afterward. Oversized input or a
  detected size change fails with a clear stderr message and exit code 2;
  it is never truncated. The parser's 512-byte minimum and valid supported
  parse behavior remain unchanged.
- Added tests for a sub-minimum file, a valid in-limit fixture, over-limit
  input, deterministic file growth during read, unsupported header and
  strict invalid-layout CLI errors. Existing malformed, overlap, and
  compatibility coverage remains.
- **Test:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`; exit 0, `Ran 27 tests in 0.687s`, `OK`
  (27 passed, 0 failed, 0 skipped). All artifacts were synthetic fixtures
  or temporary test files.
- **Docs/checks:** Updated `README.md` and
  `RX7600_ARTIFACT_INTAKE.md` with the 1 MiB policy and response behavior.
  `git diff --check`, whitespace, and local-link checks passed.
- **Initial/final Git state:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; the same 28 untracked paths
  remain, including both pre-existing cache artifacts. No staging, commit,
  reset, cleanup, stash, or deletion occurred.
- No real board artifact was supplied or analyzed. Physical RX 7600 IP
  versions remain **UNRESOLVED**; parser hardening establishes no
  board-specific evidence or macOS support.
- **Next step:** Await a genuine artifact with provenance, then analyze a
  hashed copy using strict CLI mode within the documented 1 MiB limit.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 6 OFFLINE ARTIFACT INTAKE
  DOCUMENTED; PROJECT REMAINS PAUSED.**
- Added `RX7600_ARTIFACT_INTAKE.md` and linked it from `README.md`. The
  procedure covers untrusted-byte handling, provenance, original/hash
  preservation, strict-only analysis, reportable parse failures, and the
  limits of parser evidence.
- **Parser boundary:** Strict mode accepts binary header major 0-2 and IP
  Discovery versions 1-4, one die, up to 256 IP records and 16 32-bit bases
  per record; v4 64-bit bases are unsupported. It checks structural bounds,
  overlaps, signatures and checksums. Input must be at least 512 bytes. The
  16-bit binary size allows at most 65,535 bytes for the declared binary
  region, but CLI reads the whole file and has no configured input-size cap.
  No parser behavior was changed.
- **Regression:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`; exit 0, `Ran 22 tests in 0.276s`, `OK`
  (22 passed, 0 failed, 0 skipped). Tests use synthetic data only.
- No real board-attributed artifact was supplied or acquired. The physical
  RX 7600 IP inventory remains **UNRESOLVED** and the project remains
  paused. A parser pass cannot prove authenticity, board identity,
  completeness, firmware requirements, or macOS support.
- **Initial Git state:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, 27 untracked paths. All
  pre-existing artifacts and both cache files were preserved. The new
  intake report brings the total to 28 untracked paths. No staging, commit,
  reset, cleanup, stash, or deletion occurred.
- **Next step:** Await a genuine artifact with the required provenance fields
  and existing board-identification evidence, then preserve/hash and review
  an analysis copy offline in strict mode. Do not probe hardware or infer
  RX 7600 support.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 5 REMAINING UPSTREAM SOURCE CLAIMS
  VERIFIED; PROJECT REMAINS PAUSED.**
- Added `RX7600_REMAINING_SOURCE_CLAIMS.md`. The exact pinned GitHub Contents
  and Blob APIs returned `amdgpu_discovery.c` and `smu_v13_0.c` at Linux
  commit `d0c647a6aae2ce577372594a176c82733e1ed267`; the report records blob
  object IDs, byte lengths, SHA-256 digests, and verified line ranges.
- **Source findings:** `amdgpu_discovery.c` performs version-based generic
  block routing and maps decoded IP Discovery record fields into register
  offsets and `ip_versions`. `smu_v13_0_init_microcode()` skips firmware
  requests for an SR-IOV VF; otherwise requests a required version-derived
  firmware path, with a kicker suffix branch. The report distinguishes these
  direct source facts from target-specific inference.
- `RX7600_UPSTREAM_SOURCE_VERIFICATION.md` now links this follow-up and
  reflects the resolved retrieval gap. This verifies generic Linux source
  behavior only; no physical-board IP inventory or firmware requirement was
  established.
- No `BOARD_SPECIFIC` or `TARGET_TESTED` evidence was obtained. RX 7600 IP
  versions remain **UNRESOLVED** and macOS support is not established.
- **Regression:** From `tools/ipdisc/`, `python -B test_ipdiscovery.py`
  exited 0; `Ran 22 tests in 0.376s`, `OK` (22 passed, 0 failed, 0 skipped).
- **Initial Git state:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, 26 untracked paths. Existing
  paths and both cache artifacts were preserved. Phase 5 adds one new
  untracked report, for 27 total. No stage, commit, reset, cleanup, stash,
  or deletion occurred.
- **Next decision gate:** A provenance-documented IP Discovery artifact
  attributable to the physical board can be submitted for offline strict
  parsing. The project remains paused until such evidence or another
  explicitly authorized evidence task is available; no hardware testing or
  driver implementation is authorized by this source review.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 4 BOUNDED SOURCE VERIFICATION
  COMPLETE; PROJECT REMAINS PAUSED.**
- Created `RX7600_UPSTREAM_SOURCE_VERIFICATION.md` after read-only review of
  the two exact pinned GitHub commit APIs and cited source files. Both commit
  IDs are publicly retrievable. GitHub reports both commits as unsigned.
- **Verified narrowly:** Linux `discovery.h` format declarations; exact
  `amdgpu_drv.c` generic `CHIP_IP_DISCOVERY` entries and absence of explicit
  `0x7480` / `CHIP_NAVI33` literals; selected version cases, firmware-name
  declarations, and symbols in the other retrieved sources; and `pci.ids`
  entry 1002:7480. The report includes pinned URLs and SHA-256 values for
  retrieved files.
- **Still unverified:** Detailed `amdgpu_discovery.c` dispatch/record behavior
  and `smu_v13_0.c` request conditions (raw endpoints returned HTTP 429).
  Detailed behavior not directly checked in other retrieved files is scoped
  as unverified in the report. No moving branch or mirror was substituted.
- No `BOARD_SPECIFIC` or `TARGET_TESTED` evidence was obtained. Physical RX
  7600 IP versions remain **UNRESOLVED**; source verification does not prove
  macOS compatibility. The project remains paused.
- **Regression:** From `tools/ipdisc/`, `python -B test_ipdiscovery.py`
  exited 0; exact output summary `Ran 22 tests in 0.376s`, `OK` (22 passed,
  0 failed, 0 skipped). Synthetic parser tests are not board evidence.
- **Initial Git state:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, 25 untracked paths. Existing
  artifacts and both `__pycache__` files were preserved. Phase 4 adds one
  untracked report, for 26 total paths. No staging, commit, reset, cleanup,
  stash, or deletion occurred.
- **Next decision gate:** Obtain a provenance-documented, board-attributed
  IP Discovery artifact for offline strict parsing, or separately authorize
  a bounded source check for the files left unverified. Neither authorizes
  hardware testing or implementation.

- **LATEST CHECKPOINT: 2026-10-20 — PHASE 3 EVIDENCE PLAN COMPLETE; PROJECT
  PAUSED.**
- Created `RX7600_EVIDENCE_ACQUISITION_PLAN.md` and
  `RX7600_EVIDENCE_SCHEMA.md`. These are planning/recordkeeping deliverables,
  not evidence acquisition. No external source, hardware, or firmware was
  accessed.
- The plan distinguishes generic source claims from physical-board evidence,
  lists the artifacts and provenance needed for IP/version interpretation,
  firmware review, and macOS feasibility, and defines stop/go gates. It
  retains the previously reported missing-source-object limitations and
  marks cited revisions as unverified locally.
- **Current decision:** Continue paused pending a provenance-documented,
  board-attributed IP Discovery artifact or separate authorization for bounded
  upstream source verification. Physical RX 7600 IP versions remain
  **UNRESOLVED**; functional driver implementation remains **NO-GO**.
- **Python regression:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`; exit code 0, `Ran 22 tests in 0.396s`,
  `OK` (22 passed, 0 failed, 0 skipped). These are offline parser tests using
  synthetic fixtures, not hardware evidence.
- **Git state before Phase 3 edits:** Branch `main`; HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; no staged or tracked changes.
  The 23 pre-existing untracked paths were preserved, including both
  `__pycache__` files. Phase 3 added two new untracked plan/schema files;
  the resulting 25 untracked paths are under `src/navi33-research/` and
  `tools/ipdisc/`. No stage, commit, cleanup, or deletion occurred.
- **Next task:** Wait for qualifying evidence or separate authorization for
  source verification. Do not treat this plan as evidence, begin M2/driver
  implementation, access hardware, or claim RX 7600 macOS support.

- **LATEST CHECKPOINT: `PHASE 2.2 — NO ACCESSIBLE C++17 ENVIRONMENT`.**
- Checked existing alternatives without setup: WSL is not installed;
  Docker/Podman and common VM launchers are unavailable; no local CI workflow
  or configured CI runner exists. Prior PATH, Visual Studio, LLVM/MSYS2, and
  common compiler-path checks also found no toolchain. No remote service was
  accessed.
- No code or build-system changes were justified. The documented direct
  clang++ and MSVC commands remain the portable handoff.
- C++ is still **not compiled** and the 19 C++ assertions are **not
  executed**. Required Python regression passed: from `tools/ipdisc/`,
  `python -B test_ipdiscovery.py` => `Ran 22 tests in 0.359s`, `OK` (22
  passed, 0 failed, 0 skipped). Exact checks are appended in `WORKLOG.md`.
- **Next task:** Run the README's explicit compile and test commands in an
  already provisioned MSVC Developer Command Prompt or existing Linux/macOS
  C++17 environment; record compiler version and actual output.
- **Prior checkpoint:** `PHASE 2.1 — C++17 TOOLCHAIN CHECK BLOCKED`.
- Static review found no additional implementation defect beyond the evidence-
  naming ambiguity noted in the Phase 2.1 worklog. The validation field is now
  called `claimsComplete`; caller-provided enum states are not authenticated.
- No C++17 compiler/build tool or Developer Command Prompt environment was
  available after PATH, VS environment, vswhere, and common LLVM/MSYS2/VS
  location checks. C++ source was not compiled and tests were not executed.
- Python parser regression and final checks are recorded in the Phase 2.1
  `WORKLOG.md` entry. The architecture skeleton remains offline research only;
  no hardware or macOS driver integration is implemented.
- **Prior checkpoint:** `PHASE 2 — NAVI 33 SKELETON VALIDATION`.
- **Validation findings:** Invalid architecture-state enum values could pass
  selection; evidence validation conflated structural validity with evidence
  completeness for unknown IP versions; firmware flag aggregation implied
  authentication; and one C++ test checked enum ordinals rather than behavior.
  These were corrected in the isolated model and tests. `CapabilityState` no
  longer encodes a positive support claim.
- **C++ verification:** Static review completed. C++ source was not compiled
  and C++ tests were not executed because `clang++`, `g++`, `c++`, `cl`, Make,
  CMake, Ninja, and Zig are unavailable. The test source now has 19 assertion
  checks; build instructions remain in `src/navi33-research/README.md`.
- **Python regression:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`; exit 0, `Ran 22 tests in 0.407s`, `OK`
  (22 passed, 0 failed, 0 skipped). Existing parser behavior was not changed.
- **Current project status: `NAVI 33 OFFLINE RESEARCH SKELETON COMPLETE`.**
- **Functional driver implementation: `NO-GO`.** The separately authorized
  C++ model is not a driver; never route PCI ID `1002:7480` through Navi 48
  initialization.
- **Physical RX 7600 IP versions: `UNRESOLVED`.** Board-specific IP Discovery
  evidence is not available in the reviewed project artifacts; no trusted
  board-attributed dump or hardware report is recorded.
- **Upstream provenance:** Linux and PCI source revisions/paths are documented
  in project research, but the cited commit objects and source copies are not
  independently verified from the current checkout.
- **Firmware:** Exact target requirements, provenance, and authoritative
  redistribution terms are not fully verified.
- **macOS:** Integration and build strategy remain unresolved. No RX 7600
  macOS support, build, or hardware validation is established.
- **Hardware recovery:** A tested recovery plan is required before any
  hardware experiment. The reported Ryzen 5 7500F has no iGPU; do not assume
  CPU-graphics fallback.
- **Exact resume conditions:** Resume evidence work only when at least one
  meaningful new evidence source is supplied or authorized: (1) a trustworthy,
  provenance-documented IP Discovery artifact attributable to this RX 7600
  board; (2) explicit authorization for a separate upstream-source
  verification task that requires network access; or (3) authoritative
  licensing material for specific firmware assets under consideration.
  New evidence alone does not authorize hardware testing or functional driver
  implementation; reassess all remaining prerequisites first.
- **Current milestone:** The isolated C++17 model is under
  `src/navi33-research/`. `1002:7480` selects only `Navi33ResearchOnly`;
  Navi 48 is separately represented without an initialization route. Unknown
  IP/firmware facts remain explicit. No macOS component or hardware access is
  implemented.
- **Compiler blocker:** No C++ compiler, Make, or CMake is available in this
  Windows environment. No tools or dependencies were installed.
- **Latest report:** `RX7600_ARCHITECTURE_SKELETON.md` records the Phase 2
  findings, tests, compiler blocker, and limitations.
- **Phase 1 files:** Added `src/navi33-research/include/Navi33Research.hpp`,
  `src/navi33-research/src/Navi33Research.cpp`,
  `src/navi33-research/tests/test_Navi33Research.cpp`, and
  `src/navi33-research/README.md`; added
  `tools/ipdisc/RX7600_ARCHITECTURE_SKELETON.md`; updated
  `tools/ipdisc/README.md`, `tools/ipdisc/WORKLOG.md`, and this checkpoint.
- **Phase 2 files:** Updated the Navi 33 header, implementation, tests, module
  README, architecture-skeleton report, this worklog, and this checkpoint.
- **Git state:** Final branch, refs, untracked paths, and diff check are
  recorded in the latest Phase 2 `WORKLOG.md` entry. No staging or commit
  occurred; pre-existing parser files and cache artifacts were preserved.

- **Latest checkpoint:** 2026-10-20 — M2.6 research exit decision complete.
- **Decision/report:** `RX7600_NEXT_STEP_DECISION.md` recommends pausing
  pending trustworthy board-specific RX 7600 IP Discovery evidence. It
  rejects premature implementation (Path D: **NO-GO**) and defines minimum
  prerequisites for considering a separate future proposal. Linked from
  `README.md`.
- **Basis:** No trusted board-attributed IP Discovery artifact or exact
  physical-card IP inventory is established. The cited upstream Linux and
  PCI commit objects/files are absent from this local, non-shallow repository;
  M2.1 historical source inspection remains neither proven nor disproven.
  Firmware provenance/licensing, macOS integration/build strategy, and a
  tested recovery path are also missing.
- **Current repository state before M2.6 edits:** Branch `main`; HEAD,
  `origin/main`, and merge base `d1f7bad8408a0a2c016b067c477422975a59de44`;
  divergence `0 0`; no staged or tracked diff. Seventeen pre-existing
  untracked paths all under `tools/ipdisc/`, including both preserved
  `__pycache__` artifacts.
- **Files changed:** Added `RX7600_NEXT_STEP_DECISION.md`; added a README
  link; appended the M2.6 decision and final verification to `WORKLOG.md`;
  updated this checkpoint. No files outside `tools/ipdisc/` changed.
- **Final test:** From `tools/ipdisc/`, `python -B test_ipdiscovery.py`
  exited 0; `Ran 22 tests in 0.291s`, `OK` (22 passed, 0 failed,
  0 skipped). Local Markdown links in the README, decision report, resume,
  and worklog resolved. Tests use synthetic parser inputs only.
- **Final Git status:** Branch `main`; HEAD, `origin/main`, and merge base
  `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`; no staged
  or tracked diff. Eighteen untracked paths, all under `tools/ipdisc/`,
  including the decision report and two preserved Python caches. No paths
  outside the authorized directory were reported.
- **Recommended next action:** Pause pending a trusted, board-attributed
  artifact. A bounded upstream verification task needs separate network
  authorization. Do not probe hardware, access firmware, modify drivers, add
  `1002:7480` to Navi 48, or claim RX 7600 macOS support.

- **Latest checkpoint:** 2026-10-20 — M2.5 offline upstream provenance
  verification complete.
- **Deliverable:** Created `RX7600_PROVENANCE_VERIFICATION.md`, linked it
  from `README.md`, appended M2.5 to `WORKLOG.md`, and updated this
  checkpoint. All changes are under `tools/ipdisc/`.
- **Actual repository state:** Branch `main`; HEAD, `origin/main`, and merge
  base `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`.
  Repository is not shallow. No M2.5 Git mutation operation was performed.
- **Provenance result:** Linux commit
  `d0c647a6aae2ce577372594a176c82733e1ed267` and PCI database commit
  `22da7386ba818c037894f258248734770757b14f` are absent from the local
  object database. The cited Linux files and `pci.ids` are not present as
  local source copies. No upstream source checksum/provenance record was
  found. Their contents and line references remain **documented, not
  independently verified from this checkout**.
- **Historical boundary:** This does not establish whether earlier M2.1
  work actually inspected upstream source at the time; current local absence
  neither proves nor disproves that. No evidence contradicting the earlier
  source claims was found, so those claims were not rewritten as false.
- **Test:** From `tools/ipdisc/`, `python -B test_ipdiscovery.py` exited 0;
  exact summary `Ran 22 tests in 0.275s` and `OK`; 22 passed, 0 failed,
  0 skipped.
- **Links:** Local Markdown links in the M2.5 report, README, worklog, and
  checkpoint resolved. External links were not accessed.
- **Unresolved:** Exact IP versions and firmware requirements of the physical
  RX 7600; upstream Linux and PCI source content/revision verification;
  firmware provenance and licensing; macOS support/behavior.
- **Final Git status:** Branch `main`; HEAD, `origin/main`, and merge base
  `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`; no staged
  or tracked diff. Seventeen untracked paths, all under `tools/ipdisc/`,
  comprising the prior 16 paths plus `RX7600_PROVENANCE_VERIFICATION.md`.
  Existing Python cache artifacts were preserved. No paths outside the
  authorized directory were reported.
- **Single safest next action:** Keep the citations marked as
  documented/unverified until an explicitly authorized method supplies
  trusted local source evidence. No fetching, hardware testing, or driver
  implementation is authorized by this milestone.

- **Latest checkpoint:** 2026-10-20 — M2.4 source provenance and licensing
  audit complete as offline repository documentation work.
- **M2.4 deliverable:** `RX7600_SOURCE_LICENSE_AUDIT.md` was created and
  linked from `README.md`; this checkpoint and append-only `WORKLOG.md` were
  updated. All M2.4 edits are inside `tools/ipdisc/`.
- **M2.4 test:** From `tools/ipdisc/`, `python -B test_ipdiscovery.py`
  exited 0. Exact summary: `Ran 22 tests in 0.299s` and `OK`; 22 passed,
  0 failed, 0 skipped.
- **M2.4 link check:** Local Markdown links in `README.md` and
  `RX7600_SOURCE_LICENSE_AUDIT.md` were checked; all resolved. External links
  and upstream source were not fetched.
- **M2.4 source evidence:** The Linux revision
  `d0c647a6aae2ce577372594a176c82733e1ed267` and PCI database revision
  `22da7386ba818c037894f258248734770757b14f` appear in prior project
  documentation. Their commit objects and source lines were not independently
  verified in this audit. M2.1 Linux source paths are therefore classified as
  cited, not independently verified.
- **M2.4 licensing evidence:** Local MIT/BSD and third-party license texts
  and notices were inspected. Firmware blob-specific terms, provenance, and
  redistribution permissions remain **UNVERIFIED**; no legal conclusion is
  made. Exact IP versions for the user's physical RX 7600 remain
  **UNRESOLVED**.
- **Final M2.4 Git state:** See the final M2.4 `WORKLOG.md` entry for
  post-edit status. No pre-existing cache artifacts were removed. Only
  `tools/ipdisc/` paths are present in the reported working-tree state.
- **Single safest next action:** Obtain human review of authoritative
  provenance and license terms for any exact source/firmware material before
  considering reuse or redistribution. Do not implement, probe hardware, or
  claim RX 7600 macOS support.

- **Checkpoint:** 2026-10-20T19:47:43.850+07:00 (local timezone UTC+07:00).
- **Current milestone/objective:** M2.3 research readiness and
  reproducibility audit.
- **Last confirmed completed milestone:** M2.3 complete as an offline
  documentation and test reproducibility audit. M2.2's evidence finding
  remains current: no trusted board-specific RX 7600 IP Discovery artifact was
  found in the inspected repository, and no separate user artifact was
  supplied.
- **State:** Complete for M2.3 acceptance. Exact RX 7600 IP versions and
  hardware/macOS compatibility remain **UNRESOLVED**.
- **Files created/updated in M2.3:** Created
  `tools/ipdisc/RX7600_REPRODUCIBILITY_AUDIT.md`; updated
  `tools/ipdisc/README.md`, `tools/ipdisc/WORKLOG.md`, and this checkpoint.
  The M2.2 `RX7600_EVIDENCE_GAPS.md` and M2.1 IP/firmware map were not
  changed. No parser or driver implementation was modified.
- **Last test command/output:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`:
  `Ran 22 tests in 0.331s`
  `OK`
  Exit 0; all 22 test methods passed; 0 failures, 0 skips. Tests use
  synthetic data and do not access GPU hardware.
- **Documentation review:** Seven local Markdown links across README,
  research report, IP/firmware map, evidence-gap report, this audit, worklog
  and checkpoint were checked; all resolved. The documented command works
  from the documented directory. Strict vs compatibility behavior and
  unsupported format limitations remain distinguished. Parser success is not
  represented as hardware or macOS compatibility evidence.
- **Verified Git state:** At 2026-10-20T19:47:43.850+07:00, branch `main`,
  HEAD `d1f7bad8408a0a2c016b067c477422975a59de44`; upstream
  `origin/main` and merge base resolve to the same commit; branch status
  `+0 -0`, explicit divergence `0 0`. No staged or tracked diffs. Fifteen
  untracked paths, all under `tools/ipdisc/`: `README.md`, `RESUME.md`,
  `RX7600_EVIDENCE_GAPS.md`, `RX7600_IP_FIRMWARE_MAP.md`,
  `RX7600_REPRODUCIBILITY_AUDIT.md`, `RX7600_RESEARCH.md`, `WORKLOG.md`,
  `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`. No paths outside the authorized directory were
  reported. Existing cache artifacts were preserved.
- **Discrepancies:** M2.2's reported Git state and upstream relationship
  matched this audit. One initial `git rev-list` call without a range yielded
  a misleading `0 17`; exact ref OIDs, merge base, porcelain status, and
  `HEAD...origin/main` range all confirmed `0 0`.
- **Remaining blockers:** No provenance-backed board ID/subsystem/VBIOS
  artifact, IP Discovery header/table/die/IP values, complete firmware set or
  firmware provenance/licensing, or macOS build/initialization/hardware
  evidence. No physical GPU, firmware, or connected-device interface was
  accessed.
- **Single safest next action:** If a trusted, board-attributed RX 7600 IP
  Discovery artifact already exists, submit it for offline strict parsing;
  otherwise keep the evidence gap open. Do not probe the GPU.
- **Explicitly unauthorized:** GPU/hardware/PCI/MMIO/VRAM/firmware access;
  external binary download/execution; driver changes or routing `1002:7480`
  through Navi 48; plist, EFI, boot-argument, or system-configuration edits;
  changes outside `tools/ipdisc/`; implementation work; dependency install;
  stage, commit, clean, reset, stash, or delete; claims of RX 7600 macOS
  support.


## 2026-10-30 update — physical IP Discovery artifact recovered

Current state supersedes older checkpoint statements that no raw IP Discovery
binary was present. A previous Linux capture was found on the mounted
`GameAndFile` volume under `Cobalt64/RX7600-Linux-Evidence` and copied
byte-for-byte into the CherryRed repo:

- `evidence/rx7600-amdgpu-discovery-2026-10-23.bin` (10,240 bytes)
- SHA-256: `4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`
- Strict parser: PASS; one die, 39 IP records, 30 unique numeric HWIDs.
- Parsed JSON and hash-bound provenance sidecar are adjacent to the binary.
- Detailed report: `evidence/RX7600_IP_DISCOVERY_ANALYSIS_2026-10-30.md`.
- GC 11.0.2; SDMA0 6.0.2; MMHUB 3.0.2; HDP 6.0.1; UMC 8.11.0 (instances
  0-3); NBIF 4.3.1; MP0/MP1 13.0.7; OSSSYS 6.0.2; DMU 3.2.1.
- Associated user-maintained Linux manifest/logs record Archcraft kernel
  7.2.11-arch1-1, PCI 0000:03:00.0, 1002:7480, subsystem 148c:2421, and
  amdgpu. Provenance remains UNVERIFIED pending independent authentication.

Metal Toolchain 17F42 is installed. Generic Metal probe builds but runtime
returns exit 2 because MTLCreateSystemDefaultDevice() is nil. Exact RX7600
probe builds but runtime returns exit 2 because MTLCopyAllDevices() is empty.
System Profiler reports RX 7600 PCI presence but No Kext Loaded. Metal hardware
dispatch remains BLOCKED; no CPU fallback or spoofing is accepted.

Read-only Linux capture helper: `tools/ipdisc/capture_rx7600_ip_discovery.sh`.
It requires Linux with the exact device bound to amdgpu and debugfs mounted.
Do not run it on macOS. No VBIOS has been acquired or authenticated.

Validation on 2026-10-30: IP Discovery parser 30/30; firmware manifest tests
9/9; Linux inventory collector tests 8/8; Navi33 host suite 172 checks passed;
IPD bounds 8 fixtures + 512 malformed inputs; Navi48 isolation 4/4; PCI probe
contract 4/4; Metal build contract PASS; git diff --check PASS. No hardware
initialization, kext load, firmware load, BAR/MMIO, GPU reset, EFI/SIP/AMFI
change, or reboot was performed.
