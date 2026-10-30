# RX 7600 IP Discovery artifact intake

**Purpose:** Safely prepare a user-provided IP Discovery artifact for offline
structural review using the existing parser. This document is a workflow, not
an artifact or evidence acquisition. No synthetic fixture is physical-board
evidence.

## Safety and evidence status

- Treat every supplied file as untrusted bytes. Do not execute, source, mount,
  load, or open it with a firmware or hardware tool. The parser reads the file
  as bytes; it does not execute it or access hardware.
- Do not obtain a new capture from a GPU for this workflow. No hardware,
  PCI configuration, MMIO, VRAM, firmware interface, kernel extension,
  EFI, boot argument, or system configuration access is authorized.
- Keep original evidence outside the source tree and preserve its bytes.
  Work only from a separate analysis copy.
- A successful strict parse establishes only that the supplied bytes satisfy
  this parser's supported structural and checksum rules. It does not establish
  origin, authenticity, completeness, board identity, firmware need, or
  compatibility.
- Use the statuses in `RX7600_EVIDENCE_SCHEMA.md`. Do not mark an artifact
  `BOARD_SPECIFIC` from a filename, PCI ID label, user assertion, or parser
  success alone. A genuine physical capture requires documented acquisition
  provenance and independent linkage to the identified board. Synthetic
  fixtures and upstream examples are never board-specific evidence.

## Parser input and supported layout

The existing command-line tool accepts a path to a raw binary byte file. It
does not accept a ZIP/archive, a text hex dump, a JSON rendering, or an
extracted table as a substitute for the original binary. The parser scans for
the binary signature on four-byte-aligned offsets, so a binary may be embedded
after a prefix; retain the full original file and do not crop or rewrite it.

Use strict mode (the CLI default) only. The currently inspected
`ipdiscovery.py` behavior is:

| Area | Strict-mode behavior |
|---|---|
| Input buffer | Requires at least 512 bytes. Scans four-byte boundaries for the binary signature. |
| Binary header | Accepts major versions 0, 1, or 2. Checks declared binary size and checksum. Version 1 expects all six fixed table entries; version 2 requires table count 1 through 6. |
| IP Discovery table | Reads the IP Discovery table entry, requires a nonzero offset and at least 80-byte table, rejects table overlap with the binary header/table list and bounds it within the declared binary. Requires table/header sizes to agree and validates the table checksum. |
| Die layout | Supports exactly one die. Rejects a die header overlapping the fixed 80-byte IP Discovery header or falling outside the table. |
| IP Discovery versions | Accepts versions 1 through 4. Version 4 with 64-bit base addresses is explicitly unsupported. |
| Records | Requires 1 through 256 IP records, validates each record and base array against the table boundary, and allows at most 16 base addresses per record. Base addresses are read as 32-bit values. |
| Reported values | Returns raw hardware ID, instance, major/minor/revision, and base values. Named IDs are limited to the `HwId` enum; other IDs display as `UNKNOWN_<id>` and remain numeric. |
| Integrity/provenance | Checksums are structural checks, not cryptographic authenticity. The parser does not verify capture source, board identity, signatures, or completeness. |

The binary header's size field is 16-bit, so a declared binary region cannot
exceed 65,535 bytes. The CLI enforces a **1 MiB maximum input size**. This
allows room for an aligned prefix and modest surrounding bytes while bounding
memory and signature-scan work. The limit is checked on the already-open
regular file before reading, during chunked reads (including one byte beyond
the cap to detect overflow), and after reading. Files that grow beyond the cap
are rejected; a size change during reading is rejected rather than accepted as
a stable artifact. The CLI also compares the open file's modification and
change timestamps before and after the read and rejects detected changes.
These metadata checks are best-effort: they cannot guarantee an immutable
snapshot on filesystems with unreliable timestamps or when timestamps are
deliberately restored. Oversize and detected changing-file errors exit with
status 2 and are written to stderr. The existing 512-byte minimum remains a
parser validation failure (status 1). No bytes are silently truncated. The
in-memory parser API has no file-size policy; apply the CLI limit for
untrusted-file intake.

Compatibility mode is intended to reproduce selected C++ parser behavior,
not validate untrusted inputs. It does not apply all strict version,
overlap, single-die, record-boundary, checksum, and base-count rules; its
record access is lazy and may return an unavailable record. Do not use
`--mode compatibility` for artifact intake or treat its output as evidence.
See `cpp_parity.md` and `parity_cases.json` for documented compatibility
expectations.

## Provenance record required for review

Supply the following with the unchanged original artifact. If a value cannot
be established, record `UNKNOWN` or `NOT_SUPPLIED` plus the reason; never
fill it from inference.

| Field | What to record |
|---|---|
| Source device | Exact board/card manufacturer and model if known; PCI vendor/device and subsystem vendor/device IDs if already available from an existing report; VBIOS identity/version if already documented. State how the report is tied to this artifact and board. Do not probe hardware to fill gaps under this phase. |
| Capture method | How the artifact was obtained, by whom, and whether it is an original capture, a copied file, a transformed/exported file, or an example. State any tool steps that could alter bytes. |
| Timestamp | Capture time with timezone if known; otherwise `UNKNOWN`. Distinguish capture time from file-copy or submission time. |
| Source tool/version | Exact capture/reporting tool name and version, or `NOT_APPLICABLE` for an upstream example; explain missing version information. |
| Original filename | Filename as received from the source, separate from any renamed analysis copy. |
| Size | Original byte length. Record whether the file is exactly the raw binary or contains a prefix/container/other bytes. |
| SHA-256 | SHA-256 of the original bytes before analysis. Record the command/tool and time. Recalculate after transfer and compare; a matching hash supports byte integrity across that transfer, not authenticity. |
| Custody and transformations | Who supplied it, when it was received, where the immutable original is stored, and every copy/rename/transfer/conversion. Prefer no conversions. |
| Artifact class | Explicitly label `PHYSICAL_CAPTURE`, `SYNTHETIC_FIXTURE`, `GENERIC_UPSTREAM_EXAMPLE`, or `UNKNOWN`. For the latter three, do not claim board-specific evidence. |

The artifact class is descriptive, not proof. A `PHYSICAL_CAPTURE` label must
be supported by provenance and a board-attribution review. If the source tool,
device identity, time, method, or custody is missing or uncertain, retain the
gap and classify the evidence as `UNVERIFIED` until independently resolved.

## Distinguishing artifact types

- **Synthetic fixture:** Generated by `mkfixture.py` or test code, or
  identified in a fixture manifest/test as synthetic. Existing test fixtures
  are synthetic parser inputs; passing them says nothing about a physical RX
  7600.
- **Generic upstream example:** Supplied from source code, documentation,
  test data, or a public example without capture provenance linking it to the
  user's card. It may illustrate a format but is not board evidence.
- **Physical-card capture candidate:** A binary accompanied by an acquisition
  method, timestamp, tool/version, original filename/hash, and independent
  board-identification evidence. Until linkage and custody are reviewed, call
  it a candidate and leave its status `UNVERIFIED`.

Names such as `rx7600.bin`, PCI ID `1002:7480`, a Navi 33 label, plausible
record values, or a valid checksum cannot distinguish these classes by
themselves. Never infer missing records or versions from generic Linux
dispatch branches.

## Preserve original and analyze a copy

1. Receive the artifact and provenance separately. Do not rename or edit the
   original. Store it in a designated evidence location outside the Git
   repository with access appropriate to its sensitivity.
2. Record the original filename and byte length. Calculate SHA-256 before any
   parser invocation, for example in PowerShell:

   ```powershell
   Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\evidence\original.bin'
   ```

3. Make a separate byte-for-byte analysis copy. Hash both original and copy
   with the same algorithm and confirm equality. Do not strip headers, pad,
   reorder, or “repair” bytes. If a transformed copy is ever needed for a
   separate diagnostic, preserve and label it separately; do not replace the
   original or treat the transformed file as the capture.
4. Record parser source revision (or source-file hashes), Python version,
   command, full stdout/stderr, exit code, and the analysis-copy SHA-256.
   The CLI rejects files over 1 MiB before reading them fully; analyze only
   in a resource-bounded offline environment.
5. Use strict mode explicitly and write no outputs over the input:

   ```powershell
   Set-Location '...\tools\ipdisc'
   python -B dump.py 'C:\evidence\analysis-copy.bin' --mode strict --json --pretty
   ```

   The CLI defaults to strict mode; spelling it out makes the record
   reproducible. JSON output is a parser result, not a provenance certificate.
6. Preserve the exact output and any error. Do not edit the artifact to make
   parsing succeed. If an unsupported or malformed structure is found, stop
   interpretation at that limitation and report it as such.

## Reporting parse outcomes

Record the exact error text, exit code, parser mode, input size/hash, and
whether the error occurred at a detected binary candidate. Separate parser
observations from provenance conclusions.

- **Unsupported binary/IP Discovery version:** Report the raw version value
  and error/offset. Mark layout interpretation `UNVERIFIED`/unsupported. Do
  not coerce the version or retry compatibility mode as validation.
- **Unsupported v4 64-bit bases:** Report the parser's explicit unsupported
  result. Do not reinterpret 64-bit values as 32-bit or modify bytes.
- **Malformed/checksum/overlap/bounds error:** Preserve exact error and
  offset. Describe which strict invariant failed; do not guess the intended
  offset, missing table, or record.
- **Truncated/incomplete input:** Report actual file length, declared binary
  length where safely available from output/error context, and exact
  truncation error. Do not zero-pad or append guessed bytes.
- **Successful strict parse:** Report the discovered binary/die offsets,
  supported die/IP counts, numeric IDs and versions, base-address values,
  parser revision, and hash. Distinguish `UNKNOWN_*` IDs. Do not infer absent
  blocks, board identity, firmware dependencies, or compatibility.
- **Compatibility-only success:** Not an acceptable intake result. Preserve
  it only as a diagnostic comparison if separately justified and label it
  non-validating; strict failure remains the result.

If the parser rejects the artifact or cannot represent a field, keep that
field unknown. A parser limitation is not evidence that the target lacks a
block or that a record has a particular value.

## What this review can establish

With adequate provenance, a later review may determine whether the byte
artifact is credibly attributable to a named board and whether it conforms
to the parser's supported format. A strict parse can extract the fields
actually present. Each conclusion must cite the artifact hash, source record
or parser output, and limitation.

This workflow cannot itself prove that an artifact is authentic, exhaustive,
fresh, or captured from a particular board; validate undocumented hardware
semantics; establish exact firmware assets, licensing, or macOS suitability;
or establish that Cobalt64/macOS supports the card. No `BOARD_SPECIFIC`
status is assigned until board attribution is independently reviewed, and
no `TARGET_TESTED` status is assigned without an authorized target test.

## Current intake state (updated 2026-10-30)

A 10,240-byte Linux AMDGPU debugfs capture candidate is now preserved at
`evidence/rx7600-amdgpu-discovery-2026-10-23.bin`. Its SHA-256 matches the
accompanying 2026-10-23 evidence manifest, and strict parsing succeeds with
39 IP records across one die. See
`evidence/RX7600_IP_DISCOVERY_ANALYSIS_2026-10-30.md` and the adjacent
hash-bound provenance sidecar.

The source manifest and Linux inventory logs are user-maintained and not
cryptographically authenticated. The capture remains **UNVERIFIED** for
independent provenance and board attribution; parser success alone does not
upgrade it to `BOARD_SPECIFIC`. The physical IP versions are now directly
observed in this capture, while exact source-to-binary mapping, VBIOS, firmware
compatibility, and macOS driver support remain unresolved. Existing generated
fixtures remain explicitly synthetic.
