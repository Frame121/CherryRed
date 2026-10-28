# RX 7600 PCI Resource Evidence (Linux, read-only)

## Provenance
- Capture timestamp: 2026-10-23T20:18:26+07:00
- PCI BDF: 0000:03:00.0
- Vendor/device: 1002:7480
- Subsystem vendor/device: 148c:2421
- Kernel: Linux 7.2.11-arch1-1
- Source: /sys/bus/pci/devices/0000:03:00.0/resource
- SHA-256 of source file at capture: a20ba6e9c36a2fc39f3bcd093e5c9c24e55e373ba3bb6135632eef69d127becd
- Capture method: read-only `cat` of PCI sysfs identity and resource metadata; no sysfs writes, BAR mapping, or register access.

## Raw non-empty resource entries
The Linux resource file has 13 entries, including empty slots. The following entries preserve the original start, inclusive end, and flags exactly; entry numbers are zero-based.

| Entry | Start | End (inclusive) | Flags |
|---:|---|---|---|
| 0 | 0x000000fa00000000 | 0x000000fbffffffff | 0x000000000014220c |
| 2 | 0x000000fc00000000 | 0x000000fc0fffffff | 0x000000000014220c |
| 4 | 0x000000000000f000 | 0x000000000000f0ff | 0x0000000000040101 |
| 5 | 0x00000000fcb00000 | 0x00000000fcbfffff | 0x0000000000040200 |
| 6 | 0x00000000fcc00000 | 0x00000000fcc1ffff | 0x0000000000046200 |

Empty slots 1 and 3 and slots 7–12 had start/end/flags all zero and are omitted from this table. The raw flags are retained rather than normalized here.

## Interpretation limits
- This is Linux resource metadata for this PCI function, not a macOS BAR layout.
- This record does not establish that any range is safe to map, nor that registers are safe to access.
- The portable validator accepts at most six supplied non-empty ranges; these five non-empty entries fit that bound.
- A future adapter must decode Linux resource flags explicitly and preserve the raw values/source. It must not infer VRAM size from a BAR length.
- This evidence does not establish macOS driver compatibility or GPU initialization.

## Test status
Standalone strict C++17 compile-and-run tests passed for:
- Navi33DriverCore lifecycle tests: 32 passed, 0 failed.
- Navi33BringupPlan tests: 7 passed, 0 failed.
- Navi33ResourceSurvey tests: passed (assert-based; no failure output).
CMake/CTest integration has been edited but was not run because `cmake` is unavailable on the capture host.
