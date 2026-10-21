# RX 7600 upstream source verification

**Milestone:** Phase 4 — bounded upstream source verification
**Date:** 2026-10-20

**Scope:** Public GitHub source metadata and exact pinned text files cited by
`RX7600_IP_FIRMWARE_MAP.md`. This was a read-only source review. No firmware,
hardware, PCI/MMIO/VRAM, or target interface was accessed.

## Summary

Both cited commit IDs exist and were independently retrievable from the
respective repositories' official GitHub API. The exact `discovery.h` and
`pci.ids` files, plus selected Linux dispatch/firmware files, were fetched from
their raw URLs at those pinned revisions. Retrieved bytes were hashed in
memory; source files were not copied into the repository.

This pass **partially verifies source claims only**:

- The Linux format header defines the binary and IP Discovery structures
  described below. It does not establish values reported by a physical card.
- Several pinned Linux files confirm version-named firmware declarations,
  selected version cases, or generic dispatch symbols. The principal Linux
  dispatch source `amdgpu_discovery.c` and `smu_v13_0.c` could not be retrieved
  from the raw endpoint (HTTP 429); their detailed dispatch/request claims
  remain **UNVERIFIED in this pass**.
- The PCI database lists `1002:7480` under a Navi 33 multi-product label. This
  is a database label, not physical-board attribution or an IP Discovery
  measurement.
- No `BOARD_SPECIFIC` or `TARGET_TESTED` evidence was obtained. Physical RX
  7600 IP versions and macOS compatibility remain **UNRESOLVED**.

GitHub's commit API reports both commits as unsigned (`verification.verified`
is false, reason `unsigned`). The API's returned exact SHA and pinned file
retrieval establish retrievability and content at that repository revision;
they do not establish a cryptographic signature by a particular author.

## Method and reproducibility

1. Read the current checkpoint, evidence plan/schema, provenance report, and
   IP/firmware map. Before research, the checkout was on branch `main` at
   `d1f7bad8408a0a2c016b067c477422975a59de44`; all 25 previously untracked
   paths were preserved.
2. Queried the official GitHub API commit endpoints:
   - <https://api.github.com/repos/torvalds/linux/commits/d0c647a6aae2ce577372594a176c82733e1ed267>
   - <https://api.github.com/repos/pciutils/pciids/commits/22da7386ba818c037894f258248734770757b14f>
3. Retrieved source text only from raw URLs using the full commit SHA in the
   URL. A small Python standard-library script used
   `urllib.request.urlopen()` and `hashlib.sha256()` on response bytes and
   searched cited paths for the symbols/strings listed below. No source files
   were written to disk.
4. Two raw-file requests returned HTTP 429. They were not retried or replaced
   with a moving branch. Claims depending on those file contents remain
   unverified.

For reproduction, the Linux raw URL pattern is:

```text
https://raw.githubusercontent.com/torvalds/linux/d0c647a6aae2ce577372594a176c82733e1ed267/<path>
```

The PCI raw URL is:

```text
https://raw.githubusercontent.com/pciutils/pciids/22da7386ba818c037894f258248734770757b14f/pci.ids
```

SHA-256 below is over the exact response body bytes retrieved from those URLs.
No upstream checksum manifest was consulted.

## Commit identity

| Repository and pinned revision | Retrieval result | Metadata and limitations |
|---|---|---|
| [`torvalds/linux`](https://github.com/torvalds/linux/commit/d0c647a6aae2ce577372594a176c82733e1ed267), `d0c647a6aae2ce577372594a176c82733e1ed267` | Official GitHub API returned HTTP 200 and the same full SHA. Its tree is `f079f06f7d9800f9feb9fe67d256b24471a8e83f`. | Subject: `drm/amdgpu/discovery: support new discovery binary header`; committer date `2026-03-11T17:58:07Z`. GitHub reports the commit signature as unverified/unsigned. The commit's existence does not prove every claim made about files at that revision. |
| [`pciutils/pciids`](https://github.com/pciutils/pciids/commit/22da7386ba818c037894f258248734770757b14f), `22da7386ba818c037894f258248734770757b14f` | Official GitHub API returned HTTP 200 and the same full SHA. Its tree is `2931ea4a5d58e9c1a4753d7bebf3b3f12487b1d6`. | Subject: `New snapshot generated`; commit date `2026-10-20T01:15:04Z`. GitHub reports the commit signature as unverified/unsigned. The database entry is an aggregated ID label, not evidence about an individual board. |

## Source-by-source results

### Linux `discovery.h`

**Status: `SOURCE_VERIFIED` for the narrow structure-definition claims below.**

- Exact path:
  [`drivers/gpu/drm/amd/include/discovery.h`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/include/discovery.h)
- Raw source:
  [pinned file](https://raw.githubusercontent.com/torvalds/linux/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/include/discovery.h)
- Retrieval: HTTP 200; 13,864 bytes.
- SHA-256: `032d97c699c9378edb64b005ddc53ab08763b853f6b64fcdb0a71dd7999b98e5`
- Observed declarations: `TOTAL_TABLES = 6`; the v1 and v2 binary header
  structures and v2 `num_tables`; `ip_discovery_header` with 16 `die_info`
  slots; and IP record forms `ip`, `ip_v3`, and `ip_v4`, with a 32/64-bit base
  address union in the v4 form.
- This supports the format-structure description at this revision. A
  structure declaration alone does not prove runtime validation behavior,
  compatibility with every producer, any physical RX 7600 values, or that
  Cobalt64's parser supports every form.

### Linux PCI matching and discovery source

| Exact source | Result and narrow claim | Limitation |
|---|---|---|
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c) | **`SOURCE_VERIFIED`** for the fetched file: 103,277 bytes; SHA-256 `6642b4e261db78a2216c00751ba4ea3c7a8d574f31b6f6853c268420ca1444e1`. Search found generic `.driver_data = CHIP_IP_DISCOVERY` entries at lines 2181, 2186, and 2191, and no `0x7480` or `CHIP_NAVI33` literal in the full file. | This is source-file evidence for the absence of those literals and presence of generic matches in this revision; it does not establish binding on a physical system or any live IP version. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c) | **Partially verified:** the official commit API lists this exact file among the commit's modified files and returned a patch showing `amdgpu_discovery_get_table_info()` selecting the v2 or v1 table list based on `version_major`. | Raw source retrieval returned HTTP 429. The complete file, detailed version switch cases, IP record storage claims, and cited line ranges were not verified in this pass; keep these behavioral claims **UNVERIFIED** here. |

### Other cited Linux files

The SHA-256 values identify the fetched file bytes at the pinned revision.
“Verified observation” is intentionally limited to the symbol, case, or
firmware string that was actually checked; it does not automatically verify
the full behavioral interpretation in the earlier map.

| Exact pinned path | Retrieved bytes / SHA-256 | Verified observation and boundary |
|---|---|---|
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_ucode.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_ucode.c) | 55,097 / `7c7e24d8f8887d80c332a0f54742a9862a42fb2dae54194900e840c587bb7233` | `amdgpu_ucode_ip_version_decode()` is present at line 1445. This pass did not validate every version-to-prefix branch in the function. |
| [`drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c) | 238,543 / `ea7ee8cf42864d7dbfd77e27ff3e6c20637bd9cffd3b1d236c30f95c4dd45d2c` | The `gc_11_0_2_pfp.bin` declaration occurs at line 95; `AMDGPU_UCODE_REQUIRED` occurs in the cited firmware-loading region (lines 682, 732, 750, 766, and 770). This confirms a version-named declaration and required-request markers, not the user's card's firmware requirements or every request condition. |
| [`drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c) | 28,606 / `792b404fb1c827a9016c00751ba4ea3c7a8d574f31b6f6853c268420ca1444e1` | The file contains `IP_VERSION(8, 10, 0)` at line 555 and `IP_VERSION(8, 11, 0)` at line 567. This verifies those cases exist, not the exact physical board version or all UMC/GMC implications. |
| [`drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c) | 60,229 / `e37f7f5be881bc1a6f2a9a6b7c2845338f1cda3d7adac73489a9858f2e5c7fcd` | `MODULE_FIRMWARE("amdgpu/sdma_6_0_2.bin")` occurs at line 51. This verifies a declared name, not that the target selects it or that the blob exists/is usable. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_sdma.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_sdma.c) | 17,238 / `522822484f13f2dee0a274c9f1dc6d56066bf884714a2b129afa547c4bf2fd56` | `amdgpu_sdma_init_microcode()` is present at line 203. The exact conditional and required-flag behavior is not asserted beyond the checked source occurrence in this pass. |
| [`drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c) | 10,815 / `1639ecd6902b32087f5283082281638a14991625c5e561cc713192ce073e8e88` | Firmware declarations for `psp_13_0_4_toc.bin` and `psp_13_0_4_ta.bin` occur at lines 32-33. This does not establish target selection, blob availability, licensing, or macOS suitability. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_psp.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_psp.c) | 127,313 / `918b024b2287aa244f20dd0a65dfd0b3c9c73836a5e7fd9292d21103bdb0d888` | `psp_init_toc_microcode()` and `psp_init_ta_microcode()` occur at lines 3516 and 3967. This pass did not revalidate all caller-specific flags and conditions. |
| [`drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c) | Not retrieved; no checksum | Raw source retrieval returned HTTP 429. Its request conditions and firmware claims remain **UNVERIFIED** in this pass. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c) | 45,328 / `37c2345433d395ae41fdda1f83c1b51f08ad67ace601944cdbd0b233bb677813` | `FIRMWARE_VCN4_0_3` names `amdgpu/vcn_4_0_3.bin` at line 59. This confirms a source constant, not request behavior or target applicability. |
| [`drivers/gpu/drm/amd/amdgpu/mes_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/mes_v11_0.c) | 53,857 / `f3e62774aff4c0f7e2b1d43141baafd148c4e017f9e78244521577ce34b519c6` | `MODULE_FIRMWARE("amdgpu/gc_11_0_2_mes.bin")` occurs at line 42. This confirms a declared name, not that the target selects or requires MES. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_mes.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_mes.c) | 24,226 / `0d33fce14050c0b0977ae5866be870aff68efa22efd8c8d44189a4a64ad3ecf5` | `amdgpu_mes_init_microcode()` is present at line 667. The complete conditional and required-request logic was not verified in this pass. |
| [`drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c) | 415,604 / `c9b3402933f9643997e186626cc135022d05d260c42b3ca302625dc154b1601a` | The source defines `dcn_3_2_0_dmcub.bin` at line 132 and `dcn_3_2_1_dmcub.bin` at line 134. This pass confirms names only, not every version-selection branch. |

### PCI database

**Status: `SOURCE_VERIFIED` for the exact database entry only.**

- Exact path:
  [`pci.ids`](https://github.com/pciutils/pciids/blob/22da7386ba818c037894f258248734770757b14f/pci.ids#L4238-L4239)
- Raw source:
  [pinned file](https://raw.githubusercontent.com/pciutils/pciids/22da7386ba818c037894f258248734770757b14f/pci.ids)
- Retrieval: HTTP 200; 1,675,208 bytes.
- SHA-256: `5061033a8c13354302ef9f01e8c439fa5221655aca4132dd85866efc52412634`
- At line 4238, device `7480` is labeled `Navi 33 [Radeon RX 7600/7600 XT/7600M XT/7600S / PRO W7600]`; line 4239 lists subsystem `1849:5313` as `RX 7600 Challenger OC`.
- This verifies what the database snapshot says. It does not identify the user's board, establish subsystem identity, prove the listed product group is exhaustive, or reveal live IP Discovery data.

## Evidence categories and remaining uncertainty

### Phase 5 follow-up for the two retrieval gaps

The exact pinned blobs for `amdgpu_discovery.c` and `smu_v13_0.c` were
subsequently retrieved through the official GitHub Contents/Blob API and
verified in memory. See
[`RX7600_REMAINING_SOURCE_CLAIMS.md`](RX7600_REMAINING_SOURCE_CLAIMS.md) for
their blob IDs, SHA-256 digests, source line ranges, and claim-by-claim
limitations. This resolves the Phase 4 **retrieval** gap and verifies the
narrow generic dispatch/request behavior described there; it does not promote
any claim to `BOARD_SPECIFIC` or `TARGET_TESTED`. The Phase 4 observations
above remain a record of that earlier bounded pass.

| Category | Result |
|---|---|
| `SOURCE_VERIFIED` | Exact pinned commit objects are publicly retrievable; the exact Linux structure definitions and PCI database entry were read; specific literals/cases/symbols in the fetched Linux files, including the narrow Phase 5 dispatch and SMU request behavior, are verified as scoped in this report and the Phase 5 follow-up. |
| `UNVERIFIED` | Full behavior claimed for source files where only declarations or symbols were checked; any claim not explicitly supported by the narrow observations above. The Phase 5 report retains this boundary for the decoder's complete prefix mapping and all physical-target implications. |
| `BOARD_SPECIFIC` | None. No physical RX 7600 artifact or target identity was examined. |
| `TARGET_TESTED` | None. No physical target or macOS system was tested. |

Even a verified Linux branch is generic source evidence. It cannot establish
that a physical `1002:7480` board reports the corresponding IP version or
requires a named firmware image. A filename declaration does not establish
that the binary exists, is licensed for redistribution, or is usable by
macOS. No claim of RX 7600 macOS support follows from this source review.

## Decision

The Phase 3 evidence plan's source-verification gate is **partially met** for
retrievable exact sources, and **not met** for the detailed Navi 33
version-to-implementation dispatch and SMU claims that could not be checked.
This bounded pass stops here; those claims remain unverified rather than
being expanded through moving branches or mirrors.

The project remains paused. The next justified gate is still a provenance-
documented, board-attributed IP Discovery artifact for offline strict parsing,
or a separately scoped source-verification task if the missing exact files
must be verified. Neither step establishes macOS compatibility or authorizes
hardware testing or implementation.
