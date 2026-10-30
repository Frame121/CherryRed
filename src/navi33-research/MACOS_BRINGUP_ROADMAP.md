# RX 7600 macOS bring-up roadmap

Date: 2026-10-23
Scope: Radeon RX 7600 / Navi 33 PCI ID 1002:7480
Status: research and host-side validation only; no working macOS driver yet.

## Current verified baseline

- A real RX 7600 was inspected on Linux and its IP Discovery artifact was captured.
- Portable Navi33 research tests pass on the Linux host.
- The read-only Navi33 survey source exists, but has not been compiled or loaded on macOS.
- The current host is Linux, not macOS. Apple SDK and MacKernelSDK are not present in this checkout.
- No code in the Navi33 research track initializes GPU registers, loads GPU firmware, creates a display engine, submits command buffers, or provides Metal acceleration.

## Milestone 0 — repeatable environment preflight

Run from the repository root on the intended macOS validation host:

```sh
sh src/navi33-research/tools/macos_preflight.sh
```

This is read-only. It reports toolchain prerequisites and deliberately does not install software or change security settings. A passing preflight is not a kext build.

## Milestone 1 — compile-only diagnostic

Use the portable-core procedure in `MACOS_VALIDATION_HANDOFF.md` to compile/link the user-space lifecycle tests with Apple Clang. Record macOS version, Xcode/CLT version, SDK path, architecture, compiler command, test output, and binary inspection output. This milestone is not kernel ABI validation.

## Milestone 2 — kernel extension compatibility gate

Before creating a Navi33 kext target:
1. Establish a legally usable, version-pinned MacKernelSDK or supported public SDK arrangement.
2. Prove the exact headers, kernel C++ ABI, and link libraries match the intended macOS version and architecture.
3. Remove unsupported standard-library/virtual-dispatch assumptions from any code intended for the kernel unless the chosen SDK explicitly supports them.
4. Build a minimal no-hardware kext target and inspect its symbols and metadata.
5. Keep the existing Navi48 target separate; never change its match IDs to include Navi33.

Do not treat a successful user-space executable or a Navi48 build as proof that Navi33 kernel code works.

## Milestone 3 — read-only PCI survey

Only after the kernel build gate passes, review the survey code against the exact SDK, then test on a recoverable macOS installation with a known-good boot entry. The survey must remain opt-in and read-only. Collect IOKit logs and compare PCI identity, BAR metadata, PCIe link, and ReBAR data against the existing Linux evidence. If the platform API semantics cannot be proven safe, do not load it.

## Milestone 4 — evidence-backed initialization design

No register writes are authorized by the current project state. Before implementing any hardware initialization, close all of these evidence gates:
- Resolve every IP Discovery HWID and version using source matching the installed Linux kernel or other authoritative source.
- Document exact IP block dependencies and initialization order for Navi 33, not Navi 48.
- Trace each required firmware blob to the exact upstream declaration and request callsite; document provenance and license constraints.
- Review the reset, power-management, VRAM/GART, interrupt, command processor, SDMA, display/DCN, and teardown dependencies.
- Define failure rollback and watchdog/recovery behavior.
- Add offline tests for each parser and state transition before hardware access.

If any dependency is unknown, keep initialization disabled. Linux firmware presence is not proof that firmware can be redistributed or used by a macOS driver.

## Milestone 5 — staged hardware validation

On dedicated test hardware with recoverable boot media, validate one subsystem at a time:
1. Device match and safe service lifecycle.
2. Resource discovery without register writes.
3. Firmware delivery and PSP/SMU startup only after documented prerequisites.
4. VRAM/GMC/GART and interrupt handling.
5. Command submission and synchronization.
6. Display-engine initialization and scanout.
7. Acceleration API integration and finally Metal workloads.

Each stage needs logs, reproducible steps, failure rollback, and a clear PASS/FAIL/SKIP result. Do not enable the next stage based only on compile success.

## Checkpoint — 2026-10-28 (Windows host)

This checkpoint records validation performed on the connected Windows development host. It is not macOS or GPU validation.

- Inspected the existing Navi33 research core, bring-up gate model, host tests, macOS preflight, and the RX7600 Linux evidence notes before editing.
- Extended `tests/test_Navi33BringupPlan.cpp` with regression cases for an unknown stage value, earliest-incomplete-stage blocking, and shuffled gate ordering.
- Rebuilt `navi33_bringup_plan_tests` with the existing Visual Studio 2022 Build Tools configuration: **PASS** (10 checks, 0 failures).
- Ran the configured Debug CTest suite: **PASS, 9/9 tests**.
- Ran `tools/ipdisc/test_ipdiscovery.py`: **PASS, 30 tests**.
- Ran `tools/ipdisc/test_scan_windows_driver.py`: **PASS, 7 tests**.
- The initial generic `ctest` command was not on `PATH`; tests were subsequently run through the repository's configured CMake executable and generated Visual Studio build.

### Still blocked / not claimed

- No macOS host, Apple SDK, or verified MacKernelSDK is available through this Windows session.
- No production Navi33 IOKit backend, GPU firmware startup, VRAM/GART manager, command submission, display engine, or Metal integration is present in this research module.
- No live RX 7600 test, macOS build, kext load, or Hardware Acceleration test was performed.
- No EFI files, live system configuration, or hardware state were modified.

## Current blocker

The current host has no macOS toolchain, and the project has no verified Navi33 kernel backend. Therefore the next external prerequisite is access to a suitable macOS validation host/toolchain. Meanwhile, Linux-side source tracing and offline tests can proceed, but they cannot establish that macOS boots with accelerated RX 7600 graphics.

## Completion criteria

The phrase “works on macOS” is reserved for reproducible target-hardware evidence: macOS boots reliably, display output works, GPU initialization and memory management survive stress, acceleration is demonstrated, and Metal workloads pass. OpenCore booting with the card present, a kext compiling, or a PCI match alone do not meet this bar.


## Checkpoint — 2026-10-29 (RedNavi RDNA 3 reference audit)

- Reviewed RedNavi README, architecture, compatibility layer, PSP and DisplayPort documentation, plus the probe and framebuffer source files.
- Added `docs/REDNAVI_NAVI33_PORT_AUDIT.md` with a component-by-component port assessment and explicit IP-version differences.
- Confirmed the reference reports a fixed 1080p60 DisplayPort framebuffer on RX 7800 XT / Navi 32, but no GFX11 acceleration, RADV or Metal. Its own compatibility notes report unresolved SMU mailbox responses as of 2026-10-29.
- Kept the RX 7600 bring-up kext read-only. No RedNavi hardware-write sequence was copied or enabled.
- Validation on the connected Windows host: Navi33 safety test PASS; firmware manifest test PASS (14 blobs); firmware fetch manifest test PASS (bash dry-run skipped because bash is unavailable); `git diff --check` PASS with only line-ending warnings.
- This checkpoint is source audit and host-side regression validation only. It is not a macOS kext build or RX 7600 hardware validation.

## Checkpoint — 2026-10-29 (Resizable BAR preflight correction)

- Corrected read-only ReBAR capability decoding in `src/navi33-bringup/Navi33Bringup.cpp`: BAR index, low supported-size bitmap (encodings 0–27), extended bitmap (28–43), selected size, and capability/control index consistency are now decoded separately.
- The reported current size is published only when its encoding is in range, the corresponding supported-size bit is set, and both BAR indices agree. No config writes or BAR mapping were added.
- Updated the static safety test to guard these fields and fail-closed checks.
- Validation: bring-up safety test PASS; firmware manifest test PASS (14 blobs); firmware fetch/manifest test PASS (bash dry-run skipped because bash is unavailable); `git diff --check` PASS (line-ending warnings only).
- This is a preflight correctness fix, not GPU initialization. No macOS SDK, `xcrun`, `make`, or `clang++` toolchain was found in the connected Windows environment, so the kext was not compiled. RedNavi's PSP/SMU/DP writes remain unported and disabled.


## Checkpoint — 2026-10-29 (nullarc / Phoenix-to-Navi33 port audit)

- Cloned `luuumi-dev/nullarc` to a temporary directory on the Windows host for read-only inspection; no upstream source files were copied into CherryRed.
- Reviewed upstream README, installation guide, bring-up source, RDNA 3 IP-block code, and license notices.
- Added `docs/NULLARC_NAVI33_PORT_AUDIT.md` with target differences, license gates, and a staged port plan.
- Confirmed nullarc's reported reference target is Radeon 780M / Phoenix-Hawk Point APU (gfx1103, GC 11.0.1, DCN 3.1.4) on one tested Firebat U6C with Tahoe 26.6.2. RX 7600 is Navi 33 / gfx1102 (PCI 1002:7480) and uses discrete VRAM/BAR topology; the APU's fixed UMA memory assumptions and hardcoded IP identity cannot be reused as-is.
- Identified candidate architectural areas to study: PSP firmware delivery, GPUVM, queue submission, display/acceleration layering. These are not yet ported or validated for RX 7600.
- Reviewed license boundaries: nullarc's bring-up code is identified upstream as MIT or GPL-2.0-only at recipient option; accelerator/plugin components derived from NullMoth are PolyForm Noncommercial 1.0.0. Any future source reuse must preserve notices and follow the specific file's license.
- No register writes were added or enabled; no macOS build, RX 7600 hardware initialization, scanout, acceleration, or Metal test was performed. No commit/push made.
- Next coding gate: create Navi 33-specific, host-testable data/state structures from the captured RX7600 IP-discovery evidence and firmware provenance, without importing Phoenix fixed addresses or enabling MMIO writes.


## Checkpoint — 2026-10-29 (Navi 33 target-profile preflight)

- Added `include/Navi33TargetProfile.hpp` and `src/Navi33TargetProfile.cpp`: a pure, offline validator for seven critical version observations transcribed from the RX 7600 capture (GC, SDMA0/1, UVD, MP0/1, DMU).
- The validator fails closed on null/empty input, missing required blocks, duplicate required blocks, or version mismatches. Its success message explicitly says hardware initialization remains disabled. It performs no I/O and no MMIO access.
- Added `tests/test_Navi33TargetProfile.cpp` with five scenarios: valid evidence, null input, missing block, version mismatch, and duplicate block. Registered the source and test in `CMakeLists.txt`.
- Build: Windows Visual Studio 2022 CMake Debug target `navi33_target_profile_tests` **PASS**.
- Full research suite: **10/10 CTest tests PASS**.
- Existing RX7600 safety test: PASS. Firmware manifest: PASS (14 blobs). Firmware fetch manifest: PASS (bash dry-run skipped because bash is unavailable). `git diff --check`: PASS with line-ending warnings only.
- Scope limit: this is an evidence-consistency preflight and is not yet wired to live IP Discovery, not a kext build, and not hardware initialization. The names for HWIDs 282 and 286 remain unresolved; this profile intentionally does not guess them.
- No commit or push made.


## Checkpoint — 2026-10-30 (RX 7600 IP Discovery parser integration)

- Added `Navi33IpDiscovery`, a bounded offline parser for AMD Discovery binary v1/v2 with IP Discovery table v3, the format present in the captured RX 7600 artifact.
- The parser verifies the declared binary range and checksum, table directory bounds, IPDS signature/size/checksum, die offsets/identity, record bounds, and the base-address count limit. It preserves raw numeric HWIDs and does not guess names for 282 or 286.
- Refactored the target-profile validator to use numeric HWID + instance pairs rather than string labels. Required blocks are GC (11), SDMA0 (42), SDMA1 (43), UVD (12), MP0 (255), MP1 (1), and DMU (271), with the versions documented in the capture.
- Added a CTest that parses the real `amdgpu_discovery.bin` (10 KiB file, 1,280-byte declared binary, binary v1.3, IPDS v3, one die, 39 IP records, 30 unique numeric HWIDs), checks all critical versions, verifies both unresolved IDs stay numeric, and exercises malformed-input/checksum/version/capacity failures.
- Added `IP_DISCOVERY_PARSER.md` documenting scope and interpretation limits.
- This parser remains in the host-testable research library. It is not linked to the IOKit kext and does not authorize BAR mapping, MMIO, firmware loading, register writes, GPU initialization, scanout, acceleration, or Metal.
- Next gates remain target-specific firmware/PSP/SMU dependency audit and a real macOS toolchain/backend. Do not use raw discovery base addresses as direct register offsets.


## Checkpoint — 2026-10-30 (firmware dependency and header audit)

- Added `FIRMWARE_DEPENDENCY_MAP.md` with source-referenced dependency paths for all 14 required RX 7600 blobs: PSP SoS/TA, SMU, GC PFP/ME/MEC/RLC, IMU, MES pipe selection/fallback, SDMA, VCN and DCN DMCUB.
- Confirmed from the retained Linux 7.2.11 source that MP0 13.0.7 chooses PSP v13.0 functions and the SoS + TA request path; MP1 13.0.7 selects the SMU v13.0 block; GC 11.0.2 selects GFX v11.0 and MES v11.0 blocks.
- Captured a key MES detail: scheduler pipe requests `*_mes_2.bin` (with a source-defined fallback to `*_mes.bin`); the other pipe requests `*_mes1.bin`. Do not load all MES blobs arbitrarily.
- Added `tests/test_navi33_firmware_source_trace.py` to check that each of the 14 manifest entries names a source file in the pinned local kernel-source snapshot that actually references that firmware filename.
- Added `tests/test_navi33_firmware_header_bounds.py` to validate common AMD firmware-header fields and payload bounds for all 14 local blobs, with negative cases for truncation, file-size mismatch, invalid header size, invalid payload bounds and wrong IP family. It deliberately does not claim CRC/authenticity or complete family-specific container validation.
- Expanded IP Discovery parser tests for non-zero aligned binary offset, invalid die offset, excessive base-address count, excessive die record count, and every strict truncation length below the declared 1,280-byte binary.
- Windows-side validation after these changes: full CTest suite passes 13/13; bring-up safety test, 14-blob manifest test, fetch/hash test, and 14-entry source-trace test pass. The fetch script notes that the bash-only dry-run check was skipped because bash is not installed.
- Environment blocker confirmed on this connected Windows host: `xcrun`, `xcodebuild`, Apple Clang and `MacKernelSDK` are unavailable. Therefore no macOS kext build or load test can be performed on this host. A separate macOS build host with the matching SDK and kernel headers/tooling is required.
- Hardware initialization remains deliberately disabled. No BAR mapping, MMIO writes, PSP/SMU boot, VRAM/GART, ring/command submission, display scanout, acceleration or Metal test has occurred.


## Checkpoint — 2026-10-30 (family container checks and runtime discovery gate)

- Extended firmware metadata validation from common headers to selected family-specific payload descriptors for all 14 required blobs. The validator now checks GFX code/data split, IMU IRAM/DRAM, MES code/data, SDMA context/control, RLC v2.3 section pairs, PSP packed descriptors, SMU PPTABLE entries and DMCUB instruction/constant length.
- Firmware header test now has 17 test cases, including deliberate corruptions of PSP descriptors, SMU PPTABLE payloads/counts, IMU DRAM offset, GFX data offset, MES data range, RLC section range, SDMA control range and DMCUB length. All 14 real local firmware files pass the structural checks.
- Added `HWID_MAPPING_RECHECK.md`: current upstream `soc15_hw_ip.h` names nearby IDs such as DMU 271, DIO 272, DAZ 274, MP0 255, ATU 294 and AIGC 295; IDs 282 and 286 still have no source-verified name. CherryRed keeps them numeric and does not use them for firmware or register-map selection.
- Added `IP_DISCOVERY_RUNTIME_ACQUISITION.md` describing the Linux TMR/IFWI/scratch-register/VRAM/ACPI data-acquisition route and the prerequisites for any later IOKit port. It is a design checklist only; no register reads, VRAM mapping or MMIO were added.
- Cross-checked the parser's IPDS v3 die-offset and record-field interpretation against upstream AMD Discovery definitions. Die offsets are measured from the discovery binary start; the v3 final byte contains little-endian sub-revision in the low nibble and variant in the high nibble. IPDS v4's possible 64-bit base-address form remains intentionally unsupported.
- Remaining hard blocker: the connected host is Windows and has no Apple SDK, `xcrun`, `xcodebuild`, Apple Clang or MacKernelSDK. A macOS build host and verified SDK/kernel headers are required for a real kext build.
- Still disabled: live IP Discovery acquisition, BAR/VRAM mapping, PSP/SMU boot, firmware execution, GPUVM/GART, rings, command submission, display scanout, acceleration and Metal.


## Checkpoint — 2026-10-30 (RX 7600 PCI identity recognition)

- Identified a recognition gate that was unnecessarily board-specific: although `Info.plist` matched PCI ID `1002:7480`, kext `probe()` also required subsystem `148c:2421`. That would reject RX 7600 boards with a different AIB/subsystem ID.
- Added shared, platform-neutral `src/navi33-bringup/Navi33PciMatch.hpp`. The function `isRx7600()` recognizes AMD vendor/device `1002:7480`; `matchesCapturedEvidenceBoard()` separately requires subsystem `148c:2421`.
- Updated the kext to use the shared matcher. Both `probe()` and `start()` now recognize the RX 7600 by vendor/device only. Subsystem identity is reported as provenance and is not used as the basic GPU recognition gate.
- Kext properties now explicitly identify `AMD Radeon RX 7600`, `Navi 33`, `gfx1102`, PCI ID `1002:7480`, subsystem identity, exact evidence-board match state, and that future hardware initialization is not authorized.
- Added C++ host test `navi33_pci_identity_tests` with compile-time and runtime cases for the captured board, a different RX 7600 subsystem, Navi 48 ID, wrong vendor and empty identity.
- Updated bring-up safety tests and `BUILDING.md` with the new recognition semantics and IORegistry inspection command.
- Windows verification: CMake Debug build passes; CTest 14/14; bring-up safety passes; firmware header bounds 17/17; firmware source trace passes; `git diff --check` passes.
- The driver still requires explicit opt-in (`navi33bringup=1`) and remains read-only. This is source/test verification, not a macOS runtime attachment test. The connected Windows host has no Apple SDK, xcrun, xcodebuild or MacKernelSDK, so no kext build/load or actual IORegistry output has been observed.
- Still disabled: BAR mapping, MMIO writes, live IP Discovery acquisition, firmware loading, GPU initialization, display and acceleration/Metal.
