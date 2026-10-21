# RX 7600 Research Exit Criteria and Next-Step Decision

**Milestone:** M2.6 — research exit criteria and phase decision  
**Decision date:** 2026-10-20  
**Scope:** Evidence review and decision only. No network, hardware, firmware,
or implementation activity was performed.

## Decision

**Recommended path: A — pause pending trustworthy, board-specific evidence.**
The research has reached a point where more documentation over the same
sources would repeat the existing M2.0–M2.5 work without reducing the central
uncertainty. The evidence is not sufficient to begin driver implementation.
Keep exact physical RX 7600 IP versions and macOS support **UNRESOLVED**.

Path B, independent upstream-source verification, could resolve a narrower
provenance gap, but requires a separate explicit authorization for network
access. It would not resolve board-specific IP records, firmware licensing, or
macOS integration. Path C is not recommended unless new evidence or a
concrete unanswered question makes it materially useful. Path D is **NO-GO**.

## Evidence basis and confidence labels

- **Verified locally:** M2.5 found the current repository is not shallow, but
  does not contain the Linux commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`, the PCI database commit
  `22da7386ba818c037894f258248734770757b14f`, the cited upstream source
  files, or `pci.ids`. Their object checks failed. The repository-local
  parser suite uses synthetic fixtures; its latest run before this decision
  passed 22 tests.
- **Verified locally as project state:** Existing Cobalt64 sources and
  notices describe a Navi 48 / gfx1201 implementation, with device matches
  `0x7550` / `0x7551`; they are not a Navi 33 implementation.
- **Historical project claims, not independently substantiated here:** M2.1
  reports that source files at the pinned Linux commit were inspected and
  documents conditional IP dispatch and firmware request paths. M2.5 cannot
  prove or disprove whether that earlier inspection occurred.
- **Unresolved:** No trustworthy board-attributed RX 7600 IP Discovery dump,
  hardware report, subsystem/VBIOS attribution, physical-card IP inventory,
  or actual firmware set is established by the reviewed project evidence.
- **Unresolved:** Exact firmware provenance and blob-specific license terms,
  redistribution rights, and macOS loading constraints.
- **Not established:** No RX 7600 macOS build, initialization, display,
  acceleration, or Metal result is established.

The user's RX 7600 / `1002:7480` / `gfx1102` description remains a supplied
target description, not a measured board-specific IP record. A PCI family
label, synthetic fixture, Linux switch case, or Navi 48 source is not a
substitute for one.

## Next-phase decision matrix

| Path | Evidence required | Available / missing | Risk and reversibility | Offline / network / hardware / implementation | Go/no-go condition |
|---|---|---|---|---|---|
| **A. Pause for board-specific evidence** | Existing artifact with documented source, date/tool version, original hash/custodian, and independent board attribution (model, PCI IDs including subsystem IDs, and VBIOS where available). | Available: prior repository audit reported no suitable artifact. Missing: authenticated target dump and board attribution. | Lowest risk. Waiting is fully reversible; no code or system state changes. | Can wait offline; no network, hardware, or implementation required. Later parsing of a user-supplied artifact can remain offline. | **GO as current recommendation** to pause. Resume evidence analysis only when a credible, board-attributed artifact is supplied. Otherwise remain paused. |
| **B. Verify upstream references authoritatively** | Exact source revisions and files inspected from authoritative repositories, with revision identity and source paths checked; retain citations and provenance record. | Available: hashes/URLs and source path claims in project docs. Missing: local Git objects, source copies, and checksums. | Moderate research/time cost; low technical side-effect risk if read-only. Results may still not answer physical-board mapping. | Not possible from current local evidence. Requires separately explicit network authorization; no hardware or implementation needed. | **NO-GO in this milestone** because network is prohibited. Later GO only after explicit authorization; still does not clear Path D by itself. |
| **C. More documentation-only work** | A concrete uncertainty that can be reduced with evidence already available, without restating prior reports. | M0–M2.5 already cover architecture, parser reproducibility, artifact gap, licensing caveats, and local provenance limits. No new evidence is available in this task. | Low direct risk but high risk of duplicating claims or overstating confidence; changes are reversible. | Offline, no hardware or implementation. Network is not needed only if new local evidence exists. | **NO-GO for open-ended repetition.** Reconsider only if a specific, decision-relevant local artifact or unanswered question is identified. |
| **D. Begin RX 7600 driver implementation** | All exit criteria below, plus a separately authorized implementation proposal with exact scope, tests, failure containment, and recovery plan. | Insufficient. Board-specific IP data, upstream verification, firmware provenance/terms, isolated macOS build strategy, and recovery readiness are missing. | High and potentially non-reversible on target: the existing kext can write hardware registers; failure could hang/restart the system or disrupt display. CPU reportedly lacks iGPU fallback. | Cannot be safely completed as an offline-only documentation step; would eventually involve implementation and likely target testing. | **NO-GO.** Do not modify driver, match `1002:7480`, or route Navi 33 through Navi 48 initialization. |

## Minimum exit criteria before a future implementation proposal

Every item is a prerequisite for considering a proposal, not a guarantee of
success:

1. **Upstream provenance:** Independently verify every source reference that
   will inform the design against an authoritative repository and exact
   commit/release. Record source path, revision, relevant symbols/ranges, and
   how the source was obtained. Clearly separate verified source behavior
   from historical citations.
2. **Board-specific hardware evidence:** Obtain an existing trusted IP
   Discovery artifact with provenance and hash, and independently attribute it
   to the actual RX 7600 board using its model, device/subsystem IDs, and
   VBIOS identity where available. Strictly parse it without weakening the
   parser. Unsupported or ambiguous records remain unresolved.
3. **IP-generation and initialization boundary:** Map each observed IP block,
   instance, revision, and dependency to verified source behavior. Specify an
   isolated Navi 33 path and explicit rejection/containment behavior. Do not
   add `1002:7480` to Navi 48 matches or initialization.
4. **Firmware provenance and terms:** For each actually selected firmware
   dependency, establish exact filename/version/hash, source provenance,
   authoritative blob-specific license/notice terms, and human-reviewed
   redistribution/use constraints. Document macOS loading and security
   unknowns. A Linux filename or request path alone is insufficient.
5. **macOS integration and build strategy:** Identify supported macOS
   versions, required SDK/toolchain and kernel interfaces, signing/loading
   assumptions, component boundaries, build/test targets, and a staged
   validation strategy. Establish which functions can be tested offline and
   which require hardware; do not claim Linux APIs or firmware paths transfer
   directly.
6. **Recovery and failure containment:** Before any hardware-dependent test,
   document an isolated installation, tested recovery/rollback path, external
   display or other recovery method, and safeguards against boot loops or
   lost display. The reported CPU has no integrated GPU, so assume no
   CPU-graphics fallback unless verified otherwise. Hardware testing requires
   separate explicit authorization.
7. **Authorized implementation proposal:** Only after the preceding gates,
   propose the exact files, measurable acceptance criteria, tests, review
   checkpoints, and rollback plan in a separate task. Meeting these gates does
   **not** guarantee a working, stable, or supported driver.

## Safest continuation

Pause implementation research until the user supplies trustworthy
board-specific evidence, or until they separately authorize a bounded
upstream verification task with network access. If no board-specific artifact
exists, keep the physical card's IP inventory unresolved. No milestone here
authorizes GPU probing, firmware access, driver changes, or claims of macOS
support.
