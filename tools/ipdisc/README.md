# Offline IP Discovery Parser

These tools parse AMD IP Discovery binary data entirely offline. They do not
probe, initialize, or communicate with a GPU.

## Files

- `ipdiscovery.py` — parser with strict and C++ compatibility modes.
- `mkfixture.py` — deterministic synthetic fixture generator and SHA-256 manifest.
- `test_ipdiscovery.py` — standard-library `unittest` coverage.
- `dump.py` — offline command-line parser.
- `collect_linux_ip_inventory.py` — read-only collector for Linux AMDGPU decoded IP Discovery sysfs attributes.
- `test_collect_linux_ip_inventory.py` — synthetic sysfs fixture tests for the collector.
- `parity_cases.json` and `cpp_parity.md` — verified C++ behavior and intentional
  strict-mode differences.

## Run tests

From this directory:

```text
python -B test_ipdiscovery.py
python -B test_collect_linux_ip_inventory.py
```

Python's standard library is sufficient; there are no third-party dependencies.
The tests use generated synthetic data, synthetic sysfs trees, and temporary files only. They do not access a GPU.

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

## Capture Linux AMDGPU IP inventory

**Linux-only:** run this from a Linux boot/session with the existing `amdgpu` driver bound to the RX 7600. It cannot run in macOS Terminal because macOS has no `/sys/bus/pci/devices`; the macOS diagnostic records PCI identity and Metal visibility only.

From a CherryRed checkout available in Linux, collect the decoded IP records without probing the GPU directly:

```text
python3 -B tools/ipdisc/collect_linux_ip_inventory.py --output ~/rx7600-ipd-snapshot
```

If more than one matching `1002:7480` function is present, pass its BDF with `--bdf`, for example `0000:01:00.0`. Choose a new output directory on storage with enough free space. The tool refuses to overwrite an existing output directory and refuses to write under sysfs. It records PCI identity, kernel release, bound driver, the decoded `ip_discovery` sysfs attributes, and per-file SHA-256 hashes. It reads only those sysfs attributes and does not access BARs, MMIO, VRAM, firmware, or GPU registers.

This is a Linux-driver-decoded sysfs snapshot, **not** the original binary IP Discovery artifact. It cannot authenticate the retail board model or prove macOS compatibility. Keep the output unchanged and provide the card make/model and VBIOS identity separately if known. Upstream Linux creates the `ip_discovery` sysfs hierarchy beneath the PCI device and exports per-IP fields; see the [pinned AMDGPU source](https://linux.googlesource.com/linux/kernel/git/torvalds/linux/+/94426e4201fbb1c5ea4a697eb62a8b7cd7dfccbf/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c).

The collector can be tested on any host using synthetic sysfs directories:

```text
python3 -B tools/ipdisc/test_collect_linux_ip_inventory.py
```

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
review of genuine captures. A raw RX 7600 capture is now preserved at
`evidence/rx7600-amdgpu-discovery-2026-10-23.bin`; its provenance remains
UNVERIFIED pending independent authentication. A 2026-10-24 macOS [PCI identity observation](evidence/macos_pci_identity_2026-10-24.md)
records the device and subsystem IDs reported by the current boot, but it does
not include the board IP Discovery table or verify macOS acceleration.

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


## GFX11 firmware evidence verifier

`verify_gfx11_firmware_manifest.py` checks an evidence bundle without
accessing the GPU. It validates the target PCI/subsystem identity, requires
VBIOS and physical IP Discovery evidence files, checks file containment and
size limits, and verifies SHA-256 and byte lengths for each listed firmware
blob. It also enforces the expected firmware-role set and filename prefixes
derived from the manifest's IP versions.

Example invocation after preparing a reviewed manifest:

```sh
python3 -B tools/ipdisc/verify_gfx11_firmware_manifest.py /path/to/evidence/manifest.json
```

The verifier does not discover firmware, authenticate evidence provenance,
verify a licence, load firmware, or authorize initialization. Human-reviewed
`board_attributed` and `license_approved` fields remain claims, not
cryptographic proof. Tests use synthetic temporary files only:

```sh
python3 -B tools/ipdisc/test_verify_gfx11_firmware_manifest.py
```

Latest test run: 9 verifier tests, 30 IP Discovery parser tests, and 8 Linux
sysfs collector tests passed. No physical RX 7600 firmware manifest was
available, so no real firmware bundle has been validated.


## Exact RX 7600 IP Discovery capture (Linux only)

A read-only capture helper is available at
`tools/ipdisc/capture_rx7600_ip_discovery.sh`. Run it on a Linux host where
this exact PCI/subsystem function is present and bound to the upstream
`amdgpu` driver, with debugfs already mounted:

```sh
sudo tools/ipdisc/capture_rx7600_ip_discovery.sh /absolute/path/amdgpu_discovery.bin
```

The helper refuses macOS, non-root execution, a mismatched/ambiguous PCI
identity, a non-amdgpu binding, an unmounted debugfs, an ambiguous debugfs
blob, an existing output file, or an out-of-range blob size. It reads only the
matching `amdgpu_discovery` debugfs file, records SHA-256 and capture metadata,
and runs the existing strict offline parser. It does not mount debugfs, access
BAR/MMIO/VRAM, read VBIOS, load firmware, reset the GPU, or change driver state.
A strict parse is structural validation, not independent proof of provenance
or board authenticity. Preserve the raw binary and sidecars unchanged.

The connected Mac cannot produce this artifact: it runs Darwin, has no Linux
`/sys/kernel/debug/dri` AMDGPU debugfs, and does not have `lspci`. Do not create
a synthetic binary and label it as a physical RX 7600 capture.


## Recovered physical RX 7600 capture (2026-10-30)

The previously captured Linux artifact was found on the mounted external
`GameAndFile` volume and copied byte-for-byte into
`evidence/rx7600-amdgpu-discovery-2026-10-23.bin`. Its SHA-256 matches the
preserved evidence manifest. Strict parsing succeeds with 39 records across
one die. See
`evidence/RX7600_IP_DISCOVERY_ANALYSIS_2026-10-30.md` for parsed versions,
source-attribution limits, and reproducible commands. The hash-bound sidecar
is adjacent to the binary. Provenance is recorded as UNVERIFIED because the
capture manifest and system logs are user-maintained rather than
cryptographically authenticated. This is a physical-capture candidate, not
proof of macOS compatibility.


## Linux-source-correlated HWID names

`ipdiscovery.py` now includes the 28 numeric HWID names cross-referenced in the preserved RX 7600 evidence package against its locally available Linux `soc15_hw_ip.h` / `amdgpu_discovery.c` source snapshot. This improves readable parse output but does not authenticate that snapshot as the exact source used to build the capture kernel. HWIDs 282 and 286 intentionally remain unknown; no aliases are guessed. The parser still reports raw numeric IDs and versions, and this naming table does not change structural validation or imply that any IP block is initialized or usable on macOS. Regression coverage checks all newly added enum values and preserves the two unresolved IDs.


## Board firmware manifest cross-check (2026-10-30)

`verify_board_firmware_manifest.py` checks internal consistency between the
board-attributed evidence files this repository holds: the recovered firmware
manifest (`evidence/rx7600-navi33-firmware-manifest-cobalt64.json`), the strict
parse sidecar of the captured IP Discovery binary, and the VBIOS identity
record (`evidence/rx7600-vbios-identity-2026-10-23.json`). It verifies
filename-prefix/version agreement per IP family, required firmware-role
coverage, explicit unresolved-HWID bookkeeping (282/286 must stay UNKNOWN),
the exact board PCI identity in the VBIOS record, and — with `--source-log` —
a SHA-256 binding to the source boot journal. It is a STRUCTURE_ONLY tool: it
authenticates nothing and proves no blob is loadable or licensed on macOS.

```sh
python3 -B tools/ipdisc/verify_board_firmware_manifest.py \
  --manifest tools/ipdisc/evidence/rx7600-navi33-firmware-manifest-cobalt64.json \
  --parsed tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin.parsed.json \
  --vbios tools/ipdisc/evidence/rx7600-vbios-identity-2026-10-23.json
python3 -B tools/ipdisc/test_board_firmware_manifest.py
```

Evidence intake details: `evidence/RX7600_BOARD_FIRMWARE_EVIDENCE_2026-10-30.md`.
