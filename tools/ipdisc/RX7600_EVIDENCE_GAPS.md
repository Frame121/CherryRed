# RX 7600 Offline Evidence Audit and Gaps

**Milestone:** M2.2 — offline evidence audit and implementation gate  
**Audit date:** 2026-10-20  
**Result:** No trustworthy, board-specific RX 7600 IP Discovery artifact was
found in the inspected repository or supplied with this task. Exact board IP
versions remain **UNRESOLVED**. This is an offline project-file audit; no
physical device was accessed.

## Scope and evidence labels

Inspected the recovery checkpoint, M2.0 research report, M2.1 IP/firmware map,
IP Discovery README, parser, test suite, fixture generator, parity notes, and
repository-local files matching IP Discovery, RX 7600, PCI ID, hardware-report,
binary, dump, and log indicators. The repository scan excluded only Git
internals; it did not search unrelated user directories. No separate user
artifact was supplied to this task.

- **VERIFIED** — directly observed in a local file, parser output, or pinned
  source.
- **INFERRED** — a bounded conclusion from the location, type, or surrounding
  documentation, but not direct proof of target hardware.
- **UNRESOLVED** — available material cannot establish the target-specific
  fact.

## Evidence reviewed

| Evidence | Relative path / source | Result |
|---|---|---|
| Current recovery checkpoint | `tools/ipdisc/RESUME.md` | **VERIFIED:** M2.1 says there is no target IP inventory; prior status and claims were rechecked, not assumed current. |
| M2.0 architecture report | `tools/ipdisc/RX7600_RESEARCH.md` | **VERIFIED:** no physical RX 7600 dump or independently measured board details are documented. |
| M2.1 IP/firmware map | `tools/ipdisc/RX7600_IP_FIRMWARE_MAP.md` | **VERIFIED:** Linux dispatch branches and their request paths are explicitly conditional; the map does not claim they were read from this board. |
| Parser and format notes | `tools/ipdisc/ipdiscovery.py`, `tools/ipdisc/cpp_parity.md`, `tools/ipdisc/README.md` | **VERIFIED:** offline parser accepts caller-supplied bytes and synthetic examples; parsing is not hardware identification or provenance verification. |
| Fixture generator and test suite | `tools/ipdisc/mkfixture.py`, `tools/ipdisc/test_ipdiscovery.py` | **VERIFIED:** fixtures are generated in memory by tests; generator docstring says none represents real hardware data. |
| Existing source-side IP Discovery implementation | `src/navi48-bringup/src/ipdiscovery.cpp`, `src/navi48-bringup/src/ipdiscovery.hpp`, `src/navi48-bringup/src/amd/amdgpu_discovery.cpp`, `src/navi48-bringup/src/amd/amdgpu_discovery.h` | These are parser/driver source files, not discovery binary captures or reports. Their Navi 48 path is not evidence about a Navi 33 board. |
| Binary, dump, and log inventory | Repository-local files, excluding `.git` internals | **VERIFIED:** only two `.bin` files were found; their paths identify shader test inputs, not IP Discovery data. No `.dat`, `.dump`, `.ipd`, `.ipdiscovery`, `.rom`, `.vbi`, or `.log` candidate was found. |
| PCI identity source | `pciutils/pciids` commit `22da7386ba818c037894f258248734770757b14f`, linked in M2.1 map | **VERIFIED:** `1002:7480` maps to a shared Navi 33 product group; this is not a subsystem ID or a board-specific capture. |
| Linux implementation source | `torvalds/linux` commit `d0c647a6aae2ce577372594a176c82733e1ed267`, linked in M2.1 map | **VERIFIED:** Linux reads IP versions from IP Discovery data and selects implementations based on those values. These source branches do not provide the values reported by the user's device. |

The Linux source revision pins the reference checked during research. It does
not prove which commit the earlier moving-`master` lookup used.

## Candidate artifacts and parser fixtures

No on-disk project file was identified as an RX 7600 IP Discovery dump or
hardware report.

| Relative path / fixture name | Type and size | Provenance / confidence | Parser and attribution result |
|---|---|---|---|
| `src/navi48-bringup/shaders/store_magic.bin` | Shader binary, 56 bytes | **INFERRED** non-IPD by the `shaders/` path and project shader build/test documentation; no hardware-report provenance. | Not parsed as IP Discovery. It does not identify a GPU or board. |
| `src/navi48-bringup/shaders/store_magic_hsa.bin` | Shader binary, 512 bytes | **INFERRED** non-IPD by the `shaders/` path and project shader build/test documentation; no hardware-report provenance. | Not parsed as IP Discovery. It does not identify a GPU or board. |
| `tools/ipdisc` in-memory test fixtures: `valid_minimal.bin`, `valid_multi_record.bin`, `valid_v2_header.bin`, `valid_v4_table.bin`, `table_overlaps_binary_header.bin`, `table_overlaps_table_list.bin`, `die_overlaps_ipds_header.bin`, `compatibility_truncated_record.bin`, `invalid_signature.bin`, `invalid_checksum.bin`, `invalid_table_checksum.bin`, `invalid_ipds_signature.bin`, `invalid_offset.bin`, `zero_ip_count.bin`, `excessive_ip_count.bin`, `excessive_base_count.bin`, `truncated_record.bin`, `unsupported_version.bin`, `unsupported_binary_version.bin`, `unsupported_64bit_bases.bin`, `multiple_dies.bin`, and `truncated_header.bin` | 22 generated synthetic IP Discovery-shaped fixtures, 512 bytes each; held in memory by `FixtureGenerator.generate_all()` during this audit. | **VERIFIED synthetic:** `mkfixture.py` explicitly says no fixture represents real hardware; fixture specs identify each as synthetic or malformed synthetic. Not captured from a GPU and not board-attributable. | Existing strict parser accepted the four valid fixtures and rejected the 18 intentionally malformed/unsupported fixtures with parse errors. Compatibility expectations are covered separately by the test suite. This demonstrates parser behavior only. |
| `tools/ipdisc/__pycache__/ipdiscovery.cpython-312.pyc` | Python bytecode cache, 15,707 bytes | **INFERRED** generated tooling artifact from its path/name; no GPU provenance. Preserved. | Not parsed; not IP Discovery data. |
| `tools/ipdisc/__pycache__/mkfixture.cpython-312.pyc` | Python bytecode cache, 14,646 bytes | **INFERRED** generated tooling artifact from its path/name; no GPU provenance. Preserved. | Not parsed; not IP Discovery data. |

The in-memory fixture checks were run with the existing `FixtureGenerator` and
strict parser. They wrote no fixture files. The parser's test suite separately
checks deterministic fixture bytes and manifests. No candidate hardware file
was passed to the parser because none was found.

## Facts established by pinned sources

- **VERIFIED:** `1002:7480` is associated in the pinned PCI ID database with a
  shared Navi 33 product group. It does not uniquely identify the desktop RX
  7600 board, subsystem, VBIOS, memory configuration, or display wiring.
- **VERIFIED:** Linux's pinned AMDGPU source decodes hardware IP versions from
  discovery records and dispatches IP implementations and firmware requests
  based on reported block versions.
- **VERIFIED:** The M2.1 map cites Linux branches including GC/GFX 11.x,
  SDMA 6.x, OSSSYS/IH 6.x, PSP/MP0 and SMU/MP1 13.0.4, UVD/VCN 4.0.3,
  DCE 3.2.0/3.2.1, and MES v11. These are Linux-supported code paths, not
  board measurements.
- **VERIFIED:** The current Cobalt64 Navi 48 personality matches `0x7550` and
  `0x7551`; it is not evidence that Navi 33 can use that path.
- **UNRESOLVED:** The user's supplied `gfx1102` label has not been
  independently confirmed by a target artifact.

## Board-specific evidence gaps

No trusted artifact establishes:

1. The binary-header and IP Discovery table versions for the physical card.
2. Die count and IDs; per-IP hardware IDs, instance numbers, harvest state,
   major/minor/revision, variant/subrevision, base-address counts and values;
   or the 32/64-bit base-address layout.
3. The actual board model, subsystem vendor/device IDs, VBIOS identity,
   memory arrangement, or display-output routing.
4. The exact per-IP firmware request set selected on that board.
5. Firmware binary versions, hashes, source/provenance, applicable license
   terms, and whether any candidate firmware could legally and technically be
   used by a macOS implementation.

PCI ID family mapping, user-provided GPU naming, parser-generated bytes, Linux
dispatch cases, and Navi 48 driver code cannot close these gaps.

## What parser success establishes

**VERIFIED scope:** A successful strict parse means the supplied byte buffer
conforms to the parser's supported structural and checksum rules and yields
records according to that parser. Synthetic fixture success validates the
parser against constructed examples.

Parser success does **not** establish that bytes came from a GPU, that the
artifact belongs to an RX 7600 or a particular board, that its fields are
authentic or complete, that Linux will select a specific IP path, that firmware
is present or licensed, or that Cobalt64/macOS supports the card. Provenance
and board identity must be verified independently.

## Acceptance criteria before considering driver-source changes

Driver-source changes remain out of scope until all applicable gates are met
and a separate implementation task is explicitly authorized:

1. **Artifact provenance:** A user-provided or otherwise trusted,
   read-only-collected discovery artifact has a documented acquisition method,
   date/tool version, original file hash, and custodian. Preserve the original
   unchanged; do not represent an online example or synthetic fixture as
   target data.
2. **Board attribution:** Independently documented card model and PCI vendor,
   device, subsystem vendor/device IDs, plus VBIOS identity where available,
   correlate to the artifact. Record ambiguity for shared device IDs rather
   than assuming a retail name identifies the board.
3. **Strict offline parse:** The existing strict parser accepts the artifact
   without weakening validation; record its SHA-256, parser version, output,
   discovered offsets, die/IP records and all warnings. Unsupported layouts
   remain unresolved rather than being coerced.
4. **Independent source cross-check:** For each observed IP version, verify
   the selected Linux dispatch path against the pinned source commit. Keep
   `VERIFIED` source behavior distinct from artifact-attributed hardware
   observations. Explain any mismatch.
5. **Firmware evidence:** Trace actual selected request paths and establish
   each binary's version/hash, provenance, redistribution/use license, and
   macOS loading/security constraints before any firmware integration is
   proposed. Do not infer blob availability from a filename.
6. **Implementation proposal:** Before code edits, name the milestone, exact
   affected files, isolated Navi 33 initialization boundary, failure behavior,
   offline tests/build checks, and rollback/recovery strategy. Explicitly
   exclude adding `1002:7480` to the Navi 48 personality or routing Navi 33
   through Navi 48 initialization.
7. **Separate target safety gate:** Any future hardware test needs a separately
   authorized task, isolated macOS installation, proven recovery path, and
   independent review of side effects. Nothing in this audit authorizes
   probing, GPU initialization, firmware access, or kext loading.

## Safe future evidence acquisition

Within this audit, no evidence was collected from hardware. In a separately
authorized future task, safe inputs would be an existing user-held artifact or
a read-only capture made in an isolated environment after the user explicitly
approves that activity and has a tested recovery plan. Preserve provenance and
hash the original; transfer only the minimal artifact and non-sensitive board
metadata needed for analysis. No hardware collection or firmware operation is
authorized by this report.

## Verification and outcome

From `tools/ipdisc/`, `python -B test_ipdiscovery.py` completed with 22 tests
passing, no failures, and no skips (`Ran 22 tests in 0.331s`; `OK`). A separate
in-memory audit generated 22 synthetic fixtures (512 bytes each); strict
parsing accepted the four valid fixtures and rejected malformed/unsupported
fixtures. No test or audit result is hardware evidence.

**M2.2 result:** No trusted board-specific RX 7600 IP Discovery evidence was
available in the inspected project. The evidence gap is explicit and remains
open; no physical GPU access, firmware access, driver changes, or support claim
was made.
