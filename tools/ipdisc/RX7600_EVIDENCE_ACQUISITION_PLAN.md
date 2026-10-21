# RX 7600 / Navi 33 evidence acquisition plan

**Status:** Planning only; no evidence was acquired in this phase.

**Decision:** Project remains `PAUSED — WAITING FOR TRUSTWORTHY EVIDENCE`.
Physical RX 7600 IP versions remain **UNRESOLVED**. This plan does not
authorize network access, hardware access, firmware access, implementation,
or macOS support claims.

Related records: [evidence schema](RX7600_EVIDENCE_SCHEMA.md),
[source provenance verification](RX7600_PROVENANCE_VERIFICATION.md), and
[IP/firmware map](RX7600_IP_FIRMWARE_MAP.md).

## Current evidence boundary

The reviewed project reports say that no trustworthy, board-specific IP
Discovery artifact is present and that the cited Linux and PCI source objects
are absent from the current checkout. Those are reported project findings,
not newly repeated audits. The identifiers and cited upstream paths in the
existing map are useful leads, not independently verified source evidence.
Consult `RX7600_PROVENANCE_VERIFICATION.md` for the local-object limitations.

The synthetic `tools/ipdisc/` fixtures validate parser behavior only. A
`1002:7480` PCI identity or generic Navi 33/Linux support path cannot establish
the hardware inventory of the user's physical card. No target, PCI
configuration, firmware interface, or external source was accessed to create
this plan.

## Evidence acquisition order

| Order | Evidence/artifact required | Verification and provenance to retain | Decision it can support | Does not establish |
|---|---|---|---|---|
| 1. Board identity | Existing user-held documentation or an explicitly authorized, read-only target report identifying exact card make/model, vendor/device IDs, subsystem vendor/device IDs, and VBIOS identity/version where available. | Preserve original report; record custodian, acquisition date/tool and version, exact fields, SHA-256, and how the report was associated with the card. Correlate independent board identifiers; explain conflicts or missing fields. | Whether later evidence can be attributed to the intended physical board. | IP versions, firmware needs, or macOS compatibility from product name/PCI ID alone. |
| 2. IP Discovery artifact | Original binary IP Discovery data for the physical board, including the complete binary header, table list, IP Discovery table, die headers, all IP records/instances, and base-address representation as captured. | Keep original bytes immutable and hash them. Record capture tool/version, acquisition method, date, custodian, relation to board identity record, file size, and transfer history. Review offline with the existing strict parser; record parser revision, exact command/output, warnings, and unsupported features. Do not use compatibility mode to validate untrusted input. | A board-attributed inventory of the IP records actually present in the artifact, if provenance and format checks succeed. | Authenticity solely from successful parsing; absent/harvested blocks not represented; driver support. |
| 3. Source provenance | Authoritative upstream source trees for each implementation path needed to interpret observed IP records and firmware request behavior. | Obtain only in a separately authorized source-verification task. For every file record canonical repository, full commit hash (not branch name), exact file path/symbol/range, acquisition date/method, inspected-file SHA-256, and command/output. Confirm the commit object and source contents locally. Until then, all cited upstream claims remain `UNVERIFIED` in this checkout. | Which generic upstream code path handles a specific reported block version, and what source says about its dependency/dispatch behavior. | That the physical card reports that version unless linked to the board-attributed artifact. Linux behavior or firmware names do not transfer directly to macOS. |
| 4. IP interpretation | Parsed per-die/per-IP records and corresponding verified source behavior for every present block, instance, revision, variant/subrevision, harvest state, and base-address entry supported by the format. | Cross-check all decoded fields against the pinned format definition and the exact parser output. Track unknown, duplicate, missing, or unsupported records explicitly. Do not infer a generation from a product label. | Whether a sufficiently complete, attributable hardware IP inventory exists for research planning. | Register safety, initialization sequencing, memory topology beyond what the artifact encodes, or successful operation. |
| 5. Firmware dependency evidence | Conditional source request paths selected by the board-attributed IP record, plus exact candidate firmware metadata only if separately provided and authorized. | Verify request code at pinned source revision. For each candidate file, record exact name/version/hash/source, custodian, applicable blob-specific license and notices, redistribution/use review, and macOS loading/security unknowns. Inspect no blob unless authorized; never copy blobs into this repository in this phase. | Whether a firmware dependency is a candidate for a later, reviewed design. | Availability, right to redistribute, compatibility with a macOS loader, or that the user's board needs the file merely because Linux has a request path. |
| 6. macOS source and integration evidence | Version-specific macOS kernel/IOKit interfaces and build constraints, Cobalt64 component boundaries, memory/DMA ownership, interrupt lifecycle, power management, command submission, firmware trust/loading, display and acceleration architecture. | Local Cobalt64 source can establish only what this repository currently implements. Apple SDK/interface documentation and any other authoritative source are not locally verified here; verify them in a separately authorized source review and record exact SDK/version/docs revision and interface contracts. Build a requirement-to-source matrix with unsupported gaps explicit. | Whether a bounded, offline-testable macOS integration proposal can be specified. | A working device driver or hardware compatibility. |
| 7. Target behavior | A separately authorized test protocol and results for a specific physical board, macOS build, host, firmware set, display topology, and recovery setup. | Only after independent safety review and validated rollback. Record exact hardware/software identities, build hashes, logs, test cases, failures, and stop conditions. Do not conduct this test under the current plan. | Only the behaviors actually exercised on the named setup. | General RX 7600 support, other boards/macOS versions, or features not tested. |

## Upstream references to verify

Existing project documents cite the following leads. The provenance report
says these exact commit objects/source files are not present in the current
checkout. Treat all entries here as **UNVERIFIED until a separately authorized
source review inspects the exact objects**:

| Claimed project/revision | Existing cited paths or subject | Verification target |
|---|---|---|
| `torvalds/linux`, `d0c647a6aae2ce577372594a176c82733e1ed267` | `drivers/gpu/drm/amd/include/discovery.h`; cited AMDGPU PCI/discovery dispatch, version handling, memory, PSP/SMU, SDMA, VCN, MES, and display/DMUB paths listed in `RX7600_IP_FIRMWARE_MAP.md`. | Confirm full commit object, tree paths, source contents, and each narrow claim in the existing map. Keep source-generic behavior separate from the physical-card record. |
| `pciutils/pciids`, `22da7386ba818c037894f258248734770757b14f` | `pci.ids`, entry cited for `1002:7480`. | Confirm exact commit and entry. Treat it only as a naming/ID reference, not board-specific IP evidence. |
| Other projects or source citations | `RX7600_PROVENANCE_VERIFICATION.md` lists local Cobalt64 attribution notices for mac-amdgpu/RDNA4FB and a product-page citation. | Verify only if a proposed design relies on them; obtain exact revision and source/license provenance. The Cobalt64 Navi 48 port is not evidence for Navi 33. |

Do not substitute a current branch, matching filename, copied hash string, or
search-result excerpt for the full pinned source tree. If exact source
provenance cannot be obtained, retain `UNVERIFIED` and do not derive an
implementation requirement from it.

## Generic-source evidence versus physical-board evidence

Keep separate evidence records for:

1. **Generic source claim** — e.g. a pinned Linux function dispatches an IP
   version to an implementation or requests a firmware name. This can become
   `SOURCE_VERIFIED` only after the full revision and cited code are inspected.
2. **Board claim** — e.g. a specific GPU reports that IP version. This
   requires a provenance-documented artifact independently tied to the exact
   physical board and may be `BOARD_SPECIFIC` only after attribution review.
3. **Target test claim** — a behavior observed on the identified board under a
   recorded protocol. This may be `TARGET_TESTED` only for that precise test.

Joining a source claim and board claim requires an explicit traceable mapping
from each parsed IP record to a verified dispatch condition. Neither a shared
PCI ID nor a user-supplied architecture label performs that join. No one
status implies working macOS support.

## What source-only work can and cannot decide

| Question | Source-only answer possible? | Additional evidence needed |
|---|---|---|
| How a pinned Linux revision parses or dispatches a reported IP version | Yes, if exact source objects are obtained and inspected; currently `UNVERIFIED` locally. | None for the generic source claim; a board artifact is required to apply it to this card. |
| Which IP versions and instances the user's card reports | No. | Board-attributed IP Discovery artifact with provenance and strict offline parsing. |
| Exact firmware request conditions in a source revision | Yes, for conditional code paths after provenance verification. | Board-specific IP records to establish which path applies; exact firmware evidence to evaluate actual assets. |
| Firmware availability, provenance, license, or redistribution permission | Not from a request path or filename. | Exact candidate asset metadata and authoritative blob-specific terms; human legal/licensing review where needed. |
| Current Cobalt64 architecture and component boundaries | Partially, from local code. | Version-specific macOS interface/build documentation for integration feasibility. |
| Whether macOS can safely initialize this RX 7600 | No. | Complete source/evidence prerequisites, separate design review, and eventually authorized target testing with recovery. |
| Whether the complete driver works | No. | A separately approved implementation, reproducible build, and staged target tests; results remain limited to tested configuration. |

## Licensing and copying controls

Before copying any third-party source or firmware into the repository:

- Identify the exact source repository, revision, path, file hash, copyright
  notices, and license text accompanying the specific material.
- For firmware, obtain authoritative blob-specific terms and provenance for
  the exact asset. A Linux request name, download location, or project-wide
  license declaration is not enough to establish redistribution permission.
- Record required attribution, notices, source-offer or other obligations
  visible in the applicable terms. Ask for human legal review where terms are
  missing, ambiguous, or conflict.
- Keep source-only permissions separate from firmware redistribution rights
  and from permission to load or use a blob in macOS.
- Do not copy any firmware blob or third-party source as part of this phase.
  Mark unavailable license material `UNVERIFIED`; make no legal conclusion.

## Decision gates

Each gate is a stop/go decision for a *separate* future task. Passing one gate
does not auto-approve the next.

| Gate | Required evidence and validation | Unresolved risks | Stop conditions / decision |
|---|---|---|---|
| Continue documentation research | A meaningful new artifact or a narrowly stated source question; classify each record with `RX7600_EVIDENCE_SCHEMA.md`. Verify local content only, or obtain separate explicit authorization before any network source verification. | Repeated analysis of existing claims may add no evidence; source citations may remain unauthenticated. | **GO** only when there is a new decision-relevant input or an explicitly authorized bounded source audit. Otherwise pause; do not restate prior audits as new findings. |
| Propose isolated Navi 33 implementation | Board-attributed, strict-parsed full IP inventory; exact upstream commits/files for every relied-on block; explicit Navi 33/Navi 48 boundary and negative routing tests; firmware path and exact asset/provenance/license review; design and offline acceptance criteria; recovery/failure containment plan. | Unsupported blocks, ambiguous board attribution, missing firmware terms, mismatch between Linux and macOS, and architecture behavior remain possible. | **NO-GO** if any required IP/source mapping is `UNVERIFIED`, physical versions remain unresolved, firmware requirements/terms are guessed, or proposal relies on Navi 48 initialization. Separate explicit implementation authorization still required. |
| Start macOS build/integration work | Reviewed macOS version/SDK/kernel interfaces, build/sign/load strategy, component boundaries for memory/DMA, interrupts, power, command submission, firmware and display; host-side tests and a failure-safe integration boundary; upstream/local provenance matrix. | APIs may be unavailable or restricted; build success does not show runtime behavior; display loss or boot failure risk. | **NO-GO** while toolchain/build strategy or critical interface contract is unknown. A host-only build phase needs separate authorization and must not load a kext. |
| Conduct physical GPU test | All implementation gates plus exact test binary hash, target identity, isolated test install, external recovery mechanism, tested rollback, independent risk review, defined stop conditions, and separate explicit authorization for this exact hardware test. | System has a reported Ryzen 5 7500F with no iGPU; assume no CPU-display fallback. Kext/hardware side effects may hang, restart, or remove display. Firmware and board behavior may differ from expectations. | **NO-GO** without a separately validated recovery plan that does not rely on an unverified iGPU, and explicit user authorization. A PCI ID match alone never authorizes initialization or proves IP versions. |

## Recovery prerequisite

Before any future physical GPU experiment, document and test a recovery plan
appropriate to the actual host. The Ryzen 5 7500F is reported to have no iGPU;
do not assume integrated-graphics fallback. Identify an independently
available display/recovery path, boot-volume rollback, remote/alternate access
if applicable, and a procedure to disable/revert the test component without
relying on the RX 7600 output. This plan has not been created or validated by
this document. Until separately reviewed and authorized, do not probe, map,
initialize, load firmware for, or load a kernel extension onto the card.

## Current outcome and safest next step

This phase created a plan and schema, not evidence. Existing project reports
still say that board-specific IP Discovery data is absent, upstream Linux/PCI
source provenance is not independently verified from this checkout, firmware
terms are incomplete, and macOS integration/recovery are unresolved. The
physical card's IP versions remain **UNRESOLVED** and the project remains
paused.

The smallest meaningful next action is for the user to provide an existing,
provenance-documented IP Discovery artifact and board-identification metadata
for offline review, or separately authorize a bounded upstream source
verification task requiring network access. Neither action alone authorizes
implementation or hardware testing.
