# RX 7600 Navi 33 architecture skeleton

**Status:** Offline research model only. This is not a macOS driver and does
not establish RX 7600 support.

## Scope and files

The isolated `src/navi33-research/` module provides:

- `Navi33DeviceIdentity` and a research-only architecture-selection result.
- Explicit unknown/unsupported capability states.
- IP block descriptors with optional versions, evidence source, and confidence.
- Firmware evidence states for identifier, provenance, license review, and
  availability.
- Structured validation errors and research-only `DriverStage` values.
- Synthetic C++ tests that cover identity, architecture separation, invalid
  enum rejection, incomplete evidence, firmware claim handling, and disabled
  initialization.

The `1002:7480` identity is accepted only by the Navi 33 research route. The
locally documented Navi 48 IDs are classified separately and return
`Navi48NoInitialization`; no initialization route exists in this module. A PCI
ID match does not authenticate a board or establish compatibility. Unknown IP
versions remain absent rather than receiving guessed values. Firmware
metadata passing validation would permit research review only, not loading or
redistribution.

## Phase 2 validation findings

Static review found and corrected these issues:

| Severity | Finding | Correction |
|---|---|---|
| Medium | An out-of-range `ArchitectureVersionState` value was not equal to `Unsupported` and could pass architecture selection. | Unknown enumerator values now return `InvalidArchitectureState`; regression coverage added. |
| Medium | `EvidenceValidation.complete` was true for a valid record with an unknown IP version, conflating structural validity with evidence completeness. | Split into `structurallyValid` and `claimsComplete`; unknown IP is valid but incomplete. |
| Low | `hasVerifiedFirmwareEvidence` implied the helper independently authenticated user-provided state flags. | Renamed to `firmwareEvidenceClaimsComplete`; messages/docs state that flags are claims and do not authenticate or authorize loading. |
| Low | The stage test asserted enum ordinals rather than useful behavior, and `CapabilityState::Supported` allowed this skeleton to encode a positive claim with no evidence binding. | Removed the ordinal assertion; the capability enum now contains only `Unknown` and `Unsupported`. No stage executor or positive support state is provided. |
| Low | The previous tests did not explicitly assert that a matching PCI ID with an unknown architecture family is rejected, or exercise the missing-label error. | Added direct negative assertions for both identity-validation boundaries. |

The test source now contains 19 assertion checks for these and existing
invariants. They are synthetic C++ source tests only; they were not compiled
or executed in this environment. Static inspection found no register
definitions, firmware filenames/requirements, initialization sequences, or
hardware-access APIs in the module. The model still cannot independently
authenticate a supplied evidence-source string or reviewer state.

The validation result uses `claimsComplete` rather than `evidenceComplete`:
populated enum fields are user-supplied claims, not authenticated evidence.

Phase 1 skeleton paths:

- `src/navi33-research/include/Navi33Research.hpp`
- `src/navi33-research/src/Navi33Research.cpp`
- `src/navi33-research/tests/test_Navi33Research.cpp`
- `src/navi33-research/README.md`
- `tools/ipdisc/RX7600_ARCHITECTURE_SKELETON.md`

Phase 2 validation updated the header, implementation, tests, module README,
this report, `tools/ipdisc/WORKLOG.md`, and `tools/ipdisc/RESUME.md`.

## Build and tests

The intended C++17 test command is documented in
[`../../src/navi33-research/README.md`](../../src/navi33-research/README.md).
The current Windows environment has no `clang++`, `g++`, `c++`, `cl`,
`make`, `mingw32-make`, `cmake`, `ninja`, or `zig` available, so the C++
test executable could not be built or run. The follow-up Phase 2.2 check
found WSL is not installed, common local VM/container launchers are
unavailable, and there is no repository-local CI workflow or configured
runner. No tool was installed and no remote service was accessed.

The existing parser suite was run from `tools/ipdisc/` after the code review:

```text
python -B test_ipdiscovery.py
```

The Phase 2.2 run exited 0: `Ran 22 tests in 0.359s`, `OK` (22 passed, 0
failed, 0 skipped). This verifies the parser suite only; it does not compile
or execute the C++ tests.

`git diff --check` completed without errors. Static review and an API-name
scan were performed; no C++ compilation, C++ test execution, hardware test, or
network access occurred. The smallest next step is to run the documented
commands from an already provisioned MSVC Developer Command Prompt or existing
Linux/macOS C++17 host, capturing compiler version and output.

## Limits and next task

No kernel extension, IOKit/Metal integration, hardware initialization,
register access, firmware loader, memory management, command submission,
display path, or macOS build target is implemented. No physical device,
firmware interface, PCI configuration space, MMIO, VRAM, or network was
accessed. The physical card's exact IP versions remain **UNRESOLVED**, and
functional driver work remains **NO-GO** under this scope.

The next smallest engineering task is to run the documented C++17 compile and
unit-test command in an environment with an already available compiler, then
fix only issues demonstrated by that build. This task does not authorize
hardware testing or macOS integration.
