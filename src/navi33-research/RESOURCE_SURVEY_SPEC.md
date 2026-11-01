# PCI Resource Survey — Offline Specification

## Scope

This specification defines a portable validation boundary for PCI resource metadata already collected by a trusted, read-only source. It is not a PCI enumerator, kernel extension, BAR mapper, or register-access implementation.

## Required fields

Each resource record contains an inclusive start and end address, resource kind (memory or I/O port), enabled state, and prefetchable state. The input contract must state the source platform and the exact device/function identity. Missing identity or source provenance prevents the record from being treated as board-attributed evidence.

## Validation rules

1. Accept no more than six PCI resource entries per function in this first bounded model.
2. Reject a nonzero count with a null input pointer.
3. Reject unknown resource kinds and resources not marked enabled.
4. Reject an end address smaller than its start address.
5. Detect length arithmetic overflow before deriving a byte count.
6. Reject overlapping ranges rather than guessing whether aliases are intentional.
7. Keep resource inventory, mapping permission, and register-access permission as separate concepts. A valid inventory must not imply that mapping or writes are authorized.
8. Record the original resource source and preserve the raw values; do not silently normalize addresses.

## RX 7600 evidence needed

Capture the six resource lines for PCI function `0000:03:00.0` from the host's read-only PCI/sysfs inventory and record the capture command, kernel version, timestamp, device ID `1002:7480`, and source file hash. Do not infer macOS BAR assignments from Linux resource values; compare them only after a macOS target or authoritative platform evidence is available.

## Acceptance tests

- Empty list and valid disjoint ranges accepted.
- Null pointer with nonzero count rejected.
- Seven entries rejected.
- Disabled, unknown-kind, and reversed ranges rejected.
- Full-width address range rejected if its length cannot be represented.
- Adjacent inclusive ranges accepted; intersecting ranges rejected.
- Validation result always keeps mapping and device-write permissions false.

## Status

Portable metadata validator implemented in `src/Navi33ResourceSurvey.cpp` with unit tests in `tests/test_Navi33ResourceSurvey.cpp`. The standalone g++ compile-and-run test passed on the Linux research host. CMake integration was added, but the full CMake/CTest suite was not run because `cmake` is unavailable on that host. This component validates caller-supplied metadata only; it does not collect live PCI resources or perform hardware access. No macOS build or runtime validation has been performed.