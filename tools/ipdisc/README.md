# Offline IP Discovery Parser

These tools parse AMD IP Discovery binary data entirely offline. They do not
probe, initialize, or communicate with a GPU.

## Files

- `ipdiscovery.py` — parser with strict and C++ compatibility modes.
- `mkfixture.py` — deterministic synthetic fixture generator and SHA-256 manifest.
- `test_ipdiscovery.py` — standard-library `unittest` coverage.
- `dump.py` — offline command-line parser.
- `parity_cases.json` and `cpp_parity.md` — verified C++ behavior and intentional
  strict-mode differences.

## Run tests

From this directory:

```text
python -B test_ipdiscovery.py
```

Python's standard library is sufficient; there are no third-party dependencies.
The tests use generated synthetic data and temporary files only.

### Coverage and latest verified result

The offline suite covers valid v1/v2 binary headers and IP Discovery records,
strict-versus-compatibility behavior, structural overlaps, malformed and
truncated input, deterministic fixture generation, randomized malformed data,
bounded file intake, growth during reading, text/JSON CLI paths, and
provenance-sidecar validation with hash binding. The latest run on 2026-10-21
completed with 30 tests passing and no failures or skips
(`Ran 30 tests in 1.782s`; `OK`):

```text
Ran 30 tests in 1.782s
OK
```

This result is for the command and repository state observed during that
session; rerun the command to verify later changes. `WORKLOG.md` records the
chronology and `RESUME.md` is the current recovery checkpoint.

## Generate synthetic fixtures

```text
python -B mkfixture.py C:\temp\ipdisc-fixtures
```

The output includes binary fixtures and `manifest.json` with sizes and SHA-256
hashes. The fixtures are deterministic and are not real hardware dumps.

## Parse a file

Strict validation is the default:

```text
python -B dump.py INPUT_FILE
python -B dump.py INPUT_FILE --json --pretty
```

The CLI reads only regular files and enforces a **1 MiB maximum input size**
before parsing. It checks the already-open file's size, then reads at most the
limit plus one byte in bounded chunks; a file that grows past the limit is
rejected without truncation. File size is checked again after reading, and a
change in file size, modification time, or change time during the read is
rejected. Metadata checks reduce the chance of accepting a concurrently
modified file, but cannot guarantee an immutable snapshot if a filesystem
does not update timestamps reliably or they are deliberately restored. This
cap is intentionally larger than the format's 16-bit declared binary size
(65,535 bytes), allowing room for an aligned prefix and modest surrounding
bytes while bounding memory and signature-scan work. The cap is a CLI intake
policy; it does not change the in-memory parser API.

To inspect the recorded pre-hardening Cobalt64 C++ behavior instead:

```text
python -B dump.py INPUT_FILE --mode compatibility
```

Compatibility mode intentionally retains the earlier C++ limitations and must
not be treated as validation of untrusted files. The current C++ parser has
additional declared-table bounds checks; its CMake target includes synthetic
C++ bounds regression tests. See `cpp_parity.md` for the behavior distinction
and `parity_cases.json` for the case inventory.

### Attach provenance metadata to a parse report

An optional JSON sidecar can be included with a machine-readable report:

```text
python -B dump.py INPUT_FILE --mode strict --json --pretty --provenance provenance.json
```

The sidecar must include every required field from
`RX7600_EVIDENCE_SCHEMA.md` plus `artifact_class` (`PHYSICAL_CAPTURE`,
`SYNTHETIC_FIXTURE`, `GENERIC_UPSTREAM_EXAMPLE`, or `UNKNOWN`). `checksum`
must be `sha256:<64 hex digits>` or `NOT_COMPUTED`. When a SHA-256 is supplied,
the CLI checks it against the exact input bytes and rejects a mismatch. The
sidecar is read using the same regular-file, 1 MiB bounded-read and mutation
checks as the artifact; duplicate JSON keys, missing fields, invalid values,
and malformed JSON are rejected.

The resulting report includes input size, computed SHA-256, parse mode,
`STRUCTURE_ONLY` scope, and the caller-supplied metadata. It explicitly sets
`metadata_authenticated` to `false`: this tool validates structure and
byte-hash consistency only. It cannot authenticate a source, artifact class,
verification status, board attribution, or any claim in the sidecar.

### Reviewing a user-provided artifact

Use the [RX 7600 artifact intake procedure](RX7600_ARTIFACT_INTAKE.md) before
analyzing any supplied binary. It records the provenance fields, strict parser
limits, and preservation steps. Do not use compatibility mode for untrusted
input, and do not treat a synthetic fixture or parser success as board
evidence.

## Format evidence and limitations

The C++ comparison target is
`src/navi48-bringup/src/ipdiscovery.cpp` and its companion header. The format
definitions were checked against AMD's Linux
`drivers/gpu/drm/amd/include/discovery.h` on the upstream `torvalds/linux`
repository at commit
`d0c647a6aae2ce577372594a176c82733e1ed267`. This records the exact reference
inspected during M1.1 verification; it does not prove which revision was used
by the earlier lookup of the moving `master` branch. Strict mode supports
binary header versions 0–2 and IP Discovery versions 1–4 with 32-bit base
addresses. Version 4 with 64-bit bases is explicitly unsupported.

The parser checks structure, not provenance or hardware compatibility. Passing
a synthetic fixture or a real binary through this parser does not demonstrate
that a GPU is supported by Cobalt64 or macOS.

## RX 7600 research status

This parser milestone is not an RX 7600 driver milestone. The current bring-up
target is Navi 48 / gfx1201; its PCI match, firmware, memory, command, interrupt,
and display paths are not evidence of Navi 33 support. See
[`RX7600_RESEARCH.md`](RX7600_RESEARCH.md) for the source-backed architecture
comparison, target feature matrix, unresolved evidence, and implementation
gate. The M2.1
[`RX7600_IP_FIRMWARE_MAP.md`](RX7600_IP_FIRMWARE_MAP.md) records pinned Linux
dispatch and conditional firmware requests. It does not establish the exact IP
versions or firmware set reported by a physical RX 7600. No RX 7600 device
match or hardware access is enabled by this tool.
The M2.2 [offline evidence-gap report](RX7600_EVIDENCE_GAPS.md) audits
project-local artifacts and records what would be required to attribute an IP
Discovery dump to a specific board. It found no trusted board-specific dump.
The M2.3 [reproducibility audit](RX7600_REPRODUCIBILITY_AUDIT.md) records the
latest offline test, documentation, and Git-state verification.
The M2.4 [source and licensing audit](RX7600_SOURCE_LICENSE_AUDIT.md) records
which upstream references and license statements are available locally and
which source, firmware, and redistribution questions remain unverified.
The M2.5 [provenance verification](RX7600_PROVENANCE_VERIFICATION.md) checks
whether those pinned upstream references can be authenticated from this
repository's existing Git objects and source copies.
The M2.6 [next-step decision](RX7600_NEXT_STEP_DECISION.md) records the
research exit criteria and recommends pausing pending board-specific evidence.
The Phase 4/5 [upstream source verification](RX7600_UPSTREAM_SOURCE_VERIFICATION.md)
and [remaining source claims](RX7600_REMAINING_SOURCE_CLAIMS.md) record the
pinned Linux source checks and their generic-only limitations. The Phase 6
[artifact intake procedure](RX7600_ARTIFACT_INTAKE.md) prepares for offline
review of a genuine, provenance-documented capture; no such capture is
currently supplied.

## Navi 33 architecture research skeleton

The separately authorized `src/navi33-research/` module is a hardware-independent
C++17 model for identity and evidence review. Its tests use synthetic values,
and its only `1002:7480` route is `Navi33ResearchOnly`; Navi 48 is represented
as `Navi48NoInitialization`. The module cannot initialize hardware, load
firmware, or integrate with macOS. Physical IP versions and RX 7600 support
remain unresolved. See
[`RX7600_ARCHITECTURE_SKELETON.md`](RX7600_ARCHITECTURE_SKELETON.md) for scope,
test status, and the compiler blocker.

## Safety boundary

The parser scripts and C++ research model operate only on caller-supplied or
synthetic values. They do not access PCI configuration space, MMIO, VRAM,
firmware, kernel extensions, EFI, boot arguments, or system configuration. Do
not use a real hardware dump as a substitute for synthetic tests.

## Progress records

`WORKLOG.md` is an append-only history of tasks, commands, results, and Git
state. `RESUME.md` contains the latest concise checkpoint. A new session should
read `RESUME.md` first, then inspect the actual repository and Git state before
continuing.
