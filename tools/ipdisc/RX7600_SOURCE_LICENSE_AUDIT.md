# RX 7600 Upstream Source and Licensing Audit

**Milestone:** M2.4 — source provenance and licensing audit  
**Audit date:** 2026-10-20  
**Scope:** Local repository and documentation inspection only. No source
repository was downloaded or fetched for this audit. No firmware binary or
physical device was accessed.

## Actual Git state

At the pre-edit check:

- Branch: `main`
- HEAD: `d1f7bad8408a0a2c016b067c477422975a59de44`
- Upstream: `origin/main` at the same commit; merge base is also the same
  commit; divergence `0 0`.
- No staged or tracked changes.
- Fifteen untracked paths, all under `tools/ipdisc/`, including the existing
  Python cache files. No paths outside that directory were reported.

These are observed repository facts. The source/license conclusions below are
restricted to local evidence and citations already recorded in project
documents; prior research claims have not been treated as fresh upstream
verification.

## Scope and evidence grades

The audit read `tools/ipdisc/RESUME.md`, `WORKLOG.md`, `README.md`,
`RX7600_RESEARCH.md`, `RX7600_IP_FIRMWARE_MAP.md`,
`RX7600_EVIDENCE_GAPS.md`, and `RX7600_REPRODUCIBILITY_AUDIT.md`. It inspected
the repository's `LICENSE`, `BUILDING.md`,
`src/navi48-bringup/LICENSE`, `src/navi48-bringup/NOTICE`,
`third-party/THIRD-PARTY-LICENSES.txt`, representative local source headers,
and upstream references already cited in the IP/firmware map.

- **Verified locally:** directly present in a file in this checkout. A local
  notice can verify what the project declares, but not independently prove
  upstream ownership or the completeness of a license grant.
- **Cited, not independently verified:** the existing M2.1 documentation
  identifies a commit, path, symbol, or line range, but the upstream source
  was not present in the inspected checkout and was not fetched for this audit.
- **Unresolved / UNVERIFIED:** local evidence is insufficient for the claim.

The Linux reference commit recorded by M2.1 is
`d0c647a6aae2ce577372594a176c82733e1ed267`; this audit confirmed that exact
string is cited in `tools/ipdisc/RX7600_IP_FIRMWARE_MAP.md` and
`tools/ipdisc/README.md`. The Linux repository/object and commit metadata were
not available locally for independent object verification. The pinned
`pciutils/pciids` commit `22da7386ba818c037894f258248734770757b14f` is likewise
recorded in the map but was not independently checked in this audit. Neither
pin proves the exact IP versions on the user's physical board.

## Source provenance

The paths below are the paths cited by `RX7600_IP_FIRMWARE_MAP.md`. Its Linux
URLs point to commit `d0c647a6aae2ce577372594a176c82733e1ed267`; all are
**cited, not independently verified in this audit**. The pinned references
describe Linux implementation paths, not macOS behavior or a measurement of
the user's physical RX 7600.

| IP block or firmware family | Existing reference and cited location | Evidence grade and applicability | Missing evidence / limitation |
|---|---|---|---|
| IP Discovery format and per-IP record parsing | `torvalds/linux`, `drivers/gpu/drm/amd/include/discovery.h`; `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` | **Cited, not independently verified.** Linux format and decode path. | No locally available upstream source checkout; no trustworthy target dump, board attribution, or macOS parser integration. |
| Device identification | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c`; `pciutils/pciids`, `pci.ids` entry `1002:7480` | **Cited, not independently verified.** Linux PCI matching and a shared PCI database label. | Does not establish board/subsystem identity or actual physical-card IP values. |
| GC/GFX, CP, compute and GFX firmware | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c`; generic dispatch in `amdgpu_discovery.c` | **Cited, not independently verified.** Linux GFX v11 branch and conditional request names. | GC 11.0.2 is not established for the target; firmware binaries, hashes, provenance and macOS use are unresolved. |
| GMC, UMC, GFXHUB, MMHUB and memory management | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c`; dispatch in `amdgpu_discovery.c` | **Cited, not independently verified.** Linux version-dependent memory-controller handling. | Target UMC/IP versions, topology, BAR/VRAM layout and a macOS implementation are not established. |
| SDMA | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c`, `amdgpu_sdma.c` | **Cited, not independently verified.** Linux SDMA v6 path and version-derived request. | Target SDMA version and firmware revision/binary are unknown. |
| OSSSYS / IH interrupts | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` (OSSSYS dispatch); map identifies the IH v6 path | **Cited, not independently verified.** Linux dispatch path. | Physical-card IP version, MSI configuration, event handling, and macOS teardown are unknown. |
| PSP / MP0 firmware | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c`, `amdgpu_psp.c` | **Cited, not independently verified.** Linux conditional PSP 13.0.4 TOC/TA path. | No target MP0 version, actual blob, hash, source, licensing terms, or macOS loading evidence. |
| SMU / MP1 firmware and power management | `torvalds/linux`, `drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c`; dispatch in `amdgpu_discovery.c` | **Cited, not independently verified.** Linux version-derived SMU request. | Target MP1 version, board power data, firmware, and macOS control path remain unknown. |
| VCN/JPEG media firmware | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c`; dispatch in `amdgpu_discovery.c` | **Cited, not independently verified.** Linux version/instance-dependent VCN path. | Target media IP versions, instance count, blob and macOS media implementation are unknown. |
| Display Core / DCE / DMUB | `torvalds/linux`, `drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c`; DCE dispatch in `amdgpu_discovery.c` | **Cited, not independently verified.** Linux DCE-to-DMUB selection path. | Target display IP, firmware, VBIOS, physical outputs and macOS display path are unknown. |
| MES scheduler | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/mes_v11_0.c`, `amdgpu_mes.c`; GC dispatch in `amdgpu_discovery.c` | **Cited, not independently verified.** Linux conditional MES firmware path. | Whether the target reports/enables this path and which image it selects are unknown. |
| IP Discovery sideload | `torvalds/linux`, `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` | **Cited, not independently verified.** Linux optional/forced sideload path. | Not evidence that RX 7600 requires or contains `ip_discovery.bin`; no target file is available. |
| Other blocks (NBIO, DF, audio, ISP, VPE and board-specific engines) | Partial discovery coverage cited in `amdgpu_discovery.c` by M2.1; no full target inventory | **Unresolved.** The map itself says its block list is incomplete. | Need authenticated target discovery data and source tracing for every reported block. |

The project-local comparison path is **verified locally** as repository code,
not as RX 7600 evidence: `src/navi48-bringup/src/amd/gfx_v12_0.cpp` states
that it is ported from `mac-amdgpu` commit `3bdeed2`; its Linux analogues and
the local `src/navi48-bringup/NOTICE` are declarations in this checkout. The
notice gives `RDNA4FB` and `mac-amdgpu` project URLs but no corresponding
upstream commit for all reused code. This audit does not independently verify
those projects or revisions. The code is Navi 48-oriented and must not be
treated as proof of a Navi 33 path.

## Licensing and firmware redistribution

| Component / material | Available local evidence | Grade and what it establishes | Unresolved licensing questions |
|---|---|---|---|
| Cobalt64 repository | Root `LICENSE` contains MIT License terms and a 2026 Cobalt64 contributor copyright notice. | **Verified locally:** the repository includes an MIT license text. | Scope/ownership of every file and third-party-derived portion still depends on applicable notices; root MIT does not override separate component licenses. |
| Navi48Bringup kext | `src/navi48-bringup/LICENSE` contains BSD 3-Clause terms. `src/navi48-bringup/NOTICE` identifies reused RDNA4FB material (BSD 3-Clause) and mac-amdgpu-derived code (MIT), and says Linux driver sources are documentation only, no GPL code copied. | **Verified locally as declarations and supplied texts.** The local notice says applicable BSD/MIT attribution and conditions should be retained. | Upstream provenance, revision, file-by-file derivation and accuracy of each declaration were not independently audited against those remote projects. Human review is appropriate before redistribution. |
| RDNA4FB discovery implementation | `src/navi48-bringup/NOTICE` and `third-party/THIRD-PARTY-LICENSES.txt` include a BSD 3-Clause text and identify local discovery files as reused verbatim with headers retained. | **Verified locally as project records.** BSD notice conditions visible in the local text include retaining notices/disclaimers and restrictions on endorsement. | The upstream repository/revision and exact correspondence of all affected files were not fetched or compared in this audit. |
| mac-amdgpu-derived Navi 48 code | Local source headers, including `src/navi48-bringup/src/amd/gfx_v12_0.cpp`, cite mac-amdgpu commit `3bdeed2`; local NOTICE contains MIT text. | **Verified locally as source headers and license documentation.** | Short commit identifier is not independently resolved; attribution completeness and upstream license/source correspondence need review. |
| Linux AMDGPU source | Local NOTICE says Linux amdgpu driver sources are GPL-2.0 and used as documentation/reference only; no GPL code is copied into the kext. | **Verified locally only as the project's declaration; upstream source/license not inspected here.** | Cannot confirm from local evidence that no copied/derived Linux code exists elsewhere, or independently verify exact upstream license notices. |
| Linux generated AMD register headers | `third-party/THIRD-PARTY-LICENSES.txt` includes an AMD MIT notice and describes selected register headers/tables as generated from or cross-checked against Linux AMD headers. `src/navi48-bringup/src/amd/nbif_v6_3_1.h` has an AMD copyright comment. | **Verified locally as notices/comments.** Local text states the MIT notice/conditions. | Per-file origin and full attribution for every generated table were not exhaustively established in this audit. |
| Linux firmware / AMD firmware blobs | `BUILDING.md` says firmware is not in the repository, identifies linux-firmware as the source, describes the blobs as having a separate “AMD's own licence,” and lists ten Navi 48 filenames. Root `README.md` labels linux-firmware blobs “own licence.” | **Verified locally as project documentation only.** No firmware binary or blob-specific license text was present in the inspected documents. | **UNVERIFIED:** exact license for each blob, terms/version, redistribution permissions, required notices, provenance/hash, and whether different firmware families have different terms. A request filename or download availability grants no permission. |
| Mesa patches and code | Root `README.md` and `third-party/THIRD-PARTY-LICENSES.txt` point to Mesa's license and `mesa-patches/README.txt`. | **Cited locally;** not material to establishing RX 7600 Linux firmware rights. | The exact relevant Mesa source/license scope was not independently audited for this Navi 33 research task. |
| Other third-party components | `third-party/THIRD-PARTY-LICENSES.txt` describes metal2vulkan-n48 as LGPL-3.0-or-later and includes local notices/text; root README describes related dependencies. | **Verified locally as project notices.** | Full source provenance and compliance are outside this audit; do not infer that those terms apply to firmware or Linux source. |

No legal advice is offered. No conclusion that a firmware binary may be
redistributed, embedded, or loaded by macOS is supported by this audit.

## Evidence boundaries, questions, and blockers

### Established by this audit

- **VERIFIED locally:** the M2.1 map records the Linux commit SHA and source
  paths, but its external references were not independently checked against an
  upstream checkout during this audit.
- **VERIFIED locally:** project license, notice, and build documents contain
  the declarations summarized above.
- **UNVERIFIED:** authoritative blob-specific firmware license terms and
  redistribution permissions were not available in the inspected local
  material.
- **UNRESOLVED:** exact IP versions, instance inventory, firmware requirements,
  subsystem identity, and VBIOS of the user's physical RX 7600.
- **UNRESOLVED:** whether any Linux firmware can technically or legally be
  used by a future macOS implementation.

### Human review required

1. Obtain and review authoritative license/notice material for each exact
   firmware blob and revision before copying, embedding, or distributing it.
2. Review code provenance and notices file by file before any reuse, especially
   where Linux references, generated register tables, or third-party ports are
   involved.
3. Independently verify source revision IDs and cited code against upstream
   checkouts if exact upstream behavior is needed for a future implementation.
4. Establish target-board provenance and actual IP Discovery records before
   making board-specific IP or firmware claims.
5. Separately review macOS firmware-loading, signing, and security constraints;
   Linux request code does not answer these questions.

This audit does **not** establish Linux code compatibility with macOS, a
physical RX 7600's exact IP configuration, permission to redistribute firmware,
successful macOS build or initialization, or RX 7600 support. Parser tests and
synthetic fixtures validate parser behavior only. No firmware was downloaded,
opened, or executed, and no hardware interface was accessed.

**Single safest next action:** Preserve the unresolved boundary. If any future
work proposes firmware redistribution or source reuse, first have a human
review the exact relevant upstream source/license and blob-specific terms;
do not change driver code or access hardware as part of this audit.
