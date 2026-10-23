# Cobalt64 six-step execution status — 2026-10-22

This report records what was verified during the current execution. Existing modified files and the user's in-progress work were preserved.

## 1. Repository and environment baseline — DONE

- Confirmed the target repository and recorded `git status --short` before editing.
- Confirmed Windows enumerates `AMD Radeon RX 7600`, PCI `1002:7480`, subsystem `148C:2421`, revision `CF`.
- Confirmed the repository already contains uncommitted changes; none were reset or discarded.

## 2. Parser security regression coverage — PROGRESS

- Extended `src/navi48-bringup/tests/ipdiscovery_bounds_test.cpp` with five groups: null/short input, failed-init state reset, malformed lazy record bounds, register-offset overflow preserving the caller output, and checksum rejection.
- Built with MSVC C++17 `/W4 /WX`; all 10 synthetic-data groups passed.
- Rebuilt and ran the same test with MSVC AddressSanitizer; all 10 groups passed with exit code 0.
- Python IP Discovery suite: 30/30 passed.
- CTest: 4/4 suites passed. `git diff --check` passed (Git emitted only existing line-ending conversion warnings).
- These tests are synthetic parser tests; they do not validate a real board dump.

## 3. Acquire IP Discovery from the physical RX 7600 — BLOCKED

- Windows confirms PCI identity only; no board-attributed IP Discovery dump or VBIOS artifact was found in the existing evidence record.
- Ubuntu runs under WSL2 (`6.18.54.1-microsoft-standard-WSL2`) and exposes `/dev/dxg`, but the `amdgpu` kernel module is absent. This is not equivalent to native Linux amdgpu access to the physical card.
- Do not invent IP versions, register bases, firmware rows, or die records. The next evidence step requires a native Linux boot/live environment with the card bound to amdgpu, or a provenance-documented capture supplied by the user.

## 4. Complete the Navi 33 hardware map — BLOCKED ON STEP 3

- Keep dispatch choices conditional until real board IP Discovery and firmware evidence is available.
- Existing evidence/specification documents already identify unverified rows; no board-specific values were fabricated during this run.

## 5. Driver core and integration — PARTIAL

- Rebuilt `navi33_research_tests`, `navi33_driver_core_tests`, and `ipdiscovery_bounds_tests` in Debug successfully.
- Existing lifecycle/core tests and routing-isolation test pass.
- Hardware-specific initialization, firmware, MMIO, memory management, command submission, and display integration are not demonstrated by these host tests and must not be represented as complete.

## 6. macOS build and hardware validation — BLOCKED

- The active machine is Windows; `xcrun`/Apple Clang are unavailable here.
- The checkout's existing handoff notes that MacKernelSDK headers/libraries and mandatory Navi48 firmware are absent. The current Navi48 kext personality targets `1002:7550`/`1002:7551`, not the RX 7600's `1002:7480`.
- No macOS kext build, RX 7600 initialization, display output, acceleration, or Metal validation is claimed.

## Next safe actions

1. Continue parser fuzzing and Python/C++ parity checks without weakening strict bounds.
2. Obtain a real, provenance-documented RX 7600 IP Discovery/firmware capture through an authorized native Linux path.
3. Update the Navi 33 hardware map only from that evidence, then implement one subsystem at a time.
4. Use a suitable authorized macOS host and the correct SDK/firmware prerequisites for compile/link and later hardware gates.
