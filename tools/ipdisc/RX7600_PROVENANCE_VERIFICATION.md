# RX 7600 Upstream Provenance Verification

**Milestone:** M2.5 — offline verification of existing upstream references  
**Date:** 2026-10-20  
**Scope:** Existing Git objects, working-tree files, and project documents
inside this repository only. No network access, source fetch, clone, firmware
access, or hardware access was performed.

## Method and exact commands

Read `RESUME.md` first, followed by `WORKLOG.md`, `README.md`,
`RX7600_RESEARCH.md`, `RX7600_IP_FIRMWARE_MAP.md`,
`RX7600_EVIDENCE_GAPS.md`, `RX7600_REPRODUCIBILITY_AUDIT.md`, and
`RX7600_SOURCE_LICENSE_AUDIT.md`. Inspected the source/reference sections of
the M2.1 map and research report, plus `cpp_parity.md` and
`parity_cases.json`. Then checked repository state, object availability, and
whether the exact cited Linux source paths exist in the local working tree or
tracked index.

The following Git commands were run from the repository root:

```text
git status --porcelain=v2 --branch --untracked-files=all
git branch --show-current
git rev-parse HEAD
git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}'
git rev-parse '@{upstream}'
git rev-list --left-right --count HEAD...refs/remotes/origin/main
git rev-parse --is-shallow-repository
git count-objects -v
git cat-file -t d0c647a6aae2ce577372594a176c82733e1ed267
git cat-file -t 22da7386ba818c037894f258248734770757b14f
git ls-tree -r --name-only d0c647a6aae2ce577372594a176c82733e1ed267 -- drivers/gpu/drm/amd/include/discovery.h drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c
git ls-files -- 'drivers/gpu/drm/amd/include/discovery.h' 'drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c' 'drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c' 'drivers/gpu/drm/amd/amdgpu/amdgpu_ucode.c' 'drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c' 'drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c' 'drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c' 'drivers/gpu/drm/amd/amdgpu/amdgpu_sdma.c' 'drivers/gpu/drm/amd/amdgpu/psp_v13_0.c' 'drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c' 'drivers/gpu/drm/amd/amdgpu/amdgpu_psp.c' 'drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c' 'drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c' 'drivers/gpu/drm/amd/amdgpu/mes_v11_0.c' 'drivers/gpu/drm/amd/amdgpu/amdgpu_mes.c' 'drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c' 'pci.ids'
git ls-files | Select-String -Pattern '(^|/)(discovery\.h|amdgpu_discovery\.c|amdgpu_drv\.c|gfx_v11_0\.c|gmc_v11_0\.c|sdma_v6_0\.c|amdgpu_sdma\.c|psp_v13_0_4\.c|amdgpu_psp\.c|smu_v13_0\.c|amdgpu_vcn\.c|mes_v11_0\.c|amdgpu_mes\.c|amdgpu_dm\.c|pci\.ids)$'
```

An exact-name working-tree search was also run for those source filenames,
excluding `.git`. Repository documentation and parity metadata were searched
for the Linux commit, PCI commit, related short commit references, and
checksum terms. These checks were read-only.

## Repository state and local-object results

- Branch `main`; HEAD `d1f7bad8408a0a2c016b067c477422975a59de44`.
- Upstream `origin/main` and merge base resolve to the same HEAD; divergence
  `0 0`.
- `git rev-parse --is-shallow-repository` returned `false`.
- The object database reports one pack (`in-pack: 1097`), but this does not
  imply it contains unrelated upstream histories.
- `git cat-file -t` failed with `fatal: git cat-file: could not get object
  info` for both the Linux commit
  `d0c647a6aae2ce577372594a176c82733e1ed267` and PCI database commit
  `22da7386ba818c037894f258248734770757b14f`.
- `git ls-tree` for the Linux commit failed with `fatal: not a tree object`.
- The tracked-path query found no cited Linux source files or `pci.ids`.
  The exact-name working-tree search, excluding `.git`, also found no copies
  of the cited source filenames.
- No source-file checksum or provenance manifest for these upstream files
  was found in the inspected project documents. The fixture generator's
  SHA-256 manifest covers generated synthetic parser fixtures, not upstream
  source.

**Conclusion:** The local repository is not shallow, but it does not contain
the two cited upstream commit objects or the referenced Linux/PCI source
files. The commits, their tree paths, contents, cited line ranges, and
documented code behavior cannot be independently verified from available
local Git/source evidence.

## Source-by-source findings

The `RX7600_IP_FIRMWARE_MAP.md` links below record claims at the specified
revisions. In every row, “documented” means the reference exists in local
project documentation; it does not independently establish the upstream
object or content. “Unresolved” covers source/content provenance, not a claim
that the earlier research definitely did not inspect it.

| Claimed project / commit | Source path(s) cited in the research | Local commit/file/code/checksum result | Classification and limits |
|---|---|---|---|
| `torvalds/linux` / `d0c647a6aae2ce577372594a176c82733e1ed267` | `drivers/gpu/drm/amd/include/discovery.h` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** The parser's local C++ and Python code exist, but do not prove the cited Linux definitions. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** Discovery acquisition, decode, and dispatch claims cannot be inspected locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** Generic device matching and lack/presence of a Navi 33 row cannot be confirmed locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/amdgpu_ucode.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** Firmware prefix/version decoding cannot be inspected locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** GFX 11 firmware names and request conditions cannot be confirmed locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** GMC/UMC and hub dispatch claims cannot be confirmed locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c`, `amdgpu_sdma.c` | Both source files and commit object absent; no source checksums found. | **Documented, not independently verified.** SDMA branches and firmware request behavior cannot be inspected locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/psp_v13_0.c`, `psp_v13_0_4.c`, `amdgpu_psp.c` | Cited Linux source files and commit object absent; no source checksums found. | **Documented, not independently verified.** PSP versions and firmware requests cannot be confirmed locally. |
| Same Linux commit | `drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** SMU dispatch and firmware request conditions cannot be inspected locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** VCN firmware selection cannot be confirmed locally. |
| Same Linux commit | `drivers/gpu/drm/amd/amdgpu/mes_v11_0.c`, `amdgpu_mes.c` | Both source files and commit object absent; no source checksums found. | **Documented, not independently verified.** MES firmware names and conditions cannot be inspected locally. |
| Same Linux commit | `drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** DCE/DMUB firmware selection cannot be confirmed locally. |
| `pciutils/pciids` / `22da7386ba818c037894f258248734770757b14f` | `pci.ids`, entry `1002:7480` | Commit object absent; source file absent; no source checksum found. | **Documented, not independently verified.** The claimed shared Navi 33 product-group label cannot be checked from local source. Even if checked, it would not identify a physical board's IP records. |
| `lemonade-sdk/mac-amdgpu` / short hash `3bdeed2` | Cobalt64 local `src/navi48-bringup/src/amd/gfx_v12_0.cpp` header cites the hash and source path `dext/amdgpu/gfx_v12_0.cpp`. | The Cobalt64 port and its header are locally inspectable; the upstream commit object and file are absent, and no source checksum for the upstream file was found. | **Verified locally only as a project attribution statement.** Upstream identity/content and derivation are not independently verified. Navi 48 code is not RX 7600 evidence. |
| `somestupidgirl/RDNA4FB` / no exact commit recorded in local notice | Local `src/navi48-bringup/src/ipdiscovery.{hpp,cpp}` are declared verbatim reuse in `src/navi48-bringup/NOTICE`. | Local source and notice are inspectable; no exact upstream commit/object or upstream source file is available locally; no source checksum found. | **Verified locally only as a project notice.** Upstream provenance and exact correspondence remain unresolved. |
| AMD product page / no commit | RX 7600 product URL is cited in `RX7600_RESEARCH.md`. | No local copy or checksum recorded; research report says the page request timed out. | **Documented, not independently verified.** No product-page specifications are relied on as local evidence. |

The cited path names and SHA strings are therefore **documented references**.
Their presence in `README.md`, `RX7600_RESEARCH.md`,
`RX7600_IP_FIRMWARE_MAP.md`, `cpp_parity.md`, or `parity_cases.json` does not
authenticate source content. A path such as the local
`src/navi48-bringup/src/amd/amdgpu_discovery.h` is Cobalt64 source, not the
upstream Linux `amdgpu_discovery.c` and not proof of its exact revision.

## Reconciliation and evidence boundaries

- **Verified locally:** Git branch/HEAD/upstream status; repository is not
  shallow; the two claimed upstream commit objects are absent; the cited
  upstream source paths and PCI database file are not present locally; local
  documents contain the stated commit IDs and source links; fixture SHA-256
  data is for generated test inputs.
- **Documented, not independently verified:** Linux IP Discovery structures,
  dispatch branches, firmware request names and conditions; the `pci.ids`
  family mapping; and upstream code citations/line numbers. No current
  evidence contradicts these claims, but this offline checkout cannot
  corroborate them.
- **Unresolved:** Whether the earlier M2.1 research session actually inspected
  the exact upstream commit and code. The current local object store cannot
  prove or disprove that historical event.
- **Unresolved:** The physical RX 7600's exact IP versions, firmware selection,
  subsystem identity, and VBIOS. No trustworthy board-specific artifact is
  available in this project.
- **Not established:** Linux behavior does not establish macOS support or
  legal/technical permission to use Linux firmware in macOS.

No upstream behavioral claim was changed in M2.5 because absence of local
source does not contradict the prior report of research performed at the time.
The evidence status is clarified here instead. No claim is upgraded to
verified based on a commit string or filename.

## Verification and next safe action

The local parser's test suite is synthetic and unrelated to Linux source
provenance. Its M2.5 result is recorded in `WORKLOG.md` and `RESUME.md`.
Local Markdown links in the M2.5 report, README, worklog, and checkpoint were
checked after the edits.

**Single safest next action:** Keep the Linux references marked as documented
but not independently verified until an explicitly authorized provenance
method supplies an existing trusted source artifact or source checkout. This
milestone does not authorize fetching one. Keep the physical card's IP values
**UNRESOLVED** and do not begin driver implementation or hardware testing.
