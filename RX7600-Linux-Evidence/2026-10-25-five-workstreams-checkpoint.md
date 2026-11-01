# CherryRed RX 7600 — five-workstream checkpoint

Date: 2026-10-25
Scope: safe local validation on the connected Windows host. This is a status record, not a claim of macOS support.

## 1. Navi 33 source and hardware evidence

- Existing Linux evidence artifact SHA-256: `4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`.
- Strict parser validation reports 39 IP records and a valid declared-binary checksum.
- Arch kernel source tag `v7.2.11-arch1`, commit `64a90897d3c539033f24105968e3513957e70819`, was inspected for the IP Discovery HWID name tables.
- HWID 282 (version 13.0.2) and HWID 286 (version 4.0.2) remain unknown in the inspected tables.
- Installed kernel binary provenance is not established: captured PKGBUILD is for 6.3.2.arch1, not installed 7.2.11.arch1-1. Firmware filename correlation is static evidence, not proof that every blob was requested or loaded.

## 2. macOS driver backend

- Source inspection confirms `Navi33Platform` is only an abstract callback boundary; there is no production macOS/IOKit backend for Navi 33 in this module.
- No backend was invented or wired into the existing Navi 48 kext. The Windows host does not provide a verified macOS kernel SDK/build environment for a valid target build.
- Next required work: on macOS, establish the exact supported SDK/kernel ABI and implement one narrowly scoped, fail-closed platform layer only after reviewing the corresponding Apple interfaces and hardware evidence.

## 3. Graphics / Metal integration

- The current bring-up plan models milestones for resources, firmware, memory, interrupts, command submission, display, acceleration and Metal, but it is only offline gate logic.
- No production command submission, GPU memory manager, display backend, Metal integration or hardware write path was added. Passing host tests does not imply any of these work.
- Next required work: prove each dependency in isolation and define explicit capability gates before attempting display or Metal integration.

## 4. macOS Tahoe validation

- Remote Desktop Commander currently reports the Windows device `Frame121` online; the separate device `10.151.102.86` is offline.
- Therefore no live Tahoe GPU/Metal test, system log capture, or real RX 7600 acceleration validation could be performed in this session.
- No EFI edits, kext installation/loading, firmware flashing, or hardware register writes were performed.

## 5. Tests and working-tree safety

- MSVC Debug build: PASS.
- Debug CTest: 9/9 PASS.
- MSVC Release build: PASS.
- Release CTest: 9/9 PASS.
- Firmware dependency audit Python tests: 3/3 PASS.
- IPDiscovery bounds security executable: PASS (10 synthetic-data test groups).
- `git diff --check`: PASS; only line-ending warnings were emitted.
- ASAN executable returned exit code 0 in the latest command but emitted no test summary; treat ASAN validation as inconclusive until its build/runtime and output are verified.
- Existing working-tree edits and untracked files were preserved. The existing `EFI-FramePC-Tahoe/` directory was not modified.

## Result

The research/evidence and portable host-test portions are in a better-verified state. Native macOS backend development and real Tahoe validation remain blocked by missing macOS host access and the absence of a verified Navi 33 backend. Do not claim RX 7600 hardware acceleration or Metal support until tested on the target system.
