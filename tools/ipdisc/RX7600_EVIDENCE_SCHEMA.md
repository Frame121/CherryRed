# RX 7600 evidence record schema

**Purpose:** Preserve the provenance and limits of each claim about an RX 7600
or a possible macOS implementation. This schema defines recordkeeping only;
creating a record does not acquire or validate evidence.

See the [evidence acquisition plan](RX7600_EVIDENCE_ACQUISITION_PLAN.md) for
the current evidence gaps and decision gates.

## Status vocabulary

| Status | Meaning |
|---|---|
| `UNVERIFIED` | Claim or artifact is reported but its source, content, or applicability has not been independently checked. Use this by default. |
| `SOURCE_VERIFIED` | A source file/section was inspected at the recorded repository revision and supports the narrowly stated source claim. Does not attribute the claim to a physical board. |
| `BOARD_SPECIFIC` | A provenance-documented artifact has been independently attributed to the physical RX 7600 board. Record the attribution method and its limits. Does not prove macOS support. |
| `TARGET_TESTED` | A named, board-attributed artifact or behavior was tested on the identified physical target under a recorded test protocol. Scope the claim to that test. Does not by itself prove a functioning or supported macOS driver. |

`SOURCE_VERIFIED` does **not** imply `BOARD_SPECIFIC`; neither status implies
working macOS support. A record can also be marked `BLOCKED` when the intended
verification cannot safely or currently be performed. Do not promote a status
without recording the supporting artifact and method.

## Record fields

Each record should include all fields below. If a field does not apply, use
`NOT_APPLICABLE` with an explanation rather than silently omitting it.

| Field | Required content |
|---|---|
| `evidence_id` | Stable, unique identifier, for example `RX7600-IPD-001`. |
| `claim` | One falsifiable, narrowly scoped statement. Separate generic source behavior from claims about a physical board. |
| `artifact_path` | Repository-relative path or `NOT_SUPPLIED`. Keep originals immutable; do not commit firmware or sensitive dumps without separate review. |
| `source_repository` | Canonical project/repository identity, or `USER_SUPPLIED` / `LOCAL_PROJECT`. A URL alone is not proof of inspection. |
| `source_revision` | Full commit hash, release/tag plus resolved commit, or `UNVERIFIED`. Never use a moving branch name as a pin. |
| `source_file_or_section` | Exact source path, symbol/section, line range if stable, or artifact section/record identifiers. |
| `checksum` | Algorithm and digest of the exact local artifact (normally SHA-256), or `NOT_COMPUTED`. For source files, hash the inspected bytes when provenance needs to be retained. |
| `applies_to` | One or more explicit scopes: `GENERIC_LINUX_PATH`, `RX7600_FAMILY`, `PHYSICAL_BOARD:<identity>`, `COBALT64_LOCAL_SOURCE`, `MACOS_DESIGN`, or `TEST_ENVIRONMENT:<identity>`. |
| `verification_method` | Reproducible command, parser/version, independent attribution procedure, source-inspection method, or test protocol. |
| `verification_status` | One status from the vocabulary above, with status date and reviewer/custodian recorded in the entry. |
| `limitations` | What the record does not prove, ambiguities, assumptions, and remaining prerequisites. |

Recommended additional fields: `observed_at`, `recorded_at`, `custodian`,
`tool_and_version`, `board_model`, `pci_vendor_device`, `subsystem_vendor_device`,
`vbiOS_identity`, `parent_evidence_ids`, `license_reference`, and
`reviewer_notes`. Avoid recording serial numbers or other personal data unless
needed and explicitly authorized.

## Empty record template

```text
evidence_id: RX7600-<AREA>-001
claim: <one narrow, falsifiable claim>
artifact_path: NOT_SUPPLIED
source_repository: UNVERIFIED
source_revision: UNVERIFIED
source_file_or_section: UNVERIFIED
checksum: NOT_COMPUTED
applies_to: <scope; do not infer physical-board applicability>
verification_method: UNVERIFIED
verification_status: UNVERIFIED
limitations: <what this cannot establish>
```

## Evidence status examples for this project

These examples describe the current documentation state, not new verification:

| Subject | Appropriate status now | Reason |
|---|---|---|
| Linux IP/version dispatch described by `RX7600_IP_FIRMWARE_MAP.md` | `UNVERIFIED` locally; historically documented | `RX7600_PROVENANCE_VERIFICATION.md` reports the cited upstream commit object and source copies are absent from this checkout. |
| Linux commit `d0c647a6aae2ce577372594a176c82733e1ed267` | `UNVERIFIED` locally | Commit text in a document is not source-object evidence. |
| PCI database commit `22da7386ba818c037894f258248734770757b14f` | `UNVERIFIED` locally | Commit object and `pci.ids` copy are reported absent from this checkout. |
| Synthetic parser fixtures and tests | Local parser test evidence only; never `BOARD_SPECIFIC` | Constructed fixtures do not originate from the physical card. |
| Physical RX 7600 IP inventory | `UNVERIFIED` / unresolved | No board-attributed IP Discovery artifact is available in the reviewed project artifacts. |
| Cobalt64 Navi 48 implementation | `SOURCE_VERIFIED` only for its inspected local source, where exact files are cited | It is not evidence for Navi 33 or the user's board. |
| RX 7600 macOS support | Not established | No target test or complete macOS integration exists. |

Do not use a status label as a confidence score or as a substitute for human
review of provenance, licensing, safety, and applicability.
