# IP Discovery Tool Worklog

Append new entries in chronological order. Do not rewrite or remove earlier
entries; add a later entry if a correction is needed. Older M1/M1.1 facts below
are attributed to the prior session report where not independently reproduced
in the entry that records them.

## M1 SCOPE NOTE

M1 scope: tools/ipdisc/ only (7 new files: ipdiscovery.py, mkfixture.py,
test_ipdiscovery.py, dump.py, cpp_parity.md, parity_cases.json, README.md).

M1 DELIVERED SCOPE (commit 8eb0704):
Commit 8eb0704 "Add Navi 33 research sources and the IP-discovery tooling"
contains 39 files total, exceeding the approved M1 scope:
- 7 approved M1 files under tools/ipdisc/ (present, correct)
- 19 research documentation files under tools/ipdisc/ (out of approved M1 scope)
- 13 files under src/navi33-research/ (entirely separate research scaffold, out of M1 scope)

The src/navi33-research/ scaffold is accepted as a separate deliverable under
commit 8eb0704 and is NOT part of M1 acceptance. M1 acceptance is limited to
the 7 tools/ipdisc/ files, tested at 30/30 pass.

Pre-existing kext safety changes in src/navi48-bringup/ are out of M1 scope:
- PCI identity re-check in Navi48Bringup::start()
- IP discovery bounds checking (tableEnd field)
- Overflow protection in regByteOffset()

These changes were committed separately as commit 819724f and predate the
current session.

UNVERIFIED — C++ KEXT SAFETY GUARDS (commit 819724f):
The bounds checking, overflow protection, and PCI identity re-check in
commit 819724f have NO test coverage in the current tree. The 30 Python
tests in tools/ipdisc/test_ipdiscovery.py exercise the Python parser,
not the C++ kext implementation. These guards compile cleanly but are
not verified at the edges. Test coverage will be added in a future
milestone (M6: Build and integration validation) when a macOS build
environment is available. Do not treat these guards as tested or rely on
them for safety-critical decisions until coverage is added.

PUSH HOLD DECISION (session 2026-10-21):
Commits 819724f, c21c12b, c1d5891, and 74910bb are HELD locally and not
pushed to origin/main. Rationale:
- 819724f adds untested safety-critical C++ code to the kext
- Pushing without test coverage could give false confidence
- M6 (build and integration validation) is the appropriate milestone to
  add C++ test coverage and push
- Documentation commits (c21c12b, c1d5891, 74910bb) depend on 819724f
  for context and are held together to maintain commit ordering
Authorization received to hold all 4 commits until M6.

## M1 CLOSE-OUT (session 2026-10-21)

**Milestone:** M1 — Offline IP Discovery Parser and Test Foundation

**Objective:** Create a host-only, offline IP discovery parser with Python standard library only, synthetic fixtures, and comprehensive unit tests. No hardware access, no kext modification.

**Delivered (approved scope — 7 files under tools/ipdisc/):**
- `tools/ipdisc/ipdiscovery.py` — Python IP discovery parser (377 lines)
- `tools/ipdisc/mkfixture.py` — Synthetic fixture generator (382 lines)
- `tools/ipdisc/test_ipdiscovery.py` — Unit tests (623 lines)
- `tools/ipdisc/dump.py` — CLI tool (297 lines)
- `tools/ipdisc/cpp_parity.md` — C++ compatibility documentation (80 lines)
- `tools/ipdisc/parity_cases.json` — Parity test manifest (103 lines)
- `tools/ipdisc/README.md` — Tool documentation (191 lines)

**Test Results:**
- Command: `cd tools/ipdisc && python test_ipdiscovery.py`
- Python: 3.12.10
- Result: 30 tests passed in 1.571s
- Coverage: Valid/invalid fixtures, bounds checking, checksums, offsets, compatibility modes, CLI, fuzzing

**Scope Issues:**
- Commit 8eb0704 contained 39 files total, exceeding approved M1 scope
- 19 research documentation files under tools/ipdisc/ (out of approved scope)
- 13 files under src/navi33-research/ (separate research scaffold, out of M1 scope)
- Research scaffold accepted as separate deliverable, not part of M1 acceptance
- M1 acceptance limited to 7 tools/ipdisc/ files (documented in c1d5891)

**Out-of-Scope Changes (documented, not reverted):**
- Commit 819724f: C++ kext safety guards (PCI identity re-check, bounds checking, overflow protection)
- Status: UNVERIFIED, no test coverage (documented in 74910bb)
- Held locally until M6 adds C++ test coverage

**Push Status:**
- Commits 819724f, c21c12b, c1d5891, 74910bb, e2c6b04 held locally
- 5 commits ahead of origin/main
- Rationale: untested safety-critical code should not be pushed without test coverage
- Authorization received to hold until M6 (documented in e2c6b04)

**M1 Status:** COMPLETE

**Constraints Satisfied:**
- Python standard library only ✓
- No hardware access ✓
- No modification to existing kext files (M1 scope) ✓
- Synthetic fixtures only (no real hardware dumps) ✓
- All tests pass ✓

**What Was NOT Done:**
- M2 evidence review: not approved this session
- C++ guard testing: deferred to M6 (requires macOS build environment)
- Push to origin/main: held pending M6

**Next Milestone:** Requires explicit user approval

## 2026-10-21 — Phase 25 board presence verified; start() identity guard restored

- **Repository state:** branch `main`, HEAD
  `8eb07046283c4cd433496c21277a1ced9beeee2f` at start; working tree clean.
  Six files modified in total (listed in the final status below); nothing
  staged or committed.
- **Board presence and PCI identity now VERIFIED (read-only Windows device
  model).** The earlier checkpoint recorded `1002:7480` as `UNVERIFIED —
  hardware presence unknown` because a Windows host has no `lspci`/`dmesg`/
  sysfs, and the previous `Get-Command lspci,dmesg,amdgpu-discover,atomtree`
  probe was a tool-availability check that could not say anything about the
  device. That gap was closed with a native read-only OS inventory instead of
  a substitute for Linux tools:
  - `Get-CimInstance Win32_VideoController` → `AMD Radeon RX 7600`,
    `PNPDeviceID`
    `PCI\VEN_1002&DEV_7480&SUBSYS_2421148C&REV_CF\6&EB99E4&0&00000009`,
    driver `32.0.31041.1004`, `AdapterRAM` `4293918720`.
  - `Get-PnpDevice` filtered on `VEN_1002` → `7480` Display `OK`, plus `1479`
    downstream switch port `OK`, `1478` upstream switch port `OK`, `AB30` HD
    audio `OK`. The latter three are recorded as raw observed AMD PCI entries;
    no relationship to the target board is asserted beyond what the OS
    reported.
  - Established: physical target present; vendor `1002`, device `7480`,
    subsystem `148C:2421`, revision `CF`, instance `6&EB99E4&0&00000009`; a
    display driver is bound.
  - **Not** established: IP Discovery records or versions, firmware requests,
    loaded/running firmware, VBIOS identity, VRAM size, macOS anything.
    `AdapterRAM` is a 32-bit-limited WMI field and is kept as a raw
    observation only. The subsystem vendor's brand/part number is **not**
    asserted: resolving `148C:2421` needs `pci.ids`, which is absent locally
    (M2.5), and a name lookup would need network access requiring separate
    authorization.
  - Boundary kept: PCI configuration space, ROM BAR, MMIO, VRAM, firmware
    interface, and driver state were **not** read or written; no Linux command
    or sysfs path was simulated.
- **Offline search for a pre-existing VBIOS/atom ROM artifact: none found.**
  Read-only `Get-ChildItem -Recurse -Include *vbios*,*atom*,*.rom,*atibios*`
  over `C:\Windows\System32\DriverStore\FileRepository`, `C:\AMD`,
  `C:\Program Files\AMD` returned no matches (`C:\AMD` and
  `C:\Program Files\AMD` do not exist in this search context). So no local
  source of the embedded IP Discovery table exists. Acquiring one would
  require either a ROM BAR read (hardware probe) or a third-party dump tool
  (download); **neither was attempted**, and both need separate explicit
  authorization.
- **Regression failure found at HEAD and fixed.**
  `python -B src/navi33-research/tests/test_navi48_isolation.py` failed 1 of 4
  at a clean checkout of `8eb0704`:
  `test_start_revalidates_identity_before_hardware_setup` —
  `assertIsNotNone(identity_guard)` returned `None`. Root cause confirmed by
  source inspection: `Navi48Bringup::start()`
  (`src/navi48-bringup/src/Navi48Bringup.cpp:7342`) contained **no**
  `configRead32(kIOPCIConfigVendorID)` re-check and fell straight from
  `if (!pciDevice)` into `pciDevice->setMemoryEnable(true)` →
  `latchPciBars()` → `mapRegisters()`. The file's only
  `kIOPCIConfigVendorID` read was inside `probe()` (line 7333).
  `git log -- Navi48Bringup.cpp` shows its last change is `dc128be`
  (2026-10-08), so the guard the Phase 14 checkpoint reported as added
  ("`Navi48Bringup::start()` now rechecks AMD VID and Navi48 IDs `0x7550`/
  `0x7551` before PCI memory enable, BAR access, or MMIO mapping") was **never
  committed and is not present at HEAD**, while the regression test that
  checks it was committed in `8eb0704`. The Phase 14 claim is therefore not
  reproducible at HEAD.
- **Fix applied:** restored the identity re-check in `start()` immediately
  after the null-provider check and **before** `setMemoryEnable`, using the
  same expression and types as `probe()`:
  `uint32_t vendorDevice = pciDevice->configRead32(kIOPCIConfigVendorID);`
  `uint16_t device = static_cast<uint16_t>(vendorDevice >> 16);` rejecting
  `(vendorDevice & 0xffff) != 0x1002 || (device != 0x7550 && device != 0x7551)`,
  with an `N48LOG` refusal line. Scope-block braces are indented so the test's
  non-greedy `(?P<body>.*?)\n\}` body capture still spans the whole function.
  This is defense-in-depth only: the `Info.plist` personalities match
  `0x75501002`/`0x75511002` exclusively and `probe()` already rejects other
  IDs, so no path was shown to have driven non-Navi48 hardware. **No
  `1002:7480` match, no Navi33 code, no plist, and no behavior change for
  valid Navi48 IDs were made.**
- **Second regression failure at HEAD, same root cause, fixed: parser bounds
  hardening was also never committed.** After the Navi48 fix, a full
  `cmake --build` plus `ctest` for both configurations showed
  `ipdiscovery_bounds_tests` failing 4 of 7 cases in Debug **and** Release
  (`overflowing base addition is rejected without wrapping output`,
  `byte-offset multiplication overflow is rejected without changing output`,
  `base array beyond declared table is rejected`, `record header beyond
  declared table is rejected`). `git log -- src/navi48-bringup/src/ipdiscovery.cpp`
  shows its last change is also `dc128be` (2026-10-08), so the bounds work the
  Phase 21 checkpoint described ("bounds the table to the declared binary and
  lazy record/base-array reads to the declared table") **was never committed
  either**, while the 7-case test that asserts it was committed in `8eb0704`.
  Phase 21's own record explains how: it lists
  `src/navi48-bringup/src/Navi48Bringup.cpp` as an *uncommitted* tracked
  working change at `d1f7bad` with "nothing staged", and `8eb0704` is
  "39 files changed, 9794 insertions(+)" with **zero deletions** — it captured
  only the newly added `src/navi33-research/` and `tools/ipdisc/` files. The
  tracked-source edits behind Phases 14–24 were left unstaged and the working
  tree was already clean when this session started, so the test files landed
  without the source changes they assert. That is a repository-hygiene defect
  alongside the code defect.
  Practically, `regByteOffset()` computed `(base + dwordOffset) * 4` in
  `uint32_t` with no overflow check, so a refused request instead returned a
  wrapped, valid-looking MMIO byte offset, and `ipAt()` walked records against
  the whole caller buffer rather than the declared `table_info` size, so a
  truncated table let records run into the bytes that follow it. Both matter
  because the input is an untrusted injected ROM image.
- **Parser fix applied:** added a `tableEnd` member set in `validateBinary()`
  from `ipd + ipdSize`, reset in `init()`, plus a `ipd > size || ipdSize >
  size - ipd` check so the declared table cannot leave the declared binary.
  `ipAt()` now requires `p <= tableEnd && tableEnd - p >= 8` before a record
  header and `tableEnd - p >= 8 + 4 * nbase` before its base array.
  `regByteOffset()` now rejects `base > UINT32_MAX - dwordOffset` (addition
  overflow) and `dwords > UINT32_MAX / 4` (multiplication overflow) **before**
  writing `byteOffset`, so the caller's value survives a refusal. Thresholds
  keep `dwords == 0x3FFFFFFF` accepted (`byteOffset == 0xFFFFFFFC`), matching
  the test's largest-representable case; the rejection ceiling is ~4 GiB of
  MMIO byte offset, far above this hardware's BAR, so no legitimate request
  changes behavior. No IP version dispatch, no firmware path, no Navi33 code,
  and no Navi48 behavior for valid inputs were changed.
- **Verification after the changes:** MSVC x64 Debug and Release rebuilds exit
  0. `ctest` (full path to the bundled `CMake/bin/ctest.exe`, since `ctest` is
  not on `PATH`) → **4/4 passed in each configuration, 0 failed** (`0.22s`
  each); before the parser fix it was 3/4 with `ipdiscovery_bounds_tests`
  failing in both. Navi48 isolation from repository root → exit 0,
  `Ran 4 tests in 0.007s`, `OK` (was 1 failure before). Parser suite from
  `tools/ipdisc/` → exit 0, `Ran 30 tests in 1.577s`, `OK`.
  `git diff --check` → exit 0. **The two Navi48 kext edits are compiled only
  where they are part of the portable CMake target.** `ipdiscovery.cpp` is
  compiled and tested by `ipdiscovery_bounds_tests`; `Navi48Bringup.cpp` is
  **not** compiled anywhere here — it is Apple kext source requiring the macOS
  11 SDK, `xcrun`, and `MacKernelSDK`, none of which exist on this Windows
  host, so the `start()` guard's verification remains a static source-contract
  test only.
- **Evidence boundary:** board **presence and PCI identity** are now verified;
  all IP Discovery and firmware rows in `RX7600_NAVI33_EVIDENCE.md` remain
  **UNVERIFIED**. No IP Discovery artifact, no firmware state, no macOS
  build/initialization/display/Metal evidence. Phase 4 dispatch implementation
  remains **not justified**.

## 2026-10-21 — Phase 24 macOS build-feasibility handoff

- **Repository state:** branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; existing Navi48 tracked changes
  and pre-existing untracked `src/navi33-research/` and `tools/ipdisc/` trees
  preserved. Nothing staged or committed.
- **Navi48 build contract inspected:** `src/navi48-bringup/Makefile` targets
  x86_64, `x86_64-apple-macos11.0`, minimum macOS 11.0. It invokes `xcrun`
  for the macOS SDK, `clang`/`clang++`, and `make`; sets C++17,
  `-fno-exceptions`, `-fno-rtti`, `-fcheck-new`,
  `-fno-c++-static-destructors`, `-nostdinc`, private kernel defines,
  `-mkernel`, and `-fapple-kext`; links with `-nostdlib`, kext linker mode,
  `-lkmodc++`, and `-lkmod`. Header search points to
  `../RDNA4FB/MacKernelSDK/Headers`, library search to
  `../RDNA4FB/MacKernelSDK/Library/x86_64`. `Info.plist` sets KPI library
  versions to `20.0.0` and personalities to Navi48 IDs only. Existing
  `src/xlat12`, `src/shadercache`, and `src/dcn41`, plist, Makefile, source
  files, `LICENSE`, `NOTICE`, and `tools/embed-firmware.py` are present.
- **Missing/unverified dependencies:** this is a Windows host; no macOS,
  `xcrun`, Apple Clang, macOS SDK, or GNU make could be established here.
  Referenced `src/RDNA4FB/MacKernelSDK/`, its `Headers`, and
  `Library/x86_64` are absent, so headers and `libkmodc++`/`libkmod` are not
  available in this checkout. `src/navi48-bringup/firmware/` is absent. The
  Makefile requires ten named blobs: six `gc_12_0_1_*`, two `psp_14_0_3_*`,
  `sdma_7_0_1`, and `smu_14_0_3`. The Makefile's referenced
  `BUILDING.md` and `tools/fetch-firmware.sh` are absent; the checked-in
  `tools/embed-firmware.py` exists. SDK/firmware license terms and blob
  provenance are unverified because the artifacts are absent.
- **License evidence:** local `src/navi48-bringup/LICENSE` is BSD 3-Clause.
  `NOTICE` attributes RDNA4FB source under BSD-3 and mac-amdgpu-derived
  adaptation to MIT at commit `3bdeed2de940504ad6bd1bac718d5de2f65ddb83`;
  Linux amdgpu/GPL-2.0 is recorded as reference only, no code copied. The
  missing MacKernelSDK and firmware require their own exact license/provenance
  review before use.
- **Handoff:** added `src/navi33-research/MACOS_VALIDATION_HANDOFF.md` with
  the prerequisite matrix and a direct Apple-Clang x86_64 compile/link test
  executable command using the macOS 11 deployment target and local portable
  core/test sources. It uses the normal host C++ runtime and explicitly does
  not test kext linkage or kernel ABI. Apple Silicon x86_64 execution is
  optional and requires an already available execution path. The handoff
  separates the Navi48 kext target, identifies its generated files/ad-hoc
  signing and destructive `clean` target, and states what successful portable
  and kext builds do not prove.
- **Verification:** Debug and Release builds succeeded; CTest passed 4/4 in
  each (0.16s each). `python -B test_ipdiscovery.py` from `tools/ipdisc/`
  exited 0: `Ran 30 tests in 1.778s`, `OK`. Navi48 isolation exited 0:
  `Ran 4 tests in 0.010s`, `OK`. The Release portable-core audit completed
  with its explicit Windows-only/no-Apple-compile-or-link disclaimer.
  Debug and Release portable-core audits both completed and explicitly
  reported Windows-only evidence. `git diff --check` exited 0. No macOS, kext,
  or hardware build/test ran.
- **Remaining gates:** an authorized Mac must first run the documented
  portable x86_64 compile/link diagnostic. Any kernel-link test requires
  provenance/license-reviewed MacKernelSDK and exact Apple C++ runtime
  investigation. The Navi48 kext additionally needs all mandatory firmware
  and an independent authorization/review before its build/signing steps.
  macOS compilation/linking, kernel ABI, RX 7600 initialization, display
  acceleration, and Metal remain unverified.

## 2026-10-21 — Phase 23 borrowed-view lifetime verification

- **Initial state:** branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; existing tracked changes in
  Navi48 NOTICE/source/parser files and pre-existing untracked
  `src/navi33-research/` and `tools/ipdisc/` trees were preserved.
- **Lifetime trace:** the only public `std::string_view` fields are
  `Navi33DeviceIdentity::architectureLabel`,
  `IpBlockDescriptor::{blockName,evidenceSource}`, and
  `FirmwareRequirement::identifier` (`include/Navi33Research.hpp`). There are
  no `string_view` parameters or return types in the core. `selectArchitecture`,
  `validateIpBlockEvidence`, and `validateFirmwareEvidence` read these fields
  synchronously (`src/Navi33Research.cpp`) and return validation structs
  containing `const char*`, not a view. All API-produced result messages are
  string literals with static storage duration. `rx7600ResearchIdentity()`
  also initializes its architecture-label view from the static literal
  `"Navi 33"`. `Navi33DriverCore::identify` passes the identity by const
  reference to the synchronous validator and stores only its `DeviceValidation`
  result; no input view is copied into driver-core state. Its lifecycle
  messages are literals, and `selection()` returns the stored result by
  reference, with only a static diagnostic pointer. Callers must maintain
  backing storage during validation/identify; the core does not retain it.
- **Tests/docs:** added valid identity/IP/firmware tests backed by local
  `std::string` owners, returning only static-result objects before those
  owners leave scope. Tests inspect the returned static diagnostics afterward,
  never a destroyed view. Added empty architecture-label, IP block-name, IP
  source, and firmware-identifier cases; malformed nonempty identity text is
  rejected. Lifecycle regression passes a locally owned identity label, lets
  it expire after `identify`, then continues and cleans up successfully.
  Repeated prepare/start calls after backend failure are also verified not to
  repeat platform callbacks or cleanup. The public header now explicitly
  documents synchronous borrowing and no retention. The audit banner now says
  Windows host/MSVC-DUMPBIN only and expressly disclaims macOS compile/link
  inference. CMake was reviewed and left unchanged: it defines portable C++17
  library/test targets only; it does not configure or compile an Apple kext.
- **Verification:** using the existing Visual Studio 2022 x64 CMake build:
  `cmake --build build/navi33-research-msvc --config Debug` succeeded and
  `ctest --test-dir build/navi33-research-msvc -C Debug --output-on-failure`
  passed 4/4 (0.28s); Release build succeeded and CTest passed 4/4 (0.28s).
  `python -B test_ipdiscovery.py` from `tools/ipdisc/` exited 0:
  `Ran 30 tests in 1.623s`, `OK`. From the repository root,
  `python -B src/navi33-research/tests/test_navi48_isolation.py` exited 0:
  `Ran 4 tests in 0.007s`, `OK`. Debug and Release
  `audit_portable_core.py --build-dir build/navi33-research-msvc --config
  <Debug|Release>` completed with the explicit Windows-only disclaimer; both
  report no owning `std::string` in core source and no selected allocation,
  throw, string-length-error, or RTTI symbol category in the two core objects.
  `git diff --check` exited 0. The build retains the known MSVC D9025
  exception-option override warnings.
- **Limitations:** lifetime safety is established by code structure and
  host-side tests for the synchronous API, not by an Apple build. macOS
  compilation/linking, Apple kernel ABI compatibility, RX 7600 initialization,
  display acceleration, and Metal remain unverified. No hardware, matching,
  kext, EFI, or security configuration was accessed or changed.

## 2026-10-21 — Phase 22 borrowed text API and object-level dependency map

- **Initial state:** branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; tracked changes in
  `src/navi48-bringup/NOTICE`, `src/navi48-bringup/src/Navi48Bringup.cpp`,
  `src/navi48-bringup/src/ipdiscovery.cpp`, and
  `src/navi48-bringup/src/ipdiscovery.hpp`; `src/navi33-research/` and
  `tools/ipdisc/` were already untracked. No files were staged.
- **Pre-change dependency trace:** Release `dumpbin /symbols` showed
  `operator new`, sized `operator delete`, `std::_Xlength_error`,
  `_CxxThrowException`, and `type_info::\`vftable'` as undefined references in
  both `navi33_research.dir\Release\Navi33Research.obj` and
  `navi33_research.dir\Release\Navi33DriverCore.obj` (same ownership in
  Debug). The shared owning fields were in `Navi33Research.hpp`: identity
  label, IP block/evidence text, firmware identifier, and validation messages.
  `Navi33Research.cpp` built/returned owning message strings; the direct
  lifecycle allocation site was `Navi33DriverCore.cpp`'s
  `std::string("Invalid lifecycle transition: ") + operation`. The allocation
  and length-error references arise from MSVC `std::string` machinery
  instantiated into both translation units; throw/exception metadata accounts
  for `_CxxThrowException` and the type-info vftable despite `/EHs-c- /GR-`.
  This does not show that any exception ran or that the compiler flags were
  ineffective.
- **Implementation:** public input text fields now use borrowed
  `std::string_view`; callers keep backing storage alive through the
  synchronous validation call. API-produced diagnostic text is static
  `const char*`. Lifecycle transition diagnostics remain byte-for-byte the
  same, with their full strings selected as literals rather than concatenated.
  State transitions, callback ordering, and cleanup logic are unchanged.
  Regression assertions cover exact validation and lifecycle diagnostic text.
  `audit_portable_core.py` now inspects each Navi33 object file, reports
  ownership of the selected undefined-symbol categories, scans all core source
  and public headers for owning `std::string`, and labels object/archive
  references separately from final executable imports.
- **Post-change findings:** Debug and Release core object files have no
  undefined references in the selected `operator new/delete`,
  `_CxxThrowException`, `std::_Xlength_error`, or RTTI-vftable categories.
  Release static-library output still includes `memcmp`/`strlen` and an
  intra-library `selectArchitecture` reference; Debug additionally has
  `_RTC_*` and CRT diagnostic references. These archive references are
  resolved in the successfully linked test executables. Release imports
  `VCRUNTIME140.dll`, UCRT API-set DLLs, and `KERNEL32.dll`; Debug imports
  `VCRUNTIME140D.dll`, `ucrtbased.dll`, and `KERNEL32.dll`. The audit reports
  Windows host facts only and is not a macOS ABI/link test.
- **Verification:** Visual Studio 2022 x64 Debug and Release builds succeeded
  using the already installed bundled CMake 3.31.6. CTest passed 4/4 in Debug
  (0.26s) and Release (0.24s), including the Navi48 isolation and parser
  bounds tests. `python -B test_ipdiscovery.py` passed 30 tests
  (`Ran 30 tests in 1.452s`, `OK`); `python -B
  src/navi33-research/tests/test_navi48_isolation.py` passed 4
  (`Ran 4 tests in 0.008s`, `OK`). Debug and Release audit commands both
  completed successfully. `git diff --check` passed. MSVC continues to emit
  the previously known D9025 exception-option override warnings.
- **Limitations and boundary:** `std::optional`, `std::string_view`,
  virtual callbacks, and Windows C runtime references remain. No Apple/macOS
  source was compiled or linked; Apple kernel ABI compatibility is unverified.
  RX 7600 initialization, display acceleration, and Metal remain unverified.
  No Navi48 matching/initialization, hardware, firmware, EFI, or system
  configuration was changed or accessed. No files were staged or committed.

## 2026-10-21 — Phase 21 portable-core dependency audit and C++ parser bounds

- **Repository state before edits:** branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; two tracked working changes
  (`src/navi48-bringup/NOTICE` and `src/navi48-bringup/src/Navi48Bringup.cpp`)
  and 35 untracked paths were present. Nothing was staged. All pre-existing
  sources, reports, and Python cache files were preserved. A Python
  `py_compile` check created `src/navi33-research/tools/__pycache__/audit_portable_core.cpython-312.pyc`;
  it was retained.
- **Portable-core source audit:** the public research/lifecycle interfaces
  depend on `std::string` (identity labels, evidence fields, lifecycle
  messages), `std::optional`, and virtual `Navi33Platform` callbacks. Strings
  may allocate; the implementation has no direct `new`/`delete` expression,
  declares no thread/mutex API, and the reviewed translation units have only
  `inline constexpr` namespace constants and no dynamic global objects.
  Exception/RTTI compiler macros remain disabled in the MSVC compile logs.
  No core API was changed because the Windows audit shows host-runtime
  dependencies and no Apple kernel toolchain is available to validate a
  replacement ABI.
- **Reproducible audit utility:** added
  `src/navi33-research/tools/audit_portable_core.py`. It reads an existing
  Visual Studio CMake build directory without modifying it; summarizes final
  `/GR-` and `/EHs-c-` options from compiler logs; inspects the static library
  with `dumpbin /symbols`; reports imported DLLs from the test executable;
  and labels all results preliminary Windows evidence, not macOS ABI evidence.
  The Release audit found 16 undefined external symbol names, including
  allocator/delete operators, `_CxxThrowException`, `std::_Xlength_error`,
  `type_info` vtable, and memory/string functions. The test executable imports
  MSVC runtime DLLs (`MSVCP140.dll`, `VCRUNTIME140.dll`, UCRT API-set DLLs)
  plus `KERNEL32.dll`. This demonstrates that disabling compiler exceptions
  and RTTI does not remove the C++ standard/runtime dependencies. Debug
  additionally reports debug runtime symbols. No macOS suitability follows.
- **C++ parser fix:** `src/navi48-bringup/src/ipdiscovery.cpp/.hpp` now tracks
  the declared IP Discovery table end, rejects a table that extends past its
  binary, rejects the die header if it overlaps the fixed 80-byte table header
  or lies outside the table, and checks each requested record's fixed header
  and complete declared base array against the table end. Read helpers use
  subtraction-based buffer bounds. Record validation remains lazy; returned
  bases remain clamped to 16 and traversal size uses the full count. No
  IP Discovery version/checksum compatibility claims were added.
- **Regression coverage/docs:** added
  `src/navi33-research/tests/test_ipdiscovery_bounds.cpp` and its CMake/CTest
  target. Four synthetic cases cover a valid table/record, base-array overrun,
  record-header overrun beyond declared table with input padding, and table
  extent past declared binary. Updated the Navi33 README, `tools/ipdisc/
  cpp_parity.md`, `parity_cases.json`, and `README.md` to document the host
  runtime findings and distinguish the older Python compatibility snapshot
  from the newly bounded C++ parser. These fixtures are not RX 7600 artifacts.
- **Verification:** configured with the existing VS 2022 x64 generator and
  built Debug and Release successfully. CTest Debug: 4/4 passed in 0.21s;
  CTest Release: 4/4 passed in 0.35s. Direct C++ parser bounds executable:
  `IP Discovery bounds tests: 4 passed, 0 failed`. Python parser:
  `python -B test_ipdiscovery.py` -> `Ran 30 tests in 1.574s`, `OK`.
  Navi48 isolation: 4 passed in 0.007s. Audit command in both Debug and
  Release completed; all eight logged Navi33 compile commands across the two
  configurations end with `/GR-` and `/EHs-c-`. `git diff --check` passed
  after source/doc updates.
- **Unverified / next milestone:** Windows MSVC symbols do not establish
  Apple kernel linkage. The portable core is not yet suitable for direct
  kernel inclusion due standard-library, allocation, virtual-dispatch, and
  runtime symbols. On an authorized macOS/Xcode host with the exact licensed
  MacKernelSDK, first perform a portable-core-only compile/link audit under
  the kext flags and inspect its undefined symbols. A separate kernel-neutral
  API should be designed only after that evidence establishes which runtime
  facilities are actually available. RX 7600 physical IP data, firmware,
  driver binding, acceleration, display, and Metal remain unverified.

## 2026-10-21 — Phase 22 final parser regression expansion

- Expanded the C++ synthetic parser cases to cover a valid v2 counted table
  list in addition to valid v1, rejection of a v1 table overlapping the full
  six-entry list, declared-table base-array and record-header overruns, and a
  table extending beyond the declared binary. The direct test output now
  reports five fixture cases. Updated parity documentation to describe the
  current full v1/v2 list boundary checks.
- **Validation:** an initial build caught MSVC C4244 in the v1/v2 test loop;
  explicitly typed fixture literals resolved the warning-as-error. Final
  Debug and Release builds succeeded; CTest passed 4/4 in each configuration
  (0.21s each). Direct parser output:
  `IP Discovery bounds checks: passed (5 fixture cases)`. Final Python parser
  run: `Ran 30 tests in 1.782s`, `OK`. Navi48 isolation: 4 passed in 0.008s.
  Debug/Release audits each found eight Navi33 compile records with effective
  `/GR-` and `/EHs-c-`; the Release library retained 16 undefined external
  symbols and the MSVC/UCRT imports described in Phase 21. `git diff --check`
  exited 0.

## 2026-10-21 — Phase 20 compile-mode contract and parser reuse boundary

- **Repository state before edits:** branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; tracked working changes were
  limited to `src/navi48-bringup/NOTICE` and
  `src/navi48-bringup/src/Navi48Bringup.cpp`. The Navi33 research tree and
  `tools/ipdisc/` sources/reports, including two `__pycache__` files, were
  already untracked. Nothing was staged. All were preserved.
- **macOS build gate:** checked the repository-local
  `src/RDNA4FB/MacKernelSDK` path and expected host tools; the SDK path is
  absent, and `xcrun`, `clang++`, `make`, `codesign`, and `dwarfdump` are
  unavailable on PATH. The Navi48 Makefile requires an x86_64 macOS 11 SDK,
  Apple Clang/IOKit headers, the external MacKernelSDK headers and
  `x86_64` libraries, and firmware files before its kext can link. No
  Navi33-specific SDK, plist, or IOKit service exists. No macOS build ran.
- **Existing IOKit boundary:** source uses the `IOService`, `IOPCIDevice`,
  `IODeviceMemory`, `IOMemoryMap`, work-loop, and interrupt-source classes.
  The build explicitly enables `KERNEL_PRIVATE`, `DRIVER_PRIVATE`,
  `-mkernel`, and `-fapple-kext`; it links `kmodc++`/`kmod`. The plist also
  contains Apple AMD accelerator personality classes, which are not a
  documented independent Navi33 integration interface. Without the Apple SDK,
  this source inspection does not establish symbol visibility/stability or a
  viable third-party acceleration path. Existing driver operations include
  BAR/config reads, MMIO mapping, VRAM aperture mapping, interrupts, PSP/SMU,
  command engine and display/acceleration paths. The Makefile firmware list
  is explicitly gfx12.0.1 / PSP 14.0.3 / SDMA 7.0.1 / SMU 14.0.3 for the
  Navi48 target and cannot be treated as Navi33 requirements. BSD-3-Clause
  project material and MIT mac-amdgpu adaptations/provenance are recorded in
  `src/navi48-bringup/LICENSE` and `NOTICE`; Linux GPL source is declared
  reference-only in `NOTICE`, not copied.
- **Parser reuse audit:** `src/navi48-bringup/src/ipdiscovery.cpp` verifies
  the binary checksum and input-buffer bounds for individual reads, but does
  not verify the IP Discovery table checksum or confine IP/die record walks to
  the binary/table declared extents. Its own header documents verification
  against a Navi48 gfx12.0.1 board ROM. It is not suitable evidence for
  Navi33 compatibility or parsing arbitrary artifacts. The separate Python
  strict parser validates table/die/record extents and existing tests cover
  these constraints; that still does not establish the format data of a
  physical RX 7600. Navi48 parser behavior was not changed.
- **Code change:** added
  `src/navi33-research/include/Navi33BuildConfiguration.hpp`, with
  compile-time errors if exception or RTTI macros are active, and included it
  in both library translation units and both C++ test translation units.
  Extended the Navi33 README with the MSVC D9025 explanation and the parser
  reuse boundary. This provides evidence in each compiled target that the
  CMake no-exception/no-RTTI switches are effective instead of inferring
  effectiveness from flags alone.
- **MSVC flags:** the generated commands for the library and both test
  executables include `/GR-` and `/EHs-c-`; Visual Studio also supplies
  `/EHsc` before that final option. The build emits D9025 warnings for
  intentional overrides of `/EHs` and `/EHc`. MSVC `ExceptionHandling=Sync`
  project metadata is not itself decisive: the final compiler option and
  compile-time macro guards are. All translation units compiled, proving the
  exception/RTTI macros were absent for this MSVC build. No flag correction
  was warranted; warnings remain visible rather than suppressed.
- **Validation:** `cmake --build build/navi33-research-msvc --config Debug`
  exited 0 and rebuilt the library and both test executables with the macro
  guards active. `ctest --test-dir build/navi33-research-msvc -C Debug
  --output-on-failure` exited 0: 3/3 passed (`navi33_research_tests`,
  `navi33_driver_core_tests`, `navi48_routing_isolation`; 0.28 seconds).
  `python -B test_ipdiscovery.py` from `tools/ipdisc/` exited 0:
  `Ran 30 tests in 1.643s`, `OK`. `git diff --check` was run after the
  source/doc edits and passed. No hardware or privileged operation occurred.
- **Next milestone:** on an authorized macOS/Xcode host with the exact
  MacKernelSDK, first establish a reproducible minimal C++17 kext link of the
  portable core under the Navi48 Makefile's `-nostdinc`, `-fapple-kext`,
  no-exception/no-RTTI and `kmodc++` constraints, without a device personality
  or hardware callbacks. Review the standard-library ABI and unresolved
  symbols. Only after that gate and RX 7600-specific IP/firmware evidence
  exist should a separate opt-in Navi33 service be designed. RX 7600 macOS
  functionality remains unproven.

## 2026-10-21 — Phase 19 macOS integration gate and kernel-flag prerequisite

- **Source/API review:** The only macOS target is
  `src/navi48-bringup/Makefile` + `Info.plist`, an x86_64 IOKit kext with
  deployment target 11.0 and flags `-DKERNEL -DKERNEL_PRIVATE
  -DDRIVER_PRIVATE -mkernel -fapple-kext -fno-exceptions -fno-rtti`;
  `Navi48Bringup.hpp/.cpp` use `IOService`, `IOPCIDevice`,
  `OSDeclareDefaultStructors`, and `OSDynamicCast`. Its four match
  personalities remain limited to `1002:7550`/`1002:7551`. Existing
  `src/navi48-bringup/src/ipdiscovery.{hpp,cpp}` is freestanding and has no
  IOKit/allocation dependency, but its comments cite board-ROM validation on
  Navi48 and its implementation is compatibility-oriented, not the strict
  parser; reuse for Navi33 data needs a separate bounds/behavior review.
  Navi48 MMIO, VRAM, PSP, SMU, GFX/GMC/IH/SDMA, display, interrupts, and
  accelerator paths are generation-/device-specific and were not reused.
- **Platform decision:** No Navi33 IOKit service or match was added. The
  Makefile expects an external `src/RDNA4FB/MacKernelSDK`, which is absent;
  this Windows host has no `xcrun`, `clang++`, `make`, `codesign`, or
  `dwarfdump`. The existing APIs are only locally observed in Navi48 source;
  neither the external kernel SDK nor an Apple SDK is available to verify a
  separate service's declarations, link ABI, or packaging. A new uncompiled
  kext/plist match could create an unvalidated device-binding path. The
  smallest justified code prerequisite is compile-checking the portable core
  under the existing kext's no-exceptions/no-RTTI restrictions before defining
  a platform adapter. No third-party code was copied. Existing local project
  license material is BSD-3-Clause for Navi48 source; `NOTICE` records
  upstream licenses/provenance and was not altered in this phase.
- **Code change:** Added `/EHs-c-` and `/GR-` to all MSVC CMake targets and
  `-fno-exceptions -fno-rtti` to GNU/Clang targets, alongside existing
  warnings-as-errors. Updated `src/navi33-research/README.md` to clarify this
  checks compiler restrictions only and does not prove Apple kernel ABI
  compatibility. No IOKit/macOS source, plist, matching identifier, firmware,
  hardware, or Navi48 behavior was changed.
- **Compile/test evidence:** Reconfigured
  `cmake.exe -S src/navi33-research -B build/navi33-research-msvc -G "Visual Studio 17 2022" -A x64`
  (exit 0) and built with
  `cmake.exe --build build/navi33-research-msvc --config Debug` (exit 0)
  using bundled CMake `3.31.6-msvc6`, MSVC `19.44.35229.0` x64, Windows SDK
  `10.0.26100.0`. MSVC emitted D9025 warnings because `/EHs-c-` disables
  default `/EHs` and `/EHc`; compilation completed. The test commands used the
  sibling `ctest.exe` from the bundled CMake directory because `ctest` is not
  on PATH:
  `ctest.exe --test-dir build/navi33-research-msvc -C Debug --output-on-failure`
  exited 0, 3/3 passed (final run: 0.19s). The isolation script exited 0,
  `Ran 4 tests in 0.010s`, `OK`.
  `python -B test_ipdiscovery.py` from `tools/ipdisc/` exited 0:
  `Ran 30 tests in 1.676s`, `OK`. `git diff --check` exited 0.
- **Not proven:** No Apple Clang, macOS SDK, MacKernelSDK, IOKit service,
  kext link/package, kext load, or physical GPU test was run. The Windows
  build demonstrates C++ syntax and tests under MSVC no-RTTI/no-exception
  options only. It cannot establish Apple ABI or macOS integration.
- **Next gate:** Obtain an authorized macOS/Xcode Command Line Tools and the
  exact licensed/provenanced MacKernelSDK, then implement and build an
  opt-in-only, separate Navi33 IOKit service in its own target; do not add
  Navi33 IDs to Navi48 plist or initialization. Until then, retain the
  portable core and explicit unknown/unsupported capabilities.

## 2026-10-21 — Phase 18 first MSVC C++ build and lifecycle execution

- **New environment fact:** Visual Studio 2022 Build Tools appeared after the
  previous documented checks. CMake was bundled with that installation but
  not on `PATH`. Used
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
  version `3.31.6-msvc6`; compiler was MSVC `19.44.35229.0` x64
  (`cl.exe` under toolset `14.44.35207`); selected Windows SDK
  `10.0.26100.0`. No software was installed during this milestone.
- **Configure:** From repository root, ran the exact configure arguments
  supplied by the user, invoking the bundled CMake by absolute path:
  `cmake.exe -S src/navi33-research -B build/navi33-research-msvc -G "Visual Studio 17 2022" -A x64`.
  Exit 0. CMake identified MSVC 19.44.35229.0 and generated the Visual Studio
  2022 x64 project.
- **Build:** `cmake.exe --build build/navi33-research-msvc --config Debug`
  exited 0. The output compiled `Navi33Research.cpp`,
  `Navi33DriverCore.cpp`, `test_Navi33Research.cpp`, and
  `test_Navi33DriverCore.cpp`, then linked the library and both test
  executables.
- **CTest:** The first attempted wrapper invocation failed with
  `no such file or directory` because `ctest` was not on `PATH`; this was a
  command-discovery issue, not a test failure. Located the sibling
  `ctest.exe` in the already discovered CMake `bin` directory and ran
  `ctest.exe --test-dir build/navi33-research-msvc -C Debug --output-on-failure`.
  Exit 0: 3/3 passed (`navi33_research_tests`,
  `navi33_driver_core_tests`, `navi48_routing_isolation`).
- **Direct executable checks:** `build/navi33-research-msvc/Debug/navi33_research_tests.exe`
  printed `Navi 33 research tests: 26 passed, 0 failed`, exit 0.
  `build/navi33-research-msvc/Debug/navi33_driver_core_tests.exe` printed
  `Navi 33 lifecycle tests: 28 passed, 0 failed`, exit 0.
- **Other checks:** `python -B test_ipdiscovery.py` from `tools/ipdisc/`
  exited 0 (`Ran 30 tests in 1.688s`, `OK`). `git diff --check` exited 0.
  The generated `build/navi33-research-msvc/` output is ignored by Git, was
  previously confirmed absent, and does not alter the tracked/untracked source
  state.
- **Scope and limits:** This proves the portable C++17 research/lifecycle
  library and synthetic host tests compile and execute with the recorded
  Windows toolchain. It does not build the macOS kext, test IOKit, validate a
  production platform backend, access the RX 7600, or prove GPU operation,
  display, acceleration, firmware needs, or Metal. Navi48 initialization,
  identity guards, and plist were not changed in this milestone.
- **Git:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; the two existing tracked
  modifications and 34 untracked paths at build time remain preserved.
  Nothing was staged or committed.

## 2026-10-21 — Phase 17 isolated Navi33 lifecycle core

- **Baseline:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`. Preserved two existing tracked
  modifications (`src/navi48-bringup/NOTICE` and
  `src/navi48-bringup/src/Navi48Bringup.cpp`) and all 31 existing untracked
  paths, including parser caches and earlier Navi33 source. Existing
  `src/navi33-research/CMakeLists.txt` is a standalone C++17 target; the only
  macOS IOKit integration inspected is the Navi48 kext Makefile/Info.plist.
  Its match IDs remain `1002:7550` and `1002:7551`. No separate Navi33 macOS
  service or production platform backend exists.
- **Implementation:** Added `src/navi33-research/include/Navi33DriverCore.hpp`
  and `src/navi33-research/src/Navi33DriverCore.cpp`. The core reuses
  `selectArchitecture()` and accepts only the `Navi33ResearchOnly` route.
  Lifecycle transitions are `Uninitialized -> Identified -> Prepared ->
  Running`, with `Failed` and `Stopped` terminal states. Invalid/repeated
  transitions fail deterministically; platform callback failures enter
  `Failed` and request cleanup. Explicit stop and destruction clean up once.
  Identity matching invokes no platform callback. `Navi33Platform` is an
  abstract lifecycle boundary only; no IOKit, PCI, MMIO, firmware, GPU memory,
  or command interface is implemented. `Running` means only that the supplied
  callback returned true and is not evidence of functional hardware.
- **Tests and build wiring:** Added
  `src/navi33-research/tests/test_Navi33DriverCore.cpp` using a safe host mock
  and registered a separate executable/test in
  `src/navi33-research/CMakeLists.txt`. Coverage includes valid RX 7600
  identity, wrong vendor, Navi48/wrong-device identity, no callback on
  identification/rejection, transition ordering, repeated initialization/
  prepare/start/stop, failed prepare/start, cleanup exactly once, destructor
  cleanup, and no restart after failure/stop. Extended
  `tests/test_navi48_isolation.py` to enforce that the new Navi33 core has no
  Navi48 initializer reference and remains in its own CMake source target.
  Updated the Navi33 README to document lifecycle semantics and the absence of
  a production backend.
- **Validation actually run:** From repository root,
  `python -B src/navi33-research/tests/test_navi48_isolation.py` exited 0:
  `Ran 4 tests in 0.006s`, `OK` (4 passed, 0 failed, 0 skipped). From
  `tools/ipdisc/`, `python -B test_ipdiscovery.py` exited 0:
  `Ran 30 tests in 1.595s`, `OK` (30 passed, 0 failed, 0 skipped).
  `git diff --check` exited 0. These are Python/static and parser checks.
- **C++/macOS status:** The prior environment check in this same session found
  no C++ compiler or CMake; no new toolchain lead exists. The C++ lifecycle
  executable and CTest were therefore **not run**, and its code is not claimed
  compiled. No macOS-specific build was run. No IOKit matching was added
  because the existing kext target is Navi48-specific; mixing IDs there would
  violate isolation. No physical GPU access occurred.
- **Next blocker:** Obtain user approval for an already described Visual Studio
  2022 Build Tools C++ workload/MSVC x64, Windows SDK and CMake installation,
  then configure/build/test this separate C++17 target. After host tests pass,
  the next platform gate is a separately reviewed Navi33 macOS integration
  design and authorized macOS SDK build; do not add a PCI personality or
  hardware backend before that.

## 2026-10-21 — Phase 16 provenance-bound parse report; C++ install decision

- **Baseline:** Re-read the current `RESUME.md` and recent `WORKLOG.md` entries;
  branch `main`, HEAD `d1f7bad8408a0a2c016b067c477422975a59de44`. Existing
  modifications are retained: `src/navi48-bringup/NOTICE` and
  `src/navi48-bringup/src/Navi48Bringup.cpp` are tracked modifications;
  31 paths under `src/navi33-research/` and `tools/ipdisc/` remain untracked,
  including both Python cache artifacts. No `.github/workflows` directory was
  present. The existing Navi33 C++ module and its CMake config were reviewed;
  no duplicate C++ implementation was added.
- **Toolchain decision:** No additional build environment was found in the
  already-open workspace terminal; `clang++`, `clang-cl`, `g++`, `cl`, `cmake`,
  `ninja`, `make`, and `zig` are unavailable there. The CMake project declares
  CMake 3.16+, C++17, CXX-only language, and standard-library-only source;
  it selects MSVC warning flags when using MSVC and does not itself require
  Apple/macOS SDKs. No repository CI workflow supplies another build route.
  Smallest practical Windows route chosen for review: Visual Studio 2022 Build
  Tools with the C++ build-tools workload (MSVC x64 and a Windows SDK), plus
  standalone CMake 3.16+. This is a host-side unit-test build, not a macOS
  kext build. Nothing was installed.
- **Commands for user review only (NOT EXECUTED):**

  ```powershell
  winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --interactive
  winget install --id Kitware.CMake --exact --interactive
  ```

  In the Build Tools installer select **Desktop development with C++**
  (MSVC x64/x86 build tools and a Windows 10/11 SDK). After explicit approval
  and installation, the proposed isolated build/test commands are:

  ```powershell
  cmake -S src/navi33-research -B build/navi33-research-msvc-phase16 -G "Visual Studio 17 2022" -A x64
  cmake --build build/navi33-research-msvc-phase16 --config Debug
  ctest --test-dir build/navi33-research-msvc-phase16 -C Debug --output-on-failure
  ```

  The dedicated build directory did not exist during this check. User approval
  is required before either installation command is run; no installer or
  download was invoked.
- **Implementation:** Added optional `dump.py --provenance SIDECAR.json` for
  JSON parsing reports. It requires the documented evidence fields and an
  artifact class, rejects malformed/duplicate-key/unknown-status/missing-field
  sidecars, applies the existing bounded regular-file and mutation-detection
  read path, and verifies a supplied SHA-256 against the exact parsed bytes.
  Reports include computed size/hash, parser mode, `STRUCTURE_ONLY` scope,
  caller-supplied metadata, and `metadata_authenticated: false`. `NOT_COMPUTED`
  is allowed but does not disable computed-hash output. It does not authenticate
  provenance or establish physical board attribution. README usage and limits
  were updated.
- **Validation:** From `tools/ipdisc/`,
  `python -B test_ipdiscovery.py` exited 0: `Ran 30 tests in 1.627s`,
  `OK` (30 passed, 0 failed, 0 skipped). Added CLI coverage for deterministic
  output, correct hash binding, missing fields, unknown status, hash mismatch,
  duplicate keys, malformed JSON, and oversized sidecar rejection. From repo
  root, `python -B src/navi33-research/tests/test_navi48_isolation.py` exited 0:
  `Ran 3 tests in 0.007s`, `OK` (3 passed, 0 failed, 0 skipped).
  `git diff --check` exited 0; trailing-whitespace checks passed. These tests
  are Python/parser and static routing checks, not C++ build/test evidence.
- **Unverified / blockers:** No C++ compile, link, C++ unit execution, CTest,
  macOS integration, or GPU validation. RX 7600 board IP/firmware evidence and
  macOS graphics integration remain absent. The next technical step is gated
  on explicit user approval to install the reviewed C++ toolchain; then run
  the listed CMake/CTest commands and resolve actual build failures.
- **Safety/Git:** No GPU, PCI configuration, MMIO, VRAM, firmware, driver,
  EFI, or system configuration was accessed or modified. No install, download,
  stage, commit, reset, clean, stash, or deletion occurred. Navi48 source was
  inspected but not changed in this milestone; all existing tracked changes,
  untracked sources, and caches remain preserved.

## 2026-10-20 — Phase 15 C++ prototype execution blocked by toolchain

- **Objective:** Establish the first compiled and executed Navi33 research
  component without hardware initialization.
- **Current source reviewed:** `src/navi33-research/` already contains a
  hardware-independent C++17 component: `include/Navi33Research.hpp` defines
  explicit identity, architecture route, IP evidence, firmware evidence, and
  validation result types; `src/Navi33Research.cpp` implements deterministic
  validation and fail-closed architecture selection; and
  `tests/test_Navi33Research.cpp` contains synthetic unit assertions. The
  CMake configuration builds a static library and test executable with
  warnings-as-errors. This is source review only; none of those C++ assertions
  has been compiled or executed in this environment.
- **Toolchain discovery:** From PowerShell, `Get-Command` for `clang++`,
  `clang-cl`, `g++`, `cl`, `cmake`, `ninja`, `make`, and `mingw32-make`
  returned `not-found`. Checked common existing locations under Visual Studio,
  BuildTools, Windows Kits, LLVM, MSYS2, and MinGW; none existed. `vswhere.exe`
  was absent, no Visual Studio SxS registry instance was returned, and no
  `VSINSTALLDIR`, `VCToolsInstallDir`, `WindowsSdkDir`, `INCLUDE`, or `LIB`
  build environment variables were set. No compiler, CMake, or Windows SDK
  C++ toolchain is available through these checks. No tool was installed.
- **Commands actually run:** From repository root,
  `python -B src/navi33-research/tests/test_navi48_isolation.py` exited 0:
  `Ran 3 tests in 0.007s`, `OK` (3 passed, 0 failed, 0 skipped). From
  `tools/ipdisc/`, `python -B test_ipdiscovery.py` exited 0:
  `Ran 28 tests in 0.729s`, `OK` (28 passed, 0 failed, 0 skipped).
  `git diff --check` exited 0. These Python results do not validate or
  substitute for compiling/executing `test_Navi33Research.cpp`.
- **Implementation status:** No C++ source or build configuration was changed:
  without the compiler/build system, a code edit could not meet the requested
  executable verification. No new source-inspection test was added as a
  substitute. The existing Navi48 start identity guard was inspected in the
  tracked diff and is unchanged by this milestone.
- **Unverified / blocker:** C++ compile, C++ unit-test execution, CMake/CTest,
  macOS kext integration, and physical GPU behavior remain unverified. Next
  highest-value task is to run the existing CMake configure/build/CTest
  sequence with an already authorized C++17 compiler/CMake environment; then
  fix actual compiler or test failures. Until that gate is met, treat the
  Navi33 component strictly as reviewed source, not an executable prototype.
- **Safety and Git:** No GPU, PCI configuration, MMIO, VRAM, firmware,
  active driver, EFI, or system settings were accessed or modified. No
  install, download, stage, commit, reset, clean, stash, or delete occurred.
  Branch remains `main`, HEAD remains
  `d1f7bad8408a0a2c016b067c477422975a59de44`. The pre-existing
  `src/navi48-bringup/NOTICE` edit and all 31 untracked files/artifacts,
  including caches, were preserved.

## Historical — M1 implementation and verification

- **Date/task:** 2026-10-20, M1 offline IP Discovery parser; exact time not
  retained in the prior handoff.
- **Objective:** Implement and verify the offline IP Discovery parser, fixtures,
  CLI, tests, and behavior documentation before M2.
- **Changes recorded by prior handoff:** Seven untracked M1 source/documentation
  files were created under `tools/ipdisc/`: `README.md`, `cpp_parity.md`,
  `dump.py`, `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`.
- **Commands/results:** The prior handoff reports `python -B
  test_ipdiscovery.py` passed 19 tests. Earlier initial tests failed due to a
  malformed synthetic die offset; the handoff says the fixture/parser issues
  were subsequently repaired. Exact command output and timestamps for those
  earlier runs are not available in this session.
- **Issues/limitations recorded:** A later read-only verification found defects
  in v1 fixture overlap, strict structural overlap validation, and the
  compatibility truncation test. The earlier Linux reference was obtained via
  a moving `master` lookup and was not pinned.
- **Git state reported at the time:** `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; seven source files and two
  `__pycache__` files were untracked, with no tracked or staged changes. This
  is historical until checked again below.
- **Remaining at that stage:** M1.1 repairs were required; M2 was not authorized.

## Historical — M1.1 structural repair

- **Date/task:** 2026-10-20, M1.1 structural validation repair; exact completion
  time retained only as a previous report, not independently confirmed.
- **Objective:** Repair fixture layout and strict structural validation,
  strengthen regression tests, preserve compatibility behavior, and pin the
  Linux format reference.
- **Changes recorded by prior handoff:** Updated `ipdiscovery.py`,
  `mkfixture.py`, and `test_ipdiscovery.py`; updated `README.md`,
  `cpp_parity.md`, and `parity_cases.json`. The v1 fixture uses the complete
  six-entry list, strict mode rejects table/header-list and die/fixed-header
  overlap, and the compatibility truncation fixture crosses the supplied
  buffer boundary. The documented Linux `discovery.h` reference is commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`; the documents note this does not
  prove which revision the earlier moving-`master` lookup used.
- **Commands/results:** The prior handoff reports the final
  `python -B test_ipdiscovery.py` run passed 22 tests (`Ran 22 tests in 0.327s`,
  `OK`) and a separate check found 22 deterministic generated fixtures. This
  entry preserves that prior result; the latest independently observed run is
  recorded below. The handoff also describes intermediate failures caused by
  stale fixture offsets and says those were corrected before the final run.
- **Limitations:** No compiled-C++ differential execution or hardware test was
  performed. Fixtures are synthetic. No GPU compatibility conclusion was
  established.
- **Git state reported at the time:** `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; all seven M1 files and two
  pre-existing cache files were untracked, with no staged or tracked changes.
  This session rechecked the same state before the documentation changes; see
  the current entry.
- **Remaining:** Documentation organization/recovery records were not yet
  established. M2 remained unauthorized.

## 2026-10-20 — DOC-RECOVERY documentation organization

- **Objective:** Organize the technical guide and establish persistent
  append-only work history and a current recovery checkpoint, without changing
  parser implementation.
- **Changes completed:** Updated `README.md`; created this `WORKLOG.md` and
  `RESUME.md`. The README now summarizes suite coverage and the latest test
  result. Historical M1/M1.1 details above are explicitly identified as claims
  from the prior handoff when their original outputs are unavailable.
- **Files changed/created:** `tools/ipdisc/README.md` (modified),
  `tools/ipdisc/WORKLOG.md` (created), and `tools/ipdisc/RESUME.md` (created).
- **Commands actually executed:**
  - In the repository: `git status --porcelain=v2 --branch --untracked-files=all`,
    `git status --short --branch`, `git diff --cached --stat`,
    `git diff --stat`, `git branch --show-current`, and `git rev-parse HEAD`.
  - In `tools/ipdisc/`: `python -B test_ipdiscovery.py`.
- **Observed test result:** Exit code 0; all 22 tests reported `ok`;
  `Ran 22 tests in 0.289s`; `OK`. No skipped tests were reported. The
  deterministic-fixture test passed as part of this run; no separate fixture
  generation/count command was run in this task.
- **Issues/failures/retries:** No failures occurred in this task's test run.
  The historical M1.1 intermediate fixture failures and their reported
  correction are retained in the preceding entry.
- **Git state observed before documentation edits:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, tracking `origin/main` with
  `+0 -0`; no staged or tracked changes. Seven M1 files and two Python cache
  artifacts were untracked. Final post-edit Git state is to be checked and
  captured in `RESUME.md` before ending this task.
- **Remaining/decisions:** No parser or implementation changes are part of
  this task. No C++ differential or hardware tests have been performed. M2
  remains unauthorized; no further milestone should begin without approval.

### DOC-RECOVERY post-edit Git audit

- **Date/task:** 2026-10-20, DOC-RECOVERY final scope check.
- **Commands/results:** Rechecked `git status --porcelain=v2
  --branch --untracked-files=all`, `git diff --stat`,
  `git diff --cached --stat`, `git branch --show-current`, `git rev-parse HEAD`,
  and `git rev-parse refs/remotes/origin/main`. Branch is `main`; HEAD and
  `origin/main` both resolve to
  `d1f7bad8408a0a2c016b067c477422975a59de44`. The merge base is the same
  commit, and `git status` reports `## main...origin/main`.
- **Corrections to the earlier audit note:** A first PowerShell probe passing
  the literal `@{upstream}` was parsed incorrectly and failed. A subsequent
  two-tip `git rev-list --left-right --count HEAD origin/main` displayed
  `0 17` because it counts commits reachable from each tip, including shared
  history; it is not an ahead/behind calculation. Comparing exact ref OIDs and
  their merge base confirms the refs are identical. No fetch or repository
  mutation was performed.
- **Final worktree state:** No staged or tracked changes. The following 11
  paths are untracked, all under the authorized `tools/ipdisc/` directory:
  `README.md`, `RESUME.md`, `WORKLOG.md`, `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`. The two cache artifacts were preserved.
- **Scope:** The only task edits were to `README.md`, `WORKLOG.md`, and
  `RESUME.md`. The remaining untracked source/docs and cache files were already
  present before this task. The final scope audit found no status paths outside
  `tools/ipdisc/`.

## 2026-10-20 — M2.0 RX 7600 architecture research

- **Objective:** Recover the project checkpoint, inspect Cobalt64's architecture
  and pinned upstream evidence, and complete a safe source-only RX 7600
  feasibility milestone without touching driver code or hardware.
- **Changes completed:** Created `RX7600_RESEARCH.md` with repository and
  upstream evidence, Navi 33/Navi 48 boundary analysis, an RX 7600 feature
  matrix, licensing/safety notes, blockers, and the gate for any later
  driver-source proposal. Updated `README.md` to link the report, clarify that
  IP Discovery tests do not establish target support, and record the latest
  test observation. This milestone changed documentation only.
- **Files changed/created:** Modified `tools/ipdisc/README.md`; created
  `tools/ipdisc/RX7600_RESEARCH.md`; appended this entry to
  `tools/ipdisc/WORKLOG.md`; updated `tools/ipdisc/RESUME.md`. Existing
  parser/test files and cache artifacts were not modified or removed.
- **Commands actually executed:**
  - From `tools/ipdisc/`: `python -B test_ipdiscovery.py` (run after the
    research report and README changes).
  - From repository root: `git branch --show-current`,
    `git rev-parse HEAD`, `git status --porcelain=v2 --branch
    --untracked-files=all`, `git diff --cached --stat`, and
    `git diff --stat`.
  - Read-only toolchain availability checks and source fetches were performed
    for the Windows build prerequisites and the pinned Linux and PCI ID
    references listed in `RX7600_RESEARCH.md`. The AMD product-page request
    timed out; guessed `navi32.c` and `navi32.h` raw paths returned 404, so
    they were not used as evidence.
- **Observed test result:** `python -B test_ipdiscovery.py` exited 0 and
  reported all 22 named tests `ok`, followed by `Ran 22 tests in 0.304s` and
  `OK`. No skips were reported. Tests use offline synthetic input; no physical
  GPU or hardware interface was accessed.
- **Research/build limitations:** The RX 7600 feature matrix has no target
  feature marked implemented and tested. The generic parser is only partially
  implemented for target integration. Windows tooling lacks `clang`,
  `clang++`, `make`, `xcrun`, `codesign`, MacKernelSDK, the required firmware
  directory, and macOS target environment; no kext build or hardware test was
  attempted. There is no compiled-C++ differential test or independently
  verified RX 7600 discovery dump. No code or firmware was copied.
- **Git state observed at 2026-10-20T19:16:05.948+07:00:** Branch `main`,
  HEAD `d1f7bad8408a0a2c016b067c477422975a59de44`, upstream
  `origin/main`, reported ahead/behind `+0 -0`; staged and tracked diffs were
  empty. Twelve paths were untracked, all under `tools/ipdisc/`, including
  `RX7600_RESEARCH.md`, the existing M1 files, the two preserved `__pycache__`
  files, `WORKLOG.md`, and `RESUME.md`.
- **Remaining work/decision:** No M2.1 source mapping or driver implementation
  is included in this milestone. The report recommends a pinned source-only
  Navi 33 per-IP/version and firmware mapping as the next safe research task.
  Any driver edit requires a separate milestone proposal and recovery plan;
  never route `1002:7480` through Navi 48 initialization.

### M2.0 post-documentation Git audit

- **Checked:** 2026-10-20T19:16:53.322+07:00.
- **Result:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, upstream `origin/main`, ahead/
  behind `+0 -0`. Staged and tracked diff stats were empty. All 12 reported
  untracked paths were under `tools/ipdisc/`: `README.md`, `RESUME.md`,
  `RX7600_RESEARCH.md`, `WORKLOG.md`,
  `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`. No path outside the authorized directory was reported.

## 2026-10-20 — M2.1 Navi 33 IP and firmware mapping (in progress)

- **Objective:** Identify Navi 33 / gfx1102 IP-version and Linux firmware
  dependencies from exact pinned upstream source, compare them to Cobalt64's
  Navi 48 implementation, and document the evidence without hardware access.
- **Completed research sections:** Re-read `RESUME.md`, `WORKLOG.md`,
  `README.md`, and `RX7600_RESEARCH.md`; verified current Git branch, HEAD, and
  worktree. Confirmed the existing source report's pinned Linux revision:
  `torvalds/linux` commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`. Fetched pinned raw source for
  `amdgpu_drv.c`, `amdgpu_discovery.c`, `gfx_v11_0.c`, `gmc_v11_0.c`,
  `sdma_v6_0.c`, `psp_v13_0.c`, `vcn_v4_0_3.c`, and `include/discovery.h`;
  inspected the matching local Navi 48 Makefile, initialization header,
  personality, and core source paths read-only.
- **Preliminary verified evidence:** Linux `discovery.h` contains hardware IP
  versions in per-IP discovery records rather than a fixed Navi 33 inventory.
  `gfx_v11_0.c` declares firmware families for GC 11.0.2 among several GC
  revisions, and `sdma_v6_0.c` declares SDMA 6.x names. These module
  declarations alone do not establish which requests the Navi 33 initialization
  path makes. Exact IP versions and firmware request sites are still under
  investigation; no per-card version is asserted yet.
- **Issues during research:** `rg` is not installed in this Windows environment,
  so a source grep attempt failed; native PowerShell line matching is being
  used instead. Guessed pinned URLs `amdgpu/smu_v13_0.c` and
  `display/dc/dcn32/dcn32_resource.c` returned 404; neither is used as
  evidence. A first PowerShell source scan applied `Select-String` to the whole
  fetched string rather than line arrays and did not surface the intended
  matches; that scan is being corrected. No repository source was edited.
- **Commands performed so far:** Read-only Git commands
  (`git branch --show-current`, `git rev-parse HEAD`,
  `git status --porcelain=v2 --branch --untracked-files=all`,
  `git diff --cached --stat`, `git diff --stat`); PowerShell source listing
  and pattern search; HTTP GETs of the pinned Linux source files above.
- **Git state at start:** `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, upstream `origin/main`,
  ahead/behind `+0 -0`; 12 existing untracked paths, all under `tools/ipdisc/`,
  and no staged/tracked diff.
- **Current state:** Incomplete. Evidence map has not yet been created. No
  parser tests have been run during M2.1 yet. No hardware, firmware, or driver
  implementation was accessed or changed.
- **Safest next action:** Re-run targeted extraction against the pinned raw
  files to establish actual Navi 33 ASIC/IP mapping and firmware request
  branches, then append findings before drafting the evidence map.

### M2.1 progress — discovery and Linux dispatch model

- **Checkpoint:** 2026-10-20T19:27:18.188+07:00.
- **Verified source facts:** GitHub's commit API resolved the pinned Linux SHA
  to the same full hash and subject previously recorded. In pinned
  `amdgpu_discovery.c`, `amdgpu_discovery_reg_base_init()` reads IP Discovery
  records and assigns each `ip_versions[hw_ip][instance]` from the record's
  major/minor/revision/variant/subrevision fields (lines 1480-1495,
  1614-1645). Thus Linux dispatch code establishes supported IP-version
  branches, but does not by itself establish a physical RX 7600's reported
  values.
- **Verified Linux dispatch candidates (not asserted as RX 7600 hardware
  values):** The pinned source has branches that add GFX 11 and GMC 11 code for
  GC `11.0.2` (`amdgpu_discovery.c:2086-2095,2146-2156,2467-2477`); IH v6 for
  OSSSYS `6.0.2` (`2196-2200`); PSP v13.0.4 for MP0 `13.0.4`
  (`2220-2267`); SMU v13 for MP1 `13.0.4` (`2291-2333`); VCN/JPEG 4.0.3 for
  UVD `4.0.3` (`2634-2642`); and SDMA v6 for SDMA `6.0.2`
  (`2496-2537`). These are accepted Linux routes and still require the card's
  actual IP Discovery data to link them to this target.
- **Device handling evidence:** Pinned `amdgpu_drv.c:2178-2191` has generic
  AMD display/accelerator class entries using `CHIP_IP_DISCOVERY`; the pinned
  file has no explicit `0x7480`/`CHIP_NAVI33` entry. The separate pinned PCI
  ID database maps `1002:7480` to a shared Navi 33 product group. The
  user-specified gfx1102 value is not independently established by this
  Linux PCI table.
- **Display progress:** Pinned `amdgpu_dm.c` declares both
  `dcn_3_2_0_dmcub.bin` and `dcn_3_2_1_dmcub.bin` as display firmware; DCE
  3.2.0/3.2.1 are recognized in `amdgpu_discovery.c:2375-2397`. The exact
  display version for the physical card remains unresolved pending card IPD
  evidence and display-family selection.
- **Tooling note:** GitHub semantic search returned index commit
  `f47d223627f80cb7a1eb14c585fe6cb35de5e984`, not the pinned research commit.
  Those search snippets are discovery aids only and are not used as pinned
  evidence. Exact references in the map must point to raw source at
  `d0c647a6aae2ce577372594a176c82733e1ed267`.
- **Remaining:** Firmware request/conditional analysis is in progress;
  `RX7600_IP_FIRMWARE_MAP.md` has not yet been created. No tests or repository
  implementation changes have occurred during this research stage.

### M2.1 progress — source-defined firmware selection

- **Checkpoint:** 2026-10-20T19:29:14.447+07:00.
- **Verified pinned request paths:** The generic IP-version decoder builds
  prefixes from hardware-reported major/minor/revision when no legacy alias
  applies (`amdgpu_ucode.c:1445-1489`). For the Linux-dispatch candidates only,
  the source then requests:
  - GC/GFX: `gc_<major>_<minor>_<revision>_{pfp,me,mec,rlc}.bin`, with PFP,
    ME, MEC required, and RLC required outside SR-IOV (`gfx_v11_0.c:730-788`).
  - SDMA: `sdma_<major>_<minor>_<revision>.bin`, required by
    `amdgpu_sdma_init_microcode()` (`amdgpu_sdma.c:203-224`, called by the
    v6.0 IP init at `sdma_v6_0.c:1311-1313`).
  - PSP MP0 13.0.4: required TOC and TA (`psp_v13_0_4.c:32-50`,
    `amdgpu_psp.c:3516-3524,3967-3980`); the specialized 13.0.4 init does not
    request an SOS in that function.
  - SMU MP1 13.0.4: `smu_13_0_4.bin` or kicker variant by version-derived
    prefix; required in non-SR-IOV path (`smu_v13_0.c:93-114`).
  - VCN UVD 4.0.3: `vcn_4_0_3.bin`, required by the version-derived VCN
    request (`amdgpu_vcn.c:59,85-94,99-124`).
  - MES, when that scheduler path is selected: GC-version-derived MES image
    required; GC 11.x uses scheduler/pipe suffix rules and retries
    `_mes_2.bin` as `_mes.bin` where specified
    (`amdgpu_mes.c:667-700`; names advertised in `mes_v11_0.c:36-60`).
  - DMUB, when DCE is a supported version: DCE 3.2.0 and 3.2.1 select
    `dcn_3_2_0_dmcub.bin` and `dcn_3_2_1_dmcub.bin`, respectively; firmware is
    required for a selected DMUB-capable path (`display/amdgpu_dm/amdgpu_dm.c:
    132-135,5769-5835`).
- **Important scope distinction:** These source paths define Linux's behavior
  if a matching IP Discovery version is present. They do not prove the
  physical RX 7600 reports those versions or that Linux firmware is usable by
  a macOS driver. No firmware file was retrieved, downloaded, or executed.
- **Remaining:** Need capture exact discovery file fallback and initialization
  source references, inspect Cobalt64 baseline evidence for the comparison
  section, create the evidence map, synchronize docs, run the parser suite,
  validate links and final Git scope.

### M2.1 completion — pinned Navi 33 IP and firmware map

- **Date/task:** 2026-10-20 — M2.1, source-only Navi 33 IP-version and firmware
  request mapping.
- **Objective:** Record pinned Linux AMDGPU evidence, distinguish generic
  supported IP-version branches from values reported by a specific RX 7600,
  compare them to the existing Navi 48 code without modifying it, and preserve
  the milestone state.
- **Completed:** Created `RX7600_IP_FIRMWARE_MAP.md` with pinned source
  revisions, IP Discovery structures and version decoding, Linux dispatch
  branches, version-conditional firmware request names/flags, initialization
  data flow, comparison to Cobalt64's Navi 48 files, evidence labels,
  unresolved target questions, and future implementation gates. Updated
  `RX7600_RESEARCH.md` with the M2.1 summary and safety boundary; updated
  `README.md` with the evidence-map link and latest test result.
- **Files created/updated:** Created `tools/ipdisc/RX7600_IP_FIRMWARE_MAP.md`;
  updated `tools/ipdisc/README.md`, `tools/ipdisc/RX7600_RESEARCH.md`,
  `tools/ipdisc/WORKLOG.md`, and `tools/ipdisc/RESUME.md`. Other existing
  untracked source, documentation, and cache artifacts were preserved.
- **Pinned sources:** `torvalds/linux` commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`; `pciutils/pciids` commit
  `22da7386ba818c037894f258248734770757b14f`. The Linux pin records the
  reference inspected for this research; it does not establish which commit
  the earlier moving-master lookup used.
- **Verified result and limitation:** The pinned Linux source reads IP
  versions from discovery records and selects IP implementations and firmware
  from reported versions. It contains relevant branches for GC 11.x, SDMA
  6.0.x, OSSSYS/IH 6.0.x, PSP/MP0 13.0.4, SMU/MP1 13.0.4, UVD/VCN 4.0.3,
  DCE 3.2.0/3.2.1, and MES v11. These are **not verified as the physical
  RX 7600's versions**. The source-derived firmware requests are conditional
  Linux behavior, not a verified RX 7600 firmware inventory or evidence of
  macOS suitability. gfx1102 remains user-supplied rather than independently
  confirmed. No firmware data was accessed.
- **Commands and actual outcomes:**
  - From `tools/ipdisc/`, ran `python -B test_ipdiscovery.py` twice. First run:
    `Ran 22 tests in 0.273s` / `OK`. Final run: `Ran 22 tests in 0.314s` /
    `OK`. Final run listed 22 test cases, all `ok`; 22 passed, 0 failed,
    0 skipped.
  - Checked local Markdown links in `README.md`, `RX7600_RESEARCH.md`,
    `RX7600_IP_FIRMWARE_MAP.md`, `WORKLOG.md`, and `RESUME.md`: 4 local links
    checked; all resolved. External source links use pinned commit URLs.
  - Inspected the local Navi 48 Makefile, bring-up stages, PCI match,
    probe/start path, and DCN 4.1 header read-only.
- **Patch retry:** An initial combined documentation patch applied the README
  hunk but its research-report context did not match. The resulting files were
  inspected, and the remaining report changes were applied against current
  context. Final documents were reviewed.
- **Git state observed:** `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, upstream `origin/main`,
  ahead/behind `+0 -0`; no staged or tracked diff. Thirteen untracked paths,
  all under `tools/ipdisc/`, including the new map and two pre-existing
  `__pycache__` files. No paths outside the approved directory were reported.
- **Remaining:** Exact per-board IP Discovery values, complete target firmware
  set, board/VBIOS details, firmware provenance/licensing, macOS integration,
  and hardware behavior remain unresolved. No driver, build, or hardware test
  was performed or claimed. M2.1 is complete as a research milestone only;
  RX 7600 macOS support is not established.
- **Single safest next action:** If an existing trusted RX 7600 IP Discovery
  artifact with board provenance is available, analyze it offline with the
  strict parser; otherwise keep target-specific versions unresolved. Do not
  probe hardware or modify the Navi 48 path.

## 2026-10-20 — M2.2 offline evidence audit and implementation gate

- **Objective:** Search only the existing Cobalt64 repository and available
  task artifacts for attributable RX 7600 IP Discovery evidence; document the
  evidence gap and preconditions for considering a future implementation.
- **Recovery and pre-edit Git check:** Read `RESUME.md` first, followed by
  `WORKLOG.md`, `RX7600_RESEARCH.md`, and `RX7600_IP_FIRMWARE_MAP.md`.
  Rechecked branch/status before edits: branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; no staged or tracked diffs.
  The then-existing 13 untracked paths were all under `tools/ipdisc/`.
- **Audit completed:** Inspected parser, tests, fixture generator and
  documentation; inventoried repository-local binary/dump/log and
  RX7600/IP-Discovery-named candidates without scanning unrelated user
  directories or accessing hardware. No separate user artifact was supplied.
  No IP Discovery dump, target hardware report/log, subsystem ID, or
  board-attributed IP Discovery record was found.
- **Candidate classification:** The only on-disk `.bin` files found were
  `src/navi48-bringup/shaders/store_magic.bin` (56 bytes) and
  `src/navi48-bringup/shaders/store_magic_hsa.bin` (512 bytes). Their shader
  paths/documentation indicate they are not IP Discovery captures; they were
  not passed to the parser. Existing generated fixtures are in-memory only:
  22 fixtures, each 512 bytes, generated by `tools/ipdisc/mkfixture.py`, whose
  docstring explicitly says none represents real hardware. Strict parser
  checks accepted the four valid synthetic structures and rejected the 18
  deliberately malformed/unsupported structures. Existing `__pycache__`
  artifacts were observed and preserved.
- **Report and documentation:** Created
  `tools/ipdisc/RX7600_EVIDENCE_GAPS.md` with artifact inventory, provenance
  assessment, verified pinned-source facts, unresolved board fields, limits
  of parser success, firmware provenance/licensing gaps, safe future evidence
  acquisition, and acceptance criteria before driver-source changes.
  Updated `README.md` and `RX7600_RESEARCH.md` with links and M2.2 results.
  `RX7600_IP_FIRMWARE_MAP.md` was not changed because the audit found no new
  target-specific evidence.
- **Test command and exact final result:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`. Exit code 0; the output listed all 22 test
  cases as `ok`, then:
  `Ran 22 tests in 0.331s`
  `OK`
  Count: 22 passed, 0 failed, 0 skipped. Tests and fixture audit were offline
  and synthetic; they do not establish physical GPU behavior.
- **Command issue/retry:** An initial one-line `python -c` fixture inventory
  probe failed with a quoting-induced `SyntaxError`; no files were changed by
  that failed probe. It was rerun successfully using a PowerShell here-string
  piped into `python -B`; all 22 generated fixtures were listed and parsed.
- **Link check:** Checked local Markdown links in six M2.2 documents;
  all six resolved. External source URLs remain commit-pinned as documented.
- **Safety:** No parser or driver implementation was modified. No physical
  GPU, PCI configuration space, MMIO, VRAM, or firmware interface was
  accessed; no firmware was downloaded or executed. No files outside
  `tools/ipdisc/` were edited. No stage, commit, clean, reset, stash, or delete
  operation was performed.
- **Final Git state:** `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, upstream `origin/main`,
  ahead/behind `+0 -0`; no staged or tracked diff. Fourteen untracked paths,
  all under `tools/ipdisc/`: `README.md`, `RESUME.md`,
  `RX7600_EVIDENCE_GAPS.md`, `RX7600_IP_FIRMWARE_MAP.md`,
  `RX7600_RESEARCH.md`, `WORKLOG.md`,
  `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`.
- **Outcome and unresolved issues:** No trusted board-specific evidence was
  located. Exact per-IP versions, board/subsystem/VBIOS attribution, firmware
  binaries and their provenance/licensing, and macOS integration remain
  unresolved. M2.2 is complete as an offline evidence audit only; RX 7600
  support is not claimed.
- **Single safest next action:** If an existing provenance-documented RX 7600
  IP Discovery artifact is available, submit it for offline strict parsing
  after confirming board attribution; otherwise retain the evidence gap.
  Hardware collection or driver changes need separate explicit authorization.

## 2026-10-20 — M2.3 research readiness and reproducibility audit

- **Objective:** Reproduce the offline parser suite, audit claims and links in
  the current research documents, verify current Git state, and document
  readiness without changing parser or driver behavior.
- **Recovery:** Read `RESUME.md` first, then `WORKLOG.md`, `README.md`,
  `RX7600_RESEARCH.md`, `RX7600_IP_FIRMWARE_MAP.md`, and
  `RX7600_EVIDENCE_GAPS.md`. Rechecked repository state rather than relying
  solely on the M2.2 checkpoint.
- **Files/commands inspected:** `ipdiscovery.py`, `mkfixture.py`,
  `test_ipdiscovery.py`, `cpp_parity.md`, all six M2.3 documentation files,
  Git status/refs, and project-local artifact inventory. The documented test
  invocation was run from `tools/ipdisc/`; no unrelated user directories or
  device interfaces were accessed.
- **Reproducibility result:** `python -B test_ipdiscovery.py` exited 0 and
  printed 22 test methods, each `ok`, followed by the exact summary:
  `Ran 22 tests in 0.283s`
  `OK`
  Counts: 22 passed, 0 failed, 0 skipped. The tests use synthetic fixtures and
  temporary CLI files; no physical GPU, firmware interface, or RX 7600 test
  evidence is involved. Strict mode remains distinct from C++ compatibility
  mode, and documented unsupported versions/layouts remain explicit.
- **Documentation/link audit:** Reviewed the M2.0/M2.1/M2.2 evidence boundary.
  No repository evidence establishes exact physical-card IP versions or
  successful RX 7600 macOS build/initialization/hardware behavior. Seven local
  Markdown links across README, research report, IP/firmware map, evidence-gap
  report, this audit, worklog and checkpoint were checked; all resolved.
  Created `RX7600_REPRODUCIBILITY_AUDIT.md` and linked it from `README.md`.
  No parser, driver, firmware, or system implementation was changed.
- **Git discrepancy check:** At audit start, one `git rev-list` invocation
  without a `...` range printed `0 17`; that form was not treated as an
  upstream divergence count. Exact HEAD and `origin/main` OIDs, merge base,
  porcelain branch line and the explicit range
  `git rev-list --left-right --count HEAD...refs/remotes/origin/main` were
  verified: both refs and merge base are
  `d1f7bad8408a0a2c016b067c477422975a59de44`, status is `+0 -0`, divergence
  is `0 0`. This confirms rather than contradicts the M2.2 report.
- **Git state after documentation edits:** At 2026-10-20T19:45:14.392+07:00,
  branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, upstream `origin/main` at the
  same commit; no staged or tracked diff. Fifteen untracked paths, all under
  `tools/ipdisc/`: `README.md`, `RESUME.md`, `RX7600_EVIDENCE_GAPS.md`,
  `RX7600_IP_FIRMWARE_MAP.md`, `RX7600_REPRODUCIBILITY_AUDIT.md`,
  `RX7600_RESEARCH.md`, `WORKLOG.md`,
  `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`. This matches the prior 14 project-local untracked
  paths plus the new audit report. No paths outside the authorized directory
  were reported.
- **Safety and limits:** Offline repository inspection only. No GPU, PCI,
  MMIO, VRAM or firmware access; no external binary download/execution; no
  changes to driver sources, personality, plist, EFI, boot arguments or system
  configuration. No generated or temporary files were added by this audit.
- **Outcome:** The documented parser test command is reproducible. This does
  not prove authentic hardware data, RX 7600 compatibility, successful macOS
  build/initialization, acceleration, or Metal support. Exact board IP
  versions, board attribution, firmware provenance/licensing, and target
  behavior remain unresolved.
- **Single safest next action:** If a trusted, board-attributed RX 7600
  discovery artifact already exists, submit it for offline strict parsing;
  otherwise preserve the unresolved mapping. Do not probe the physical GPU.

### M2.3 final verification update

- **Date/time:** 2026-10-20; final result observed before 2026-10-20T19:46:19
  local time.
- **Reason:** After synchronizing the README and audit report with the latest
  observed test output, reran the required suite.
- **Final command/result:** From `tools/ipdisc/`, `python -B
  test_ipdiscovery.py` exited 0. All 22 listed test methods reported `ok`;
  final output:
  `Ran 22 tests in 0.331s`
  `OK`
  22 passed, 0 failed, 0 skipped.
- **Final documentation check:** Rechecked 7 local Markdown links across the
  README, research report, IP/firmware map, evidence-gap report, reproducibility
  audit, worklog, and recovery checkpoint; all resolved.
- **Scope:** Documentation-only result synchronization. No parser behavior,
  driver, hardware, or firmware changes.

## 2026-10-20 — M2.4 upstream source and licensing audit

- **Objective:** Audit existing local evidence for upstream provenance,
  component license declarations, and firmware redistribution terms; document
  verification limits without implementation or legal conclusions.
- **Recovery and current state:** Read `RESUME.md` first, then
  `WORKLOG.md`, `README.md`, `RX7600_RESEARCH.md`,
  `RX7600_IP_FIRMWARE_MAP.md`, `RX7600_EVIDENCE_GAPS.md`, and
  `RX7600_REPRODUCIBILITY_AUDIT.md`. Rechecked Git before editing:
  branch `main`, HEAD and `origin/main`
  `d1f7bad8408a0a2c016b067c477422975a59de44`, merge base identical,
  divergence `0 0`; no staged or tracked diffs. Fifteen pre-existing
  untracked paths, all under `tools/ipdisc/`, including two preserved
  `__pycache__` files.
- **Sources inspected:** Root `LICENSE`, `BUILDING.md`,
  `src/navi48-bringup/LICENSE`, `src/navi48-bringup/NOTICE`,
  `third-party/THIRD-PARTY-LICENSES.txt`, selected Navi 48 source headers,
  the root README license/dependency notes, and the cited M2.1 Linux source
  paths. No Linux or other upstream checkout was locally available for
  independent commit/source verification; no fetch or download was performed.
- **Provenance findings:** M2.1 documents Linux commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`, PCI database commit
  `22da7386ba818c037894f258248734770757b14f`, and per-IP source paths. This
  audit verified the citations exist in project files only; upstream objects
  and cited lines were not independently rechecked. The report classifies all
  source-map rows accordingly. Family-level Linux paths do not establish
  physical RX 7600 IP versions or macOS behavior.
- **Licensing findings:** Local MIT, BSD-3-Clause, third-party notices, and
  project declarations were read. `BUILDING.md` calls Navi 48 Linux-firmware
  blobs separately licensed but supplies no blob-specific license text in the
  inspected material. Firmware terms, provenance, redistribution permissions,
  and macOS use remain **UNVERIFIED**. No legal advice or permission claim is
  made.
- **Files changed:** Created `RX7600_SOURCE_LICENSE_AUDIT.md`; linked it from
  `README.md`; updated this worklog and `RESUME.md`. All edits are within
  `tools/ipdisc/`. No parser, driver, firmware, hardware, or system files were
  modified.
- **Verification:** `python -B test_ipdiscovery.py` was run from
  `tools/ipdisc/`; the exact output and exit status are recorded in the final
  M2.4 entry below. Local Markdown links in the new audit and updated README
  were checked. Final Git status and changed-path scope are recorded below.
- **Limitations and outcome:** No source fetch, firmware access, hardware
  access, or implementation work. The exact IP versions of the user's
  physical RX 7600 and firmware licensing/redistribution terms remain
  unresolved. The audit does not establish RX 7600 macOS support.
- **Single safest next action:** Obtain human review of authoritative
  blob-specific terms and exact source provenance before any source reuse or
  firmware redistribution is proposed; keep implementation and hardware
  access separately gated.

### M2.4 final verification

- **Test command:** From `tools/ipdisc/`, ran `python -B
  test_ipdiscovery.py`; process exited 0. All 22 tests reported `ok`.
  Exact summary:
  `Ran 22 tests in 0.299s`
  `OK`
  Result: 22 passed, 0 failed, 0 skipped.
- **Local links:** Checked local Markdown targets in `README.md` and
  `RX7600_SOURCE_LICENSE_AUDIT.md`; all resolved. External links were not
  fetched.
- **Final Git state:** Branch `main`; HEAD/upstream/merge-base all
  `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`; no staged
  or tracked diff. Sixteen untracked paths, all under `tools/ipdisc/`:
  `README.md`, `RESUME.md`, `RX7600_EVIDENCE_GAPS.md`,
  `RX7600_IP_FIRMWARE_MAP.md`, `RX7600_REPRODUCIBILITY_AUDIT.md`,
  `RX7600_RESEARCH.md`, `RX7600_SOURCE_LICENSE_AUDIT.md`, `WORKLOG.md`,
  `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`. Existing cache files are preserved; no changed or
  untracked path outside the authorized directory was reported.
- **Completion boundary:** No GPU, PCI, MMIO, VRAM, firmware, or external
  source repository was accessed. Linux source references were not
  independently verified during M2.4. Firmware licensing/redistribution,
  physical-card IP versions, and macOS behavior remain unresolved.

## 2026-10-20 — M2.5 offline upstream provenance verification

- **Objective:** Determine whether local Git objects, source copies, or
  provenance records independently substantiate the pinned upstream source
  references in the RX 7600 research documents.
- **Recovery:** Read `RESUME.md` first, then `WORKLOG.md`, `README.md`,
  `RX7600_RESEARCH.md`, `RX7600_IP_FIRMWARE_MAP.md`,
  `RX7600_EVIDENCE_GAPS.md`, `RX7600_REPRODUCIBILITY_AUDIT.md`, and
  `RX7600_SOURCE_LICENSE_AUDIT.md`. Existing untracked files and cache
  artifacts were preserved.
- **Commands and observed Git state:** Ran the Git state/ref commands and
  source-object/path queries recorded verbatim in
  `RX7600_PROVENANCE_VERIFICATION.md`. Branch `main`; HEAD, upstream, and
  merge base `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`.
  `git rev-parse --is-shallow-repository` returned `false`.
- **Provenance result:** `git cat-file -t` could not resolve Linux commit
  `d0c647a6aae2ce577372594a176c82733e1ed267` or PCI database commit
  `22da7386ba818c037894f258248734770757b14f`. `git ls-tree` for the Linux
  commit returned “not a tree object.” Neither tracked-path queries nor an
  exact-name working-tree search found the cited upstream Linux files or
  `pci.ids`. No upstream source checksum/provenance manifest was found.
  Thus the source behavior and commit contents remain documented but not
  independently verified from the current repository. This does not prove
  whether the earlier M2.1 online research did or did not inspect those
  sources; no contradictory evidence was found.
- **Evidence boundary:** Local Navi 48 source headers and notices substantiate
  only the presence of those local attribution statements, not the referenced
  upstream source contents. Fixture hashes apply only to generated synthetic
  data. The physical RX 7600's exact IP versions remain **UNRESOLVED**.
- **Files changed:** Created `RX7600_PROVENANCE_VERIFICATION.md`; added its
  link to `README.md`; appended this M2.5 entry and updated `RESUME.md`. All
  edits are under `tools/ipdisc/`.
- **Test and links:** The prescribed offline test and final local-link check
  were executed; exact output and results are recorded in the final M2.5
  verification entry below.
- **Safety:** No network, external repository, hardware, PCI, MMIO, VRAM, or
  firmware access. No source or firmware was downloaded or executed. No
  driver/system changes or Git mutation operations were performed.
- **Single safest next action:** Retain the documented/not independently
  verified status. Only a separately authorized task may establish upstream
  provenance using a trusted local source artifact; do not fetch source,
  access hardware, or begin implementation in M2.5.

### M2.5 final verification

- **Test command/result:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`; exit code 0. Each of 22 test methods
  reported `ok`. Exact summary:
  `Ran 22 tests in 0.275s`
  `OK`
  22 passed, 0 failed, 0 skipped.
- **Local links:** Checked links in `README.md`,
  `RX7600_PROVENANCE_VERIFICATION.md`, `RESUME.md`, and `WORKLOG.md`;
  all local targets resolved. No external links were accessed.
- **Final Git state:** Branch `main`; HEAD, `origin/main`, and merge base
  all `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`; no
  staged or tracked diff. Seventeen untracked paths, all under
  `tools/ipdisc/`: `README.md`, `RESUME.md`, `RX7600_EVIDENCE_GAPS.md`,
  `RX7600_IP_FIRMWARE_MAP.md`, `RX7600_PROVENANCE_VERIFICATION.md`,
  `RX7600_REPRODUCIBILITY_AUDIT.md`, `RX7600_RESEARCH.md`,
  `RX7600_SOURCE_LICENSE_AUDIT.md`, `WORKLOG.md`,
  `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`. No paths outside the authorized directory were
  reported.
- **Outcome:** Repository-local evidence does not authenticate the cited
  Linux/PCI source objects or contents. This does not disprove historical
  upstream inspection. Exact physical-card IP versions and firmware mapping
  remain unresolved. No implementation or macOS support is established.

## 2026-10-20 — M2.6 research exit criteria and next-step decision

- **Objective:** Review M0–M2.5 evidence, compare pause/source verification/
  more documentation/implementation paths, and set prerequisites for any
  future proposal without starting implementation.
- **Recovery and actual starting Git state:** Read `RESUME.md` first, then
  `WORKLOG.md`, `README.md`, `RX7600_RESEARCH.md`,
  `RX7600_IP_FIRMWARE_MAP.md`, `RX7600_EVIDENCE_GAPS.md`,
  `RX7600_REPRODUCIBILITY_AUDIT.md`, `RX7600_SOURCE_LICENSE_AUDIT.md`, and
  `RX7600_PROVENANCE_VERIFICATION.md`. Branch `main`; HEAD, `origin/main`,
  and merge base `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence
  `0 0`; no staged or tracked diffs. Seventeen existing untracked paths,
  all in `tools/ipdisc/`, including both existing Python caches; all were
  preserved.
- **Decision:** Recommend Path A, pause until trustworthy, board-specific
  RX 7600 IP Discovery evidence is supplied. Path B requires separately
  explicit network authorization and cannot determine board IP records.
  Path C would repeat completed audits without new evidence. Path D is
  **NO-GO** because the board-specific inventory, independently verified
  upstream source, firmware provenance/terms, macOS build strategy, and
  system recovery plan are not established.
- **Report:** Created `RX7600_NEXT_STEP_DECISION.md` with a four-path
  decision matrix, evidence status, explicit D rejection, and minimum exit
  criteria for any later proposal. Linked it from `README.md`; updated this
  append-only worklog and `RESUME.md`. All edits are within `tools/ipdisc/`.
- **Evidence boundary:** Local facts and historical claims are distinguished.
  M2.5 establishes local absence of the cited upstream commit objects and
  files; it does not prove or disprove M2.1's historical upstream inspection.
  Exact physical-card IP versions remain **UNRESOLVED**. No macOS support is
  claimed.
- **Verification:** Local links and `python -B test_ipdiscovery.py` were
  checked; exact final results and Git state are appended below.
- **Safety:** No network, GPU, PCI, MMIO, VRAM, firmware, driver, plist, EFI,
  boot argument, or system access/change. No stage, commit, reset, clean,
  stash, or delete operation.
- **Single safest next action:** Pause. Resume only when a trusted,
  board-attributed artifact is supplied; if source provenance is needed,
  request separate authorization for network verification. Do not begin
  implementation.

### M2.6 final verification

- **Test:** From `tools/ipdisc/`, ran `python -B test_ipdiscovery.py`;
  exit code 0. All 22 tests reported `ok`. Exact summary:
  `Ran 22 tests in 0.291s`
  `OK`
  22 passed, 0 failed, 0 skipped.
- **Links:** Checked local Markdown links in `README.md`,
  `RX7600_NEXT_STEP_DECISION.md`, `RESUME.md`, and `WORKLOG.md`; all
  resolved.
- **Final Git state:** Branch `main`; HEAD, `origin/main`, and merge base
  `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`; no staged
  or tracked diff. Eighteen untracked paths, all under `tools/ipdisc/`:
  `README.md`, `RESUME.md`, `RX7600_EVIDENCE_GAPS.md`,
  `RX7600_IP_FIRMWARE_MAP.md`, `RX7600_NEXT_STEP_DECISION.md`,
  `RX7600_PROVENANCE_VERIFICATION.md`,
  `RX7600_REPRODUCIBILITY_AUDIT.md`, `RX7600_RESEARCH.md`,
  `RX7600_SOURCE_LICENSE_AUDIT.md`, `WORKLOG.md`,
  `__pycache__/ipdiscovery.cpython-312.pyc`,
  `__pycache__/mkfixture.cpython-312.pyc`, `cpp_parity.md`, `dump.py`,
  `ipdiscovery.py`, `mkfixture.py`, `parity_cases.json`, and
  `test_ipdiscovery.py`. Existing caches were preserved. No paths outside
  the authorized directory were reported.
- **Decision:** Pause pending board-specific evidence. D remains **NO-GO**.
  No implementation, network, hardware, firmware, or system action occurred.

## Research pause and safe handoff — 2026-10-20

- **Status:** `PAUSED — WAITING FOR TRUSTWORTHY EVIDENCE`; driver
  implementation **NO-GO**. Exact physical RX 7600 IP versions remain
  **UNRESOLVED**, and no trustworthy board-specific IP Discovery artifact is
  available in the reviewed project files.
- **Resume only when:** (1) a provenance-documented IP Discovery artifact
  attributable to the actual board is supplied; (2) separate explicit
  authorization is given for upstream verification requiring network access;
  or (3) authoritative license material for specific firmware under
  consideration becomes available. One condition alone does not authorize
  implementation; reassess every remaining prerequisite.
- **Other unresolved gates:** upstream Linux/PCI references are documented
  but not independently verified from this checkout; firmware requirements
  and redistribution terms are incomplete; macOS integration/build strategy
  is unresolved. A tested recovery plan is required before hardware
  experiments; the reported Ryzen 5 7500F has no iGPU.
- **Handoff verification:** Rechecked actual Git state: branch `main`, HEAD
  and `origin/main` `d1f7bad8408a0a2c016b067c477422975a59de44`, divergence
  `0 0`, no staged or tracked changes, 18 untracked paths all under
  `tools/ipdisc/`. Existing files and cache artifacts preserved.
- **Scope:** Updated only `RESUME.md` and appended this entry. No network,
  hardware, firmware, or driver access/change. No macOS support claim.

## 2026-10-20 — Navi 33 offline architecture research skeleton

- **Authorization and scope:** Implemented only the isolated, hardware-
  independent research model requested for Navi 33. This does not reverse the
  prior NO-GO for functional driver development and does not establish RX 7600
  support by macOS.
- **Module:** Added `src/navi33-research/` with typed device identity,
  capability/evidence/firmware states, structured validation results,
  research-only driver stages, and an offline architecture selector.
  `1002:7480` can select only `Navi33ResearchOnly`; the separate Navi 48
  classification returns `Navi48NoInitialization`. Unknown IP versions and
  firmware metadata fail closed. The module has no hardware or macOS APIs.
- **Documentation:** Added `RX7600_ARCHITECTURE_SKELETON.md`; updated this
  README and `RESUME.md`; added module build/test instructions. Historical
  pause records remain intact.
- **C++ tests/build:** Added 13 synthetic C++ checks. C++ compilation and
  execution were **not run**: `Get-Command` found no `clang++`, `g++`, `c++`,
  `cl`, `make`, `mingw32-make`, `cmake`, or `ninja`. No software was installed.
  The reproducible commands are in `src/navi33-research/README.md`.
- **Existing parser suite:** Ran `python -B test_ipdiscovery.py` from
  `tools/ipdisc/`; exit 0. Exact summary:
  `Ran 22 tests in 0.484s`
  `OK`
  All 22 passed; 0 failed and 0 skipped.
- **Other checks:** Local Markdown links in four files at the time of the
  check resolved; `git diff --check` produced no errors. Source scan found no
  IOKit, PCI/MMIO, VRAM, or direct hardware access API references in the
  module source. No hardware, network, firmware, or system interfaces were
  accessed.
- **Final Git state:** Branch `main`; HEAD, `origin/main`, and merge base
  `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`; no staged or
  tracked changes. Twenty-three untracked paths: four new module files and 19
  `tools/ipdisc/` files, including all previously present source/documentation
  and both `__pycache__` artifacts. No pre-existing files were deleted or
  overwritten.
- **Remaining:** Physical RX 7600 IP versions, upstream provenance,
  firmware requirements/licensing, macOS integration/build strategy, and a
  tested recovery plan remain unresolved. No hardware initialization,
  firmware loading, driver integration, or macOS compatibility is claimed.
- **Next smallest task:** Run the C++17 build and unit tests using an already
  available compiler, then address only failures confirmed by that run.

### Final documentation and state check

- Rechecked local Markdown links after updating the worklog and checkpoint:
  9 local links in 5 files resolved.
- Rechecked Git status and refs: `main`, HEAD/upstream/merge base
  `d1f7bad8408a0a2c016b067c477422975a59de44`, divergence `0 0`, no staged or
  tracked changes, 23 untracked paths. The 18 pre-existing `tools/ipdisc/`
  paths and caches remain present.
- `git diff --check` produced no output. The C++ compiler remains unavailable;
  no C++ build was claimed.

## 2026-10-20 — Phase 2 Navi 33 skeleton validation

- **Scope:** Read the recovery checkpoint, prior worklog, architecture-skeleton
  report, module README, and all three C++ sources before modifying anything.
  Validated the offline model only; no hardware or macOS-driver milestone was
  started.
- **Findings and fixes:** (1) An out-of-range `ArchitectureVersionState` enum
  value could bypass the `Unsupported` comparison; selection now explicitly
  rejects unknown enumerators. (2) `EvidenceValidation.complete` conflated a
  structurally valid unknown IP record with complete evidence; it is now
  `structurallyValid` plus `evidenceComplete`, so unknown IP remains valid but
  incomplete. (3) `hasVerifiedFirmwareEvidence` overstated what checking
  caller-set flags establishes; it is now
  `firmwareEvidenceClaimsComplete`, and the result says the flags do not
  authenticate firmware or authorize loading. (4) The prior stage test only
  asserted enum ordinals, and `CapabilityState::Supported` allowed positive
  claims without evidence; removed that assertion/value and added behavior
  tests for invalid enum values and evidence boundaries. Added direct tests
  for unknown architecture families and missing labels.
- **Changed files:** Updated
  `src/navi33-research/include/Navi33Research.hpp`,
  `src/navi33-research/src/Navi33Research.cpp`,
  `src/navi33-research/tests/test_Navi33Research.cpp`,
  `src/navi33-research/README.md`,
  `tools/ipdisc/RX7600_ARCHITECTURE_SKELETON.md`, this worklog, and
  `tools/ipdisc/RESUME.md`. Existing parser code/tests and historical pause
  entries were not changed.
- **C++ build/test status:** Checked `clang++`, `g++`, `c++`, `cl`, `make`,
  `mingw32-make`, `cmake`, `ninja`, and `zig` using PowerShell `Get-Command`;
  all were unavailable. Also checked common LLVM/MSYS2 compiler paths; none
  existed. Therefore the C++ source was **not compiled** and its 19 assertion
  checks were **not executed**. No toolchain or dependency was installed.
  Reproducible commands remain in `src/navi33-research/README.md`.
- **Python regression:** From `tools/ipdisc/`, ran
  `python -B test_ipdiscovery.py`; exit code 0. Exact summary:
  `Ran 22 tests in 0.389s`
  `OK`
  22 passed, 0 failed, 0 skipped.
- **Other checks:** `git diff --check` emitted no errors. Local Markdown links
  were checked after report updates. Static source/API review confirmed the
  module has no IOKit, PCI/MMIO, VRAM, firmware-loader, or initialization
  interfaces. Driver stages are labels only; no stage transition executor or
  hardware side effects exist. No physical GPU, connected device, firmware
  interface, or network was accessed.
- **Final Git state:** Branch `main`; HEAD, `origin/main`, and merge base
  `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`. No staged or
  tracked changes. The same 23 untracked paths remain (four
  `src/navi33-research/` files and 19 `tools/ipdisc/` paths, including the
  pre-existing parser files and both `__pycache__` artifacts). No files were
  staged, committed, reset, cleaned, stashed, or deleted.
- **Remaining limitations:** No C++ compiler execution; caller-provided
  provenance/confidence/license state is represented but not authenticated by
  this model. Physical RX 7600 IP versions, firmware needs/terms, macOS
  integration, and recovery strategy remain unresolved. No RX 7600 macOS
  support is claimed.
- **Next smallest task:** Build and run the C++17 tests with an already
  available compiler, then fix only compiler/test failures actually observed.

## 2026-10-20 — Phase 2.1 C++17 build and test validation

- **Scope:** Re-read `RESUME.md`, `WORKLOG.md`, the architecture-skeleton
  report, module README, and all Navi 33 C++ sources before checking the
  toolchain. Starting repository state was branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, with no staged/tracked changes
  and 23 untracked paths (including pre-existing parser artifacts).
- **Toolchain search:** PowerShell `Get-Command` checks found no `clang++`,
  `clang++.exe`, `g++`, `g++.exe`, `c++`, `c++.exe`, `cl`, `cl.exe`, `make`,
  `cmake`, or `ninja` on PATH. The VS environment variables
  `VSINSTALLDIR`, `VCINSTALLDIR`, `VCToolsInstallDir`, `VisualStudioVersion`,
  and `DevEnvDir` were unset. `vswhere.exe` was absent from its standard
  installer locations. Common LLVM/MSYS2 compiler paths and standard Visual
  Studio installation roots were not present. No C++ compiler/version could
  be reported. No compile command or C++ executable was run; C++ tests remain
  **uncompiled and unexecuted**.
- **C++ review and correction:** Reviewed all 19 synthetic test assertions,
  architecture selection, invalid enum rejection, evidence-state handling,
  and initialization guard. Renamed `EvidenceValidation::evidenceComplete`
  to `claimsComplete` because the model can inspect caller-supplied status
  fields but cannot authenticate evidence. The module README and architecture
  report now state this limitation. No other C++ defect was confirmed during
  this milestone; no assertions were weakened.
- **Python regression:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit code 0. Output summary:
  `Ran 22 tests in 0.407s`
  `OK`
  All 22 passed, 0 failed, 0 skipped. This does not validate C++.
- **Documentation/hygiene:** Local Markdown links were rechecked after report
  updates. `git diff --check` exited 0. No hardware, firmware, network, or
  macOS integration was accessed.
- **Final Git state:** Branch `main`; HEAD and `origin/main`
  `d1f7bad8408a0a2c016b067c477422975a59de44`, divergence `0 0`; no staged or
  tracked modifications. The same 23 untracked paths remain, including all
  pre-existing `tools/ipdisc/` files and cache artifacts. No stage, commit,
  reset, clean, stash, or delete operation occurred.
- **Result/blocker:** The source test suite contains 19 assertions, but
  reproducible compile-and-run evidence is blocked by the absence of an
  existing C++17 toolchain and Developer Command Prompt environment. No
  software was installed and no compilation was simulated.
- **Next smallest task:** Run the README's C++17 compile and test commands in
  a pre-provisioned environment with an available compiler; do not proceed to
  hardware testing or macOS integration.

## 2026-10-20 — Phase 2.2 external toolchain readiness

- **Scope:** Read the current `RESUME.md`, worklog, architecture-skeleton
  report, module README, and Navi 33 C++ source/test files. Confirmed actual
  Git state before checking alternatives. No code changes or system changes
  were made.
- **Existing alternatives checked:** `wsl.exe --list --quiet` returned exit 1
  and stated that Windows Subsystem for Linux is not installed. `Get-Command`
  found no `docker`, `podman`, `vmrun`, `VBoxManage`, `qemu-system-x86_64`, or
  `multipass`. The repository has no `.github/workflows` or other checked
  local CI configuration, and `GITHUB_ACTIONS`, `RUNNER_NAME`, `RUNNER_OS`,
  and `CI` are unset. A configured `origin` exists, but no remote service was
  accessed. Prior Phase 2.1 PATH/Visual Studio/common compiler-path checks
  found no C++ toolchain and are not repeated here.
- **Portability review:** The documented clang++ and MSVC commands are
  explicit and dependency-free; tests use standard C++17 facilities and
  synthetic data. No portability defect justifying a code or build-system
  change was identified. No command was used to simulate compilation.
- **C++ result:** No accessible compiler, WSL distribution, local VM/container
  environment, or configured CI runner was found. C++ sources remain
  **uncompiled** and the 19 assertion checks remain **unexecuted**.
- **Python regression:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit code 0. Exact summary:
  `Ran 22 tests in 0.359s`
  `OK`
  All 22 passed, 0 failed, 0 skipped. This is parser regression evidence only.
- **Safety:** No installation, download, network access, account/remote
  service access, GPU/device/firmware access, or system configuration change.
  Nothing was staged, committed, reset, cleaned, stashed, or deleted.
- **Final Git state:** Branch `main`; HEAD and `origin/main`
  `d1f7bad8408a0a2c016b067c477422975a59de44`; divergence `0 0`; no staged or
  tracked changes. The same 23 untracked paths remain, including the four
  Navi 33 skeleton files, prior parser documents/sources, and both existing
  `__pycache__` artifacts.
- **Next practical step:** From a pre-provisioned Windows Developer Command
  Prompt with an existing MSVC toolchain, run the `cl /std:c++17 ...` and
  resulting `navi33-research-tests.exe` commands in
  `src/navi33-research/README.md`; otherwise use its documented clang++ pair
  from an already available Linux/macOS development host. Keep the test
  output and compiler version as the validation artifact. No environment
  setup or hardware testing is authorized here.

## 2026-10-20 — Phase 3 RX 7600 evidence acquisition plan

- **Scope:** Offline evidence planning and schema only. Read the current
  research checkpoint, worklog, next-step decision, evidence gaps, IP/firmware
  map, and provenance report. Initial state was branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, with no staged or tracked
  changes and 23 pre-existing untracked paths.
- **Deliverables:** Added `RX7600_EVIDENCE_ACQUISITION_PLAN.md` with
  artifact/provenance requirements, source-vs-board evidence distinctions,
  source-only limitations, licensing controls, and decision gates. Added
  `RX7600_EVIDENCE_SCHEMA.md` defining record fields and explicit
  `UNVERIFIED`, `SOURCE_VERIFIED`, `BOARD_SPECIFIC`, and `TARGET_TESTED`
  statuses. Updated `RESUME.md` with this checkpoint.
- **Evidence boundary:** No source objects were fetched or independently
  inspected, and no hardware or firmware was accessed. Linux revision
  `d0c647a6aae2ce577372594a176c82733e1ed267` and PCI revision
  `22da7386ba818c037894f258248734770757b14f` remain citations reported by
  prior project documents, not independently verified source evidence in
  this checkout. The plan does not claim earlier moving-reference history is
  established. Physical RX 7600 IP versions remain **UNRESOLVED**.
- **Regression:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit code 0. Exact summary:
  `Ran 22 tests in 0.396s`
  `OK`
  All 22 passed; 0 failures and 0 skips. The tests use synthetic parser
  fixtures and do not establish board facts or macOS support.
- **Git/safety:** Preserved all 23 pre-existing untracked paths and cache
  artifacts. The two new plan/schema files bring the total to 25 untracked
  paths, all under the pre-existing `src/navi33-research/` and
  `tools/ipdisc/` untracked trees. No files were staged, committed, reset,
  cleaned, stashed, or deleted. No dependency was installed and no network,
  hardware, PCI/MMIO/VRAM, or firmware access occurred.
- **Decision/next step:** Remain paused. The smallest useful next action is
  offline review of a user-provided, provenance-documented IP Discovery
  artifact and board-identification metadata, or separate authorization for
  a bounded upstream source verification task. Neither permits hardware
  testing or implementation by itself.

## 2026-10-20 — Phase 4 bounded upstream source verification

- **Scope and starting state:** Read `RESUME.md`, `WORKLOG.md`, the Phase 3
  evidence plan/schema, provenance verification, and IP/firmware map. Before
  research, branch was `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, with 25 untracked paths.
- **Deliverable:** Added `RX7600_UPSTREAM_SOURCE_VERIFICATION.md` and updated
  `RESUME.md`. The report records repository/revision identity, exact source
  paths, narrow supported claims, source limitations, pinned URLs, retrieved
  file sizes and SHA-256 values where available, and the HTTP 429 gaps.
- **Commit verification:** Official GitHub APIs independently returned Linux
  commit `d0c647a6aae2ce577372594a176c82733e1ed267` and PCI database commit
  `22da7386ba818c037894f258248734770757b14f` with matching full SHAs and
  trees. GitHub reports both commits as unsigned. This verifies
  retrievability, not cryptographic author signature.
- **Source findings:** Verified the cited Linux `discovery.h` structures,
  generic `CHIP_IP_DISCOVERY` entries and lack of `0x7480`/`CHIP_NAVI33`
  literals in pinned `amdgpu_drv.c`, selected version cases/names/symbols
  from retrieved files, and the `1002:7480` Navi 33 multi-product label in
  pinned `pci.ids`. This is generic source/database evidence only.
- **Unverified scope:** The exact raw `amdgpu_discovery.c` and
  `smu_v13_0.c` requests returned HTTP 429. Detailed IP dispatch and SMU
  request claims remain unverified in this pass; other source behavior is
  claimed only to the checked literals/symbols stated in the report. No
  source was substituted from a moving branch or mirror.
- **Evidence categories:** No `BOARD_SPECIFIC` or `TARGET_TESTED` evidence
  was obtained. The physical RX 7600 IP inventory remains **UNRESOLVED**;
  Linux source verification does not prove macOS compatibility.
- **Regression:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit code 0. Exact summary:
  `Ran 22 tests in 0.376s`
  `OK`
  All 22 passed; 0 failures and 0 skips. Tests use synthetic fixtures.
- **Final state/safety:** Existing untracked files and cache artifacts were
  preserved; the Phase 4 report brings the count to 26 untracked paths.
  Nothing was staged, committed, reset, cleaned, stashed, or deleted. No
  firmware or hardware was accessed.
- **Decision/next gate:** Project remains paused. Next, obtain a
  provenance-documented board-specific IP Discovery artifact for offline
  strict parsing, or separately authorize a bounded check of the exact files
  still unverified. Neither step authorizes hardware testing or
  implementation.

## 2026-10-20 — Phase 5 remaining upstream source claims

- **Scope and initial state:** Read the current checkpoint, worklog, Phase 4
  source report, and evidence schema. The precise gaps were the detailed
  `amdgpu_discovery.c` dispatch/record behavior and `smu_v13_0.c` firmware
  request conditions at Linux commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`. Initial branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, 26 untracked paths.
- **Retrieval method:** Used the official GitHub Contents API for each exact
  path with the full commit SHA, then decoded the returned Git Blob API data
  in memory. No raw endpoint retry or mirror was used; source files were not
  persisted in the checkout.
- **Deliverable:** Added `RX7600_REMAINING_SOURCE_CLAIMS.md` and updated
  `RX7600_UPSTREAM_SOURCE_VERIFICATION.md` with a Phase 5 follow-up and
  `RESUME.md` with the new checkpoint.
- **Verified `amdgpu_discovery.c`:** Blob
  `163b40c8afb06c69f17f708f4b11f57a370bd5af`, 100,281 bytes, SHA-256
  `7159e6c3cf5101120bef3f33c4f2e23ea9632a096c7d76d47e17d2df23880d32`.
  Confirmed the v1/v2 table-list selector; IP-record-to-version/register
  mapping; and the cited generic, version-dependent common/GMC/IH/PSP/SMU/
  display/GFX/SDMA/VCN/MES switch behavior. Exact function links and
  line ranges are in the Phase 5 report.
- **Verified `smu_v13_0.c`:** Blob
  `4e09eda77d8b8acf4dcd29b47cf0c257c4287577`, 71,132 bytes, SHA-256
  `4c5a3d31dc6315f613692744e06760c948ee1c94a1800d1ccc2576fa3e4b1093`.
  Confirmed that `smu_v13_0_init_microcode()` skips requests for an SR-IOV
  VF; otherwise decodes MP1 to a prefix and requests the `%s.bin` or
  `%s_kicker.bin` path as `AMDGPU_UCODE_REQUIRED`. The exact prefix mapping
  and all target implications are not asserted by this check.
- **Evidence boundary:** These are generic Linux source facts, not a Navi 33
  PCI-ID-specific inventory. No `BOARD_SPECIFIC` or `TARGET_TESTED` evidence
  was obtained; physical RX 7600 IP versions remain **UNRESOLVED** and macOS
  support is not established.
- **Regression:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit code 0. Exact summary:
  `Ran 22 tests in 0.376s`
  `OK`
  All 22 passed; 0 failures and 0 skips.
- **Final state/safety:** The new report brings the untracked count to 27;
  all existing untracked artifacts and both cache files were preserved. No
  files were staged, committed, reset, cleaned, stashed, or deleted. No
  firmware or hardware was accessed.
- **Next gate:** Remain paused until provenance-documented board-specific
  evidence is available for offline strict parsing or a separate evidence
  task is explicitly authorized. Source verification does not authorize
  implementation or target testing.

## 2026-10-20 — Phase 6 offline IP Discovery artifact intake

- **Scope and initial state:** Read the Phase 5 checkpoint, worklog,
  evidence plan/schema/gaps, upstream source reports, and inspected
  `ipdiscovery.py`, `dump.py`, `test_ipdiscovery.py`, and `README.md`.
  Initial branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, with 27 untracked paths.
- **Documentation:** Added `RX7600_ARTIFACT_INTAKE.md`; linked it from
  `README.md`; appended this entry; updated `RESUME.md` and the README's
  recorded test output. The procedure requires original filename, source
  device, capture method/time, tool/version, size, custody/transformation
  history, and SHA-256; it distinguishes physical capture candidates from
  generated fixtures and generic examples.
- **Parser behavior reviewed:** CLI accepts a raw binary path, reads it fully
  into memory, scans four-byte-aligned signature positions, and defaults to
  strict mode. There is no configured input-file size cap; the 16-bit
  declared binary size is at most 65,535 bytes and does not cap an enclosing
  file. Strict mode supports binary header major versions 0-2 and IP
  Discovery versions 1-4 with one die, up to 256 records and 16 32-bit base
  addresses per record. It validates bounds, overlap, signatures and
  checksums; v4 64-bit bases are explicitly unsupported. Compatibility mode
  is not suitable for validating untrusted input. No concrete parser defect
  was identified; parser behavior and tests were not changed.
- **Regression:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit code 0. Exact summary:
  `Ran 22 tests in 0.276s`
  `OK`
  All 22 passed; 0 failures and 0 skips. The suite uses synthetic fixtures
  and is not board evidence.
- **Evidence boundary:** No physical-card artifact was supplied, acquired,
  generated, or parsed. No network or hardware was accessed. Physical RX
  7600 IP versions remain **UNRESOLVED**; parser success cannot prove
  provenance, board attribution, completeness, firmware requirements, or
  macOS support.
- **Final state/safety:** The intake report adds one untracked path, bringing
  the total from 27 to 28. All prior untracked files and both `__pycache__`
  artifacts were preserved. Nothing was staged, committed, reset, cleaned,
  stashed, or deleted; no software was installed.
- **Next step:** Await a genuine artifact and accompanying provenance and
  board-identification evidence. Then hash and preserve the original, verify
  a byte-identical analysis copy, and run the CLI in strict mode only in an
  offline, resource-bounded environment.

## 2026-10-20 — Phase 7 bounded offline artifact intake

- **Scope and initial state:** Read the Phase 6 checkpoint, intake procedure,
  README, parser, CLI, tests, and recent worklog. Confirmed the CLI previously
  used `Path.read_bytes()` without a size cap; the parser itself requires at
  least 512 bytes and scans for a signature at four-byte offsets. Initial
  branch `main`, HEAD `d1f7bad8408a0a2c016b067c477422975a59de44`, with 28
  untracked paths.
- **Implementation:** `dump.py` now caps CLI input at 1 MiB. It rejects
  non-regular files; checks size through the opened descriptor before
  reading; reads bounded chunks no larger than limit+1; and rechecks size
  after reading. Growth beyond the limit is rejected during reading; size
  changes detected around the read are rejected. Oversize/change errors are
  reported to stderr with exit code 2. Input is never silently truncated.
  The default is fixed (not user-configurable) and documented.
- **Rationale/compatibility:** The format's 16-bit binary-size field allows
  at most 65,535 declared bytes. A 1 MiB CLI cap allows a prefix and modest
  surrounding bytes while bounding memory and signature-scan work. The cap
  affects only CLI inputs above that policy; strict/compatibility parser
  behavior and valid supported fixtures are unchanged. Existing minimum
  buffer behavior remains (under 512 bytes reaches parser and fails with
  exit 1).
- **Tests:** Added tests for valid in-limit CLI input, 511-byte input,
  oversized sparse file and clear exit/message, an input stream that grows
  past a small test limit, actual temporary-file growth during the read,
  unsupported binary header, and strict overlap rejection at the CLI.
  Existing parser malformed/overlap/version tests remain.
- **Regression:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit code 0. Exact summary:
  `Ran 27 tests in 0.687s`
  `OK`
  All 27 passed; 0 failures and 0 skips. Tests used only generated synthetic
  fixtures and temporary test files.
- **Documentation/checks:** Updated `README.md` and
  `RX7600_ARTIFACT_INTAKE.md`; refreshed this checkpoint. `git diff --check`
  and local Markdown-link/whitespace checks passed.
- **Evidence/safety/final state:** No real board artifact was supplied,
  obtained, or parsed. No network or hardware was accessed. The same 28
  untracked paths and cache artifacts remain. Nothing was staged, committed,
  reset, cleaned, stashed, or deleted. RX 7600 IP versions remain
  **UNRESOLVED** and no macOS support claim is made.
- **Next step:** Await a genuine, provenance-documented artifact and analyze
  a hash-verified copy with strict mode within the documented input cap.

## 2026-10-20 — Phase 8 Navi 33 research build readiness

- **Scope and initial state:** Audited the isolated C++ research model,
  public header, existing synthetic tests, README, and repository build
  conventions. Initial branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, with 28 untracked paths.
  No root or local CMake configuration existed. No source defect was found
  in the existing routing/evidence behavior.
- **Build configuration:** Added `src/navi33-research/CMakeLists.txt` for
  only the isolated static library and its test executable. It requires
  CMake 3.16 and C++17, uses the standard library only, enables CTest, and
  enables compiler warnings as errors. Updated the local README with
  repository-root CMake configure/build/test commands and direct clang++
  instructions. Generated outputs are directed to
  `build/navi33-research`.
- **Test coverage:** Added fail-closed checks for out-of-range architecture
  family and firmware evidence enum values. Existing synthetic tests cover
  `1002:7480` selecting only `Navi33ResearchOnly`, unknown IDs, Navi 48
  separation, incomplete evidence, non-authentication of caller claims, and
  disabled hardware initialization. No production driver or hardware path
  was changed.
- **Additional routing audit:** Read-only search found no `1002:7480` match in
  `src/navi48-bringup`. Its four `IOPCIPrimaryMatch` declarations in
  `Info.plist` (lines 31-32, 46-47, 68-69, and 85-86) list only
  `0x75501002` and `0x75511002`. This confirms the declared match IDs do not
  include `1002:7480`; it does not establish runtime behavior or validate
  loading.
- **Tool availability:** Checked `clang++`, `g++`, `c++`, `cl`, `cmake`,
  `ninja`, and `make`; all were unavailable. Consequently no C++ configure,
  compile, link, or test command could be executed. C++ source review is not
  executable validation.
- **Regression/checks:** From `tools/ipdisc/`, ran exactly
  `python -B test_ipdiscovery.py`; exit 0, `Ran 27 tests in 0.738s`, `OK`
  (27 passed, 0 failed, 0 skipped; synthetic parser fixtures only).
  `git diff --check` exited 0. The README whitespace/local-link check
  reported `Checked 1 Markdown files; 0 local links` and passed. These
  checks do not validate C++ compilation.
- **Final state and limits:** Branch `main`, HEAD remains
  `d1f7bad8408a0a2c016b067c477422975a59de44`; 29 untracked paths, including
  the two existing `__pycache__` artifacts. Existing untracked paths were
  preserved. No files were staged, committed, reset, cleaned, stashed, or
  deleted; no toolchain was installed. No hardware was accessed, and neither
  macOS support nor board-specific evidence was established.
- **Next justified action:** Run the documented CMake/CTest sequence in an
  environment that already has CMake 3.16+ and a C++17 compiler; until then,
  the C++ build/test result remains **UNVERIFIED**.

## 2026-10-20 — Phase 9 offline intake race checks and fuzz coverage

- **Environment recheck:** Confirmed branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, and the same 29 untracked
  paths. `clang++`, `g++`, `c++`, `cl`, `cmake`, `ninja`, and `make` remain
  unavailable. Git Bash has no compiler/build tools in its PATH, and
  `wsl.exe --list --quiet` reported that WSL is not installed. No toolchain
  was installed or downloaded.
- **Input mutation gap:** The CLI already bounded input and detected size
  changes, but only compared file size before/after reading. A same-size
  in-place mutation was not detectable from that check. `dump.py` now also
  compares the already-open file's `st_mtime_ns` and `st_ctime_ns`; a
  detected metadata change raises `InputChangedError`, and the existing CLI
  reports it with exit status 2. This is best-effort, not an immutable-file
  guarantee: unreliable or deliberately restored timestamps can evade it.
- **Regression coverage:** Added a deterministic test for changed file
  metadata during the read. Strengthened the seeded malformed-input loop
  from 120 to 512 cases; half now place the binary signature at a valid
  four-byte-aligned scan offset with at least 512 bytes remaining, and assert
  the candidate reaches validation rather than merely testing "signature not
  found." Inputs remain synthetic.
- **Build reproducibility:** Set `CXX_EXTENSIONS OFF` for both isolated CMake
  targets so the future build uses standard C++17 rather than compiler
  extensions. README documents the stricter mode. CMake remains unavailable,
  so this configuration change has not been configured or compiled here.
- **Files changed:** `tools/ipdisc/dump.py`,
  `tools/ipdisc/test_ipdiscovery.py`, `tools/ipdisc/README.md`,
  `tools/ipdisc/RX7600_ARTIFACT_INTAKE.md`,
  `src/navi33-research/CMakeLists.txt`, and
  `src/navi33-research/README.md`.
- **Regression/checks:** Baseline command from `tools/ipdisc/`,
  `python -B test_ipdiscovery.py`, exited 0 (`Ran 27 tests in 0.691s`,
  `OK`). After the changes the same command exited 0 (`Ran 28 tests in
  0.832s`, `OK`; 28 passed, 0 failed, 0 skipped). `git diff --check`
  passed; custom whitespace and local Markdown-link checks passed.
- **Evidence/safety:** No hardware or network access, package installation,
  or firmware activity occurred. No board artifact or board-specific
  evidence was obtained; no macOS-support claim is established.
- **Next justified action:** C++ build verification remains blocked until an
  existing CMake 3.16+ and C++17 compiler are available. Avoid speculative
  driver work in the absence of board evidence and a validated build route.

## 2026-10-20 — Phase 10 Navi33/Navi48 boundary verification

- **Architecture gap analysis (checked-in source):** `src/navi48-bringup/`
  contains an actual x86_64 IOKit kext path, not a Navi33 implementation.
  `src/navi48-bringup/Info.plist` declares four personalities whose PCI
  match union is exactly `0x75501002` and `0x75511002`. The runtime
  `Navi48Bringup::probe()` in `src/navi48-bringup/src/Navi48Bringup.cpp`
  (around lines 7327-7339) separately requires `navi48bringup=1`, AMD vendor
  `0x1002`, and device `0x7550` or `0x7551`. `start()` (around lines
  7342-7381) enables PCI memory, latches BARs, maps BAR5, surveys IP/PSP/SMU,
  and may proceed into the staged bring-up path. These paths are hardware
  operations; they were only inspected, never run.
- **Implemented subsystem evidence:** `src/navi48-bringup/src/amd/`
  contains generation-specific GFX12, GMC12, IH7, SDMA7 and SMU14 modules;
  `src/navi48-bringup/src/dcn/navi48_dcn.cpp` is the Navi48 display path;
  `amdgpu_init.cpp` coordinates stages represented in `amdgpu_init.h`,
  including firmware, memory, interrupts, engines, and test dispatch.
  `amdgpu_gart.cpp` documents a platform adaptation from an arm64 DriverKit
  reference to this x86 IOKit kext, including direct host-physical DMA
  assumptions. `BUILDING.md` describes external MacKernelSDK and firmware
  prerequisites and says the experimental kext was run on one x86_64
  RX 9070 XT test PC. The module comments cite upstream commit `3bdeed2`;
  this pass did not retrieve or independently verify that external commit.
- **Navi33 readiness gap:** `src/navi33-research/` is a hardware-independent
  identity/evidence model and has no IOKit provider, BAR access, firmware
  loader, memory/VM, command submission, interrupt, display, or power
  integration. Its physical IP and firmware evidence remain unknown. The
  N48 generation-specific files and the DriverKit-to-IOKit adaptations are
  not proof of Navi33 compatibility or directly portable macOS APIs.
- **Implementation:** Added
  `src/navi33-research/tests/test_navi48_isolation.py`, a stdlib-only static
  contract test checking every plist personality's PCI match union and the
  runtime probe's opt-in, vendor, and device guards. Registered it in CTest
  when Python 3 is found. Documented its static-only scope in the research
  README. This protects an existing architecture boundary; it does not
  implement GPU support.
- **Tests:** From repository root, ran
  `python -B src/navi33-research/tests/test_navi48_isolation.py`; exit 0,
  `Ran 2 tests in 0.003s`, `OK` (2 passed, 0 failed, 0 skipped). From
  `tools/ipdisc/`, ran `python -B test_ipdiscovery.py`; exit 0,
  `Ran 28 tests in 0.724s`, `OK` (28 passed, 0 failed, 0 skipped).
  `git diff --check` passed. CMake/CTest and C++ compilation remain
  unavailable (`cmake`, C++ compilers, Ninja and Make absent); CTest
  registration is not configured or executed here.
- **Evidence/safety/Git:** Facts above are local-source inspection; no
  external source verification, network, physical GPU, PCI configuration,
  MMIO, VRAM, firmware, or target test occurred. No board-specific evidence
  or RX 7600 macOS support is established. Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; 30 untracked paths. All
  previous untracked files and cache artifacts were preserved; nothing was
  staged, committed, reset, cleaned, stashed, or deleted.
- **Next blocker:** A real Navi33-specific plan still requires trustworthy
  board-attributed IP/firmware evidence and a buildable, isolated macOS
  development environment. Do not route `1002:7480` through the Navi48
  bring-up code.

## 2026-10-20 — Phase 11 offline macOS source feasibility

- **Initial state:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; existing untracked research
  and parser files were present and preserved. The only configured Git
  remote is the Cobalt64 origin. No network access was authorized or used.
- **Upstream revision check:** Local source comments cite mac-amdgpu commit
  prefix `3bdeed2`; `NOTICE` identifies `lemonade-sdk/mac-amdgpu` and MIT
  licensing. `git cat-file -e '3bdeed2^{commit}'` failed, so the full commit,
  tree and original source blobs cannot be resolved from local objects.
  Cobalt64 `NOTICE` and `third-party/THIRD-PARTY-LICENSES.txt` record
  mac-amdgpu MIT, RDNA4FB BSD-3-Clause, and Linux amdgpu GPL-2.0 reference
  only. These are checked-in project notices, not independent upstream
  revision/license verification. Do not adapt more source until provenance
  is resolved.
- **Source compatibility findings:** Checked-in Navi48 x86_64 IOKit code is
  explicitly limited to `1002:7550`/`1002:7551` and has GFX12/GMC12/IH7/
  SDMA7/SMU14/Navi48 DCN paths. The x86 IOKit Makefile uses private kernel
  build flags and external MacKernelSDK; `amdgpu_gart.cpp` documents
  DriverKit DMA API replacement with host-physical-memory assumptions. This
  is not a portable Navi33 implementation. Existing provenance reports
  record narrow Linux behavior at
  `torvalds/linux@d0c647a6aae2ce577372594a176c82733e1ed267`; those generic
  Linux dispatch/request facts do not identify the physical board or define
  Apple interfaces. No upstream files were re-fetched in this pass.
- **Compatibility matrix:** Added
  `tools/ipdisc/RX7600_MACOS_SOURCE_FEASIBILITY.md` with local file/symbol
  evidence and subsystem classifications for discovery, firmware/PSP, GFX,
  GMC/VM/VRAM, interrupts, SDMA, SMU, display, kernel/userspace integration,
  Metal and build/licensing. It defines evidence dependencies and selects
  offline IP-artifact intake followed by pure version dispatch as the
  smallest credible vertical slice.
- **Implementation decision:** No driver algorithm or initialization code
  was added. A Navi33 IP-version dispatch model would be speculative without
  a board-attributed discovery artifact; reusing mac-amdgpu code would be
  premature without resolving `3bdeed2` and reviewing original sources. This
  is the explicit implementation blocker, not a claim that driver support
  is impossible.
- **Validation:** Ran from repository root
  `python -B src/navi33-research/tests/test_navi48_isolation.py`; exit 0,
  `Ran 2 tests in 0.003s`, `OK` (2 passed, 0 failed, 0 skipped). Ran from
  `tools/ipdisc/` `python -B test_ipdiscovery.py`; exit 0,
  `Ran 28 tests in 0.724s`, `OK` (28 passed, 0 failed, 0 skipped). Ran
  `git diff --check`; passed. C++ compilation/CMake/CTest were not run;
  tools remain unavailable.
- **Safety and Git:** No network, hardware, PCI config, MMIO, VRAM, firmware,
  privileged operation, installation or system change occurred. No files
  were staged, committed, reset, cleaned, stashed or deleted. Existing
  untracked files and caches were preserved.
- **Next blocker:** Board-attributed Navi33 IP/firmware evidence and the exact
  full mac-amdgpu revision are absent. Resolve those before code reuse or
  version-specific implementation; obtain a supported C++/macOS SDK build
  environment before claiming compile validation.

## 2026-10-20 — Phase 12 evidence categories and local build feasibility

- **Initial/final Git identity:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`. `git status
  --short --untracked-files=all` reports 31 untracked paths and no tracked or
  staged changes. Existing paths, including the two `tools/ipdisc/__pycache__`
  artifacts, were preserved; no stage, commit, reset, clean, stash, or delete
  operation occurred.
- **Bounded local evidence inventory:** No user-supplied or repository-held
  board-attributed RX 7600 IP Discovery artifact, mac-amdgpu checkout/archive,
  or compiler/build tool was available in the inspected project. The only
  project `.bin` files found were
  `src/navi48-bringup/shaders/store_magic.bin` and
  `src/navi48-bringup/shaders/store_magic_hsa.bin`; these are Navi48 shader
  assets, not RX 7600 evidence. The short source citation
  `lemonade-sdk/mac-amdgpu@3bdeed2` remains unresolved locally. No network,
  artifact execution, or external source retrieval was performed. Existing
  reports' pinned Linux revision
  `d0c647a6aae2ce577372594a176c82733e1ed267` remains generic source evidence
  only, not board data or a macOS interface contract.
- **Implementation:** `src/navi33-research/include/Navi33Research.hpp`,
  `src/navi33-research/src/Navi33Research.cpp`, and
  `src/navi33-research/tests/test_Navi33Research.cpp` now use an explicit
  `EvidenceKind` for synthetic fixtures, generic upstream sources,
  board-attributed artifacts, macOS interface reviews, and target hardware
  tests. IP/firmware completeness now requires a recognized kind. The
  board-attribution helper only classifies a caller-supplied label; it does
  not authenticate evidence. C++ tests cover untyped and invalid categories
  and distinguish generic/synthetic/interface categories from board/target
  claims. `src/navi33-research/README.md` documents these semantics. This
  does not establish any RX 7600 IP version, firmware requirement, or support.
- **Tests actually run:** From `tools/ipdisc/`, `python -B
  test_ipdiscovery.py` exited 0: `Ran 28 tests in 0.788s`, `OK` (28 passed,
  0 failed, 0 skipped). From the repository root,
  `python -B src/navi33-research/tests/test_navi48_isolation.py` exited 0:
  `Ran 2 tests in 0.004s`, `OK` (2 passed, 0 failed, 0 skipped).
  `git diff --check` exited 0, but the affected project files are untracked,
  so this command does not check their content. C++ compilation, CMake and
  CTest could not be run: `Get-Command clang++,g++,c++,cl,cmake,ninja,make
  -ErrorAction SilentlyContinue` found none. C++ behavior tests remain
  authored but unexecuted.
- **Safety and next blocker:** Only local source/documentation/test files were
  inspected or changed. No hardware, PCI configuration, MMIO, VRAM, firmware,
  system configuration, or active driver was accessed or changed. No
  board-attributed artifact, resolved `3bdeed2` source revision, or
  build-capable C++/macOS environment is available; do not claim RX 7600
  macOS support or route `1002:7480` through Navi48 initialization.

## 2026-10-20 — Phase 13 pinned mac-amdgpu and DriverKit interface audit

- **Initial Git state:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`, no tracked/index changes, and
  the same 31 pre-existing untracked paths from Phase 12. All were preserved.
- **Upstream resolution:** Public GitHub API resolves the cited prefix to
  `lemonade-sdk/mac-amdgpu@3bdeed2de940504ad6bd1bac718d5de2f65ddb83`,
  tree `7101e9c18d41b22a77c5d06f452a99f50f5a5e6e`, commit date
  2026-05-22, version v0.1.48. GitHub reports the commit unsigned. The exact
  LICENSE blob `473fec8f4a7313c7417e6e3405c6b770ddde6d4b` states MIT.
  Relevant pinned files inspected: `project.yml`,
  `dext/Info.plist`, `dext/MacAMDGPU.iig`, `dext/README.md`,
  root `README.md`, and the `dext/amdgpu/` source inventory.
- **Verified scope and compatibility:** The upstream build declares a
  DriverKit driver-extension, `SDKROOT=driverkit`, `PCIDriverKit.framework`,
  arm64-only, macOS 26.2 and DriverKit 25.2 deployment targets. Its IIG
  interface subclasses DriverKit `IOService` and declares Start/Stop/
  NewUserClient with a PCIDriverKit `IOPCIDevice` provider. Apple docs
  inspected at `developer.apple.com/documentation/pcidriverkit/iopcidevice`
  and `/documentation/driverkit/ioservice` describe PCI configuration/MMIO,
  service lifecycle and user-client entry points. They do not establish an
  AMD GPU acceleration/Metal ABI.
- The pinned upstream source inventory includes GFX12, GMC12, IH7, SDMA7,
  SMU14, PSP14 and MES12 modules. It is documented for Radeon AI PRO R9700
  (`1002:7551`), RDNA4 on Apple Silicon/Thunderbolt, not Navi33. The root
  README reports R9700 initialization and lists first real PM4 submission,
  BO/userspace ABI and Mesa winsys as future work. The same commit's
  `dext/README.md` describes an earlier Phase 1A/no-DMA/no-MSI-X skeleton,
  while its commit message says SDMA ring submissions work. These are
  inconsistent project-authored status claims, not independently verified
  integration tests; the commit is unsigned.
- Cobalt64's existing target is a distinct x86_64 IOKit kext
  (`src/navi48-bringup/Makefile`: private-kernel flags, `-fapple-kext`,
  MacKernelSDK). Its plist and runtime probe restrict matching to
  `1002:7550`/`1002:7551`. Local `amdgpu_init.cpp`, `sdma_v7_0.cpp`,
  `gfx_v12_0.cpp`, and `amdgpu_gart.cpp` blob hashes differ from the pinned
  upstream blob IDs; the adaptations are not verbatim copies and DriverKit
  is not a drop-in API replacement for this IOKit kext.
- **Changes:** Corrected `src/navi48-bringup/NOTICE` with the exact upstream
  commit/license reference and clarified that upstream's R9700 claims do not
  independently validate this tree or establish Navi33 compatibility.
  Updated existing `tools/ipdisc/RX7600_MACOS_SOURCE_FEASIBILITY.md` with
  the pinned source/API evidence, source-status discrepancy and revised
  remaining gates; no duplicate report or hardware code was added.
- **Checks:** From `tools/ipdisc/`, `python -B test_ipdiscovery.py` exited 0:
  `Ran 28 tests in 0.915s`, `OK` (28 passed, 0 failed, 0 skipped). From repo
  root, `python -B src/navi33-research/tests/test_navi48_isolation.py` exited
  0: `Ran 2 tests in 0.004s`, `OK` (2 passed, 0 failed, 0 skipped).
  `git diff --check` exited 0. A targeted trailing-whitespace scan of the
  two edited files found none. C++ compilation, CTest, integration tests and
  physical hardware tests were not run. No toolchain search was repeated.
- **Safety and next gate:** Public source/docs and local files only; no remote
  code executed, source archive downloaded, tool installed, hardware/firmware
  accessed, or system setting changed. Exact RX 7600 IP/firmware data and a
  macOS integration strategy/build environment remain unavailable. Next
  engineering task: obtain a genuine provenance-documented `1002:7480`
  IP Discovery artifact and use it to scope a hardware-independent Navi33
  version-dispatch slice; retain the research-only route until then.

## 2026-10-20 — Phase 14 x86_64 architecture decision and Navi48 start guard

- **Initial state:** Branch `main`, HEAD
  `d1f7bad8408a0a2c016b067c477422975a59de44`; one tracked modification in
  `src/navi48-bringup/NOTICE` and 31 existing untracked files/artifacts. All
  pre-existing paths and the two parser cache files were preserved.
- **Architecture decision:** No full RX 7600 driver architecture is currently
  supported by sufficient board evidence, macOS GPU/userspace interface
  evidence, or build/test infrastructure. If x86_64 work proceeds, the
  lowest-duplication integration shell is a **separate Navi33 service/class
  and per-generation modules** in Cobalt64's existing x86_64 IOKit target;
  this does not reuse Navi48 hardware routines. A standalone driver duplicates
  platform plumbing before solving the same private graphics-ABI and evidence
  gaps. The arm64 mac-amdgpu DriverKit/RDNA4 source is not a drop-in base.
  WhateverGreen at
  `acidanthera/WhateverGreen@093e76fec5655001f38b367dd22459c756ee89fa`
  (BSD-3-Clause) has `RAD::processKernel`, `RAD::processKext`,
  `RAD::processHardwareKext` and Apple Radeon hardware-kext hooks; it patches
  an existing Apple stack, not a standalone Navi33 device implementation.
  NootedRed at
  `ChefKissInc/NootedRed@9ff85a6636a3f01d3d1dc02ae09d057605b76c3c` describes
  glue around existing AMD drivers but its published license prohibits AI
  use, so no code was adapted. The current selected result for a functioning
  RX 7600 driver remains “no viable path demonstrated”; the x86_64 IOKit
  route is only a provisional integration shell.
- **Implementation:** `Navi48Bringup::start()` now independently requires
  the AMD vendor ID and Navi48 device IDs `0x7550`/`0x7551` immediately
  before any PCI memory enable, BAR latch, or MMIO mapping. This prevents
  `1002:7480` from reaching Navi48 hardware setup even if a future matching
  configuration bypasses or drifts from the `probe()` contract. Added
  `test_start_revalidates_identity_before_hardware_setup` to the existing
  static routing-contract test. The check protects source structure; it is
  not a runtime or hardware test.
- **Validation:** From repo root, `python -B
  src/navi33-research/tests/test_navi48_isolation.py` exited 0:
  `Ran 3 tests in 0.006s`, `OK` (3 passed, 0 failed, 0 skipped). From
  `tools/ipdisc/`, `python -B test_ipdiscovery.py` exited 0:
  `Ran 28 tests in 0.762s`, `OK` (28 passed, 0 failed, 0 skipped).
  `git diff --check` exited 0. A targeted trailing-whitespace scan and
  report-local-link check passed. No C++ compilation/CTest, macOS
  integration, or physical GPU validation occurred.
- **Verified vs. unresolved:** Public pinned README, project files, licenses,
  source inventory and Apple DriverKit docs establish only the described
  interface/source scopes. WhateverGreen hooks do not prove driver support
  for RX 7600. mac-amdgpu's unsigned commit and contradictory status docs
  remain unverified project claims. No genuine board-attributed artifact or
  evidence of RX 7600 macOS functionality exists.
- **Safety/final Git:** No GPU, PCI configuration, MMIO, VRAM, firmware,
  active driver, EFI, boot, or system setting was accessed or modified.
  No download/archive, installation, staging, commit, reset, clean, stash,
  or deletion occurred. The tracked Navi48 source change is from this
  milestone; the pre-existing `src/navi48-bringup/NOTICE` modification was
  preserved, not overwritten. The existing untracked routing test and
  feasibility/worklog/resume documents were updated in place. Branch/HEAD
  unchanged; two tracked modifications plus 31 untracked paths remain.
- **Next task:** Obtain a hash-preserved, provenance-documented RX 7600
  IP Discovery artifact, then define an offline dispatch model only for
  records actually observed. Separately demonstrate a minimal x86_64
  IOKit service build using an authorized macOS SDK before attempting any
  driver integration. Do not add Navi33 matching to Navi48 personalities.
# 2026-10-21 — Pinned Navi33 IP dispatch specification

- **Objective:** Use public, pinned upstream Linux AMDGPU sources to describe
  the version-driven dispatch and conditional firmware paths relevant to
  Navi33 without fabricating physical RX 7600 records.
- **Sources reviewed:** Linux
  `d0c647a6aae2ce577372594a176c82733e1ed267` (commit dated 2026-03-11),
  including `discovery.h`, `amdgpu_drv.c`, `amdgpu_discovery.c`,
  `amdgpu_ucode.c`, GFX11/GMC11/IH6/PSP13/SMU13/SDMA6/VCN4/MES11 and display
  DM sources. Also reviewed the pinned pciids snapshot
  `22da7386ba818c037894f258248734770757b14f`. Exact symbols, file URLs,
  source conditions, and limits are recorded in
  `RX7600_NAVI33_IP_DISPATCH_SPEC.md`.
- **Finding:** Linux dispatches using per-IP versions parsed from discovery
  records. Its generic switch includes multiple GC 11.x, OSSSYS 6.x, MP0/MP1
  13.x, SDMA 6.x, UVD 4.x and DCE 3.2.x branches. These establish available
  Linux code paths, not the versions reported by `1002:7480`. The PCI ID
  database names a product group; it does not supply board-specific IP data.
- **Firmware boundary:** The specification records conditional request
  families and example names without asserting target selection. Exact board
  selection, firmware binary identity/provenance, blob-specific license, and
  macOS compatibility remain unresolved. No Linux code or firmware was
  copied, downloaded, or accessed.
- **Changes:** Added `RX7600_NAVI33_IP_DISPATCH_SPEC.md`; updated this
  append-only log and `RESUME.md`. No code changes. No physical GPU, PCI/MMIO,
  VRAM, firmware interface, EFI, boot, security setting, or system setting was
  accessed.
- **Validation:** Documentation-only; no parser or C++ tests were run because
  no implementation changed. Source URLs are pinned to full revisions; this
  work did not alter repository tests or fixtures.
- **Outcome / next milestone:** No trustworthy board-attributed RX 7600
  artifact was found in the local notes. Next, strictly parse an existing
  user-supplied artifact with documented provenance and correlate only its
  observed IP records to the pinned dispatch source. Do not probe hardware to
  fill the gap.

## 2026-10-21 — Phase 2 skeptical dispatch-spec review

- **Review scope:** Reviewed
  `RX7600_NAVI33_IP_DISPATCH_SPEC.md` without editing it. Re-fetched the exact
  pinned Linux `amdgpu_discovery.c`, `psp_v13_0_4.c`, `amdgpu_psp.c`, and
  `gfx_v11_0.c` source URLs at
  `d0c647a6aae2ce577372594a176c82733e1ed267`; cross-checked existing local
  pinned-source claim records and the repository's root `LICENSE`.
- **Correct:** Full commit hashes and pinned links are present. The spec
  repeatedly identifies all versions as conditional Linux source branches,
  explicitly not an inventory for `1002:7480`. The MP0 13.0.4 specialized
  path in the dispatch switch calls PSP TOC then TA initialization, consistent
  with the report's conditional example.
- **Needs citation:** Some switch rows link the full source without line
  anchors; IH 6.1.0-6.1.1 lacks a direct switch citation in the evidence
  table; PSP TA's shared helper needs its own lines in addition to TOC; and
  GFX component/RLC request conditions need precise function fragments.
- **Ambiguous:** Future evidence must classify firmware as log-observed
  request, loaded/running, source-inferred expected request, or unverified.
  AMD per-file permissive notices are not kernel GPL-only symbol policy,
  firmware blob license, or document license. The root MIT license and
  component-specific Navi48 license have distinct scopes; no legal conclusion
  was made.
- **Blocker:** No board-attributed evidence exists. Current host is Windows;
  no Linux hardware commands were attempted. RX 7600 presence is unknown,
  not absent; proceed only with an authorized Linux capture or existing
  provenance-documented board evidence. Phase 4 dispatch work is not
  evidence-justified.
- **Changes:** Added `RX7600_NAVI33_SPEC_REVIEW.md`; updated this log and
  `RESUME.md`. Dispatch spec, code, and tests were unchanged.
- **Validation:** Documentation-only. `git diff --check` and whitespace
  scans over `tools/ipdisc/*.md` are required at phase end; no C++ or parser
  tests were run.

## 2026-10-21 — Phase 3 board evidence capture blocked by Windows host

- **Environment check:** `Get-Command lspci,dmesg,amdgpu-discover,atomtree
  -ErrorAction SilentlyContinue` returned no command entries. This checkout
  is on Windows and no authorized Linux hardware-capture environment was
  available in this task.
- **Commands/paths not checked:** Did not run `lspci -nn | grep -i
  1002:7480` or `dmesg | grep -i amdgpu`; did not read
  `/sys/kernel/debug/dri/*/amdgpu_firmware_info` or
  `/sys/bus/pci/devices/0000:*/` because those Linux interfaces are
  unavailable here. Did not attempt WSL/VM/other improvised access. No target
  hardware was accessed, and no claim of hardware absence is made.
- **Evidence record:** Added `RX7600_NAVI33_EVIDENCE.md`. Device presence,
  board/subsystem/VBIOS identity, all IP Discovery values, and all
  board-specific firmware requirements remain **UNVERIFIED**. Firmware
  request category (a), log-observed request, is unverified/not observed;
  category (b), loaded/running, is unverified/not observed; category (c)
  contains only conditional generic Linux source expectations; category (d)
  target requirements is unverified. Missing logs are not evidence of no
  request or no firmware load.
- **Phase 4 gate:** Not met. No confirmed board-attributed row exists, so no
  golden tests or production dispatch changes were made. The spec remains
  unchanged.
- **Validation:** Documentation-only. `git diff --check` and a whitespace
  scan across all `tools/ipdisc/*.md` were run. The scan found pre-existing
  Markdown hard-break trailing spaces in seven older reports; these files
  were not edited. New evidence/review files and continuity updates have no
  trailing whitespace. No C++/parser tests or hardware commands were run.
