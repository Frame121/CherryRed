# Remaining upstream source claims

**Milestone:** Phase 5 — bounded verification of two pinned Linux files

**Date:** 2026-10-20
**Source revision:** `torvalds/linux` commit
`d0c647a6aae2ce577372594a176c82733e1ed267`

This report resolves the two source-retrieval gaps recorded in Phase 4. The
scope is limited to `amdgpu_discovery.c` dispatch/version handling and
`smu_v13_0.c` firmware-request behavior. No firmware blob was retrieved,
written, or inspected. No hardware or target was accessed.

## Retrieval and content identity

The raw URL had returned HTTP 429 in Phase 4. Rather than retrying it, Phase 5
used the official GitHub Contents API for each exact path and revision, then
the GitHub Git Blob API using the blob object IDs returned by Contents:

| Path | Official pinned source URL | Official blob object | Retrieval/checksum |
|---|---|---|---|
| `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` | [GitHub file at pinned commit](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c); [Contents API](https://api.github.com/repos/torvalds/linux/contents/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c?ref=d0c647a6aae2ce577372594a176c82733e1ed267) | `163b40c8afb06c69f17f708f4b11f57a370bd5af` | HTTP 200; 100,281 bytes; SHA-256 `7159e6c3cf5101120bef3f33c4f2e23ea9632a096c7d76d47e17d2df23880d32` |
| `drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c` | [GitHub file at pinned commit](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c); [Contents API](https://api.github.com/repos/torvalds/linux/contents/drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c?ref=d0c647a6aae2ce577372594a176c82733e1ed267) | `4e09eda77d8b8acf4dcd29b47cf0c257c4287577` | HTTP 200; 71,132 bytes; SHA-256 `4c5a3d31dc6315f613692744e06760c948ee1c94a1800d1ccc2576fa3e4b1093` |

The full blob content was decoded in memory from the official API response and
hashed with Python's standard-library `hashlib.sha256`; no source file was
saved into the repository. The API blob URL can be reproduced as
`https://api.github.com/repos/torvalds/linux/git/blobs/<blob-object-id>`.
The GitHub file/blob pages and API establish that the content corresponds to
the pinned repository tree; the checksum records the exact bytes retrieved.

## `amdgpu_discovery.c`

**Status: `SOURCE_VERIFIED` for the exact generic source behavior below.**

| Source section | Directly established by source | Boundary |
|---|---|---|
| [`amdgpu_discovery_get_table_info()` lines 492-514](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L492-L514) | Selects v2's table list for `version_major == 2`, v1's list for `version_major == 1`, and returns `-EINVAL` for other major versions. | Does not establish safe parsing of all malformed input or the RX 7600's actual header. |
| [`amdgpu_discovery_reg_base_init()` lines 1461 onward; record mapping lines 1592-1645](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L1461-L1495) | Gets the IP Discovery table using the version-aware helper. The record loop maps hardware IDs to register offsets and assembles `ip_versions` from record major/minor/revision, plus variant/subrevision for table version 3 or newer. For table versions below 3, variant/subrevision are set to zero. | These are source-handling rules for data Linux receives; they do not provide any physical card's records or prove the provenance of a captured artifact. |
| [`amdgpu_discovery_set_common_ip_blocks()` lines 2054-2097](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2054-L2097) | Its GC-version switch maps GC 11.0.0-11.0.4 and 11.5.0-11.5.4 to `soc21_common_ip_block`. | Generic branch only; no device-ID-to-version assignment. |
| [`amdgpu_discovery_set_gmc_ip_blocks()` lines 2114-2166](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2114-L2166) | Switches on GC version; 11.0.0-11.0.4 and 11.5.0-11.5.4 add `gmc_v11_0_ip_block`. | The comments and switch describe generic source dispatch, not a measured GMC/UMC version for the target. |
| [`amdgpu_discovery_set_ih_ip_blocks()` lines 2171-2215](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2171-L2215) | Switches on OSSSYS version; 6.0.0-6.0.2 add `ih_v6_0_ip_block`. | Does not associate those versions with PCI ID `1002:7480` or establish target interrupt behavior. |
| [`amdgpu_discovery_set_psp_ip_blocks()` lines 2218-2286](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2218-L2286) | Switches on MP0 version; 13.0.4 adds `psp_v13_0_4_ip_block`. | No evidence that the target reports MP0 13.0.4 or needs any particular PSP firmware. |
| [`amdgpu_discovery_set_smu_ip_blocks()` lines 2289-2352](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2289-L2352) | Switches on MP1 version; 13.0.4 is among the versions assigned to `smu_v13_0_ip_block`. | This selects a Linux implementation from a reported version. It does not prove a target version, firmware asset, board power behavior, or macOS suitability. |
| [`amdgpu_discovery_set_display_ip_blocks()` lines 2373-2397](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2373-L2397) | When DC is supported and the DCE version is nonzero, the switch recognizes DCE 3.2.0 and 3.2.1 among its explicit versions. | Recognition is not board identification or proof of display outputs, PHY routing, or successful display operation. |
| [`amdgpu_discovery_set_gc_ip_blocks()` lines 2434-2480](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2434-L2480) | Switches on GC version; 11.0.0-11.0.4 and 11.5.0-11.5.4 add `gfx_v11_0_ip_block`. | Generic version dispatch, not a Navi 33 PCI ID mapping. |
| [`amdgpu_discovery_set_sdma_ip_blocks()` lines 2494-2544](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2494-L2544) | Switches on SDMA0 version; 6.0.0-6.0.3 add `sdma_v6_0_ip_block`. | Does not prove SDMA version, count, firmware revision, or target requirements. |
| [`amdgpu_discovery_set_vcn_ip_blocks()` lines 2600-2668](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2600-L2668) | In the UVD version switch, 4.0.3 adds `vcn_v4_0_3_ip_block` and `jpeg_v4_0_3_ip_block`. | Does not establish that the physical board reports UVD 4.0.3 or has either block enabled. |
| [`amdgpu_discovery_set_mes_ip_blocks()` lines 2671-2687](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2671-L2687) | GC versions 11.0.0-11.0.4 and 11.5.0-11.5.4 add `mes_v11_0_ip_block` and set `enable_mes` and `enable_mes_kiq` true. | This is generic enablement based on GC version. It does not itself prove MES firmware request details or target applicability. |

These are explicit source branches. A statement such as “RX 7600 uses this
branch” would be an inference unsupported by the source alone. The file
contains version-driven switches; they do not turn `1002:7480` into a
board-specific IP inventory. The `amdgpu_discovery.c` dispatch evidence is
not a Navi 33-specific PCI match.

## `smu_v13_0.c`

**Status: `SOURCE_VERIFIED` for the request logic in
`smu_v13_0_init_microcode()`; exact target firmware remains unverified.**

- Exact function:
  [`smu_v13_0_init_microcode()` lines 93-117](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c#L93-L117)
- It returns success without requesting SMU firmware when
  `amdgpu_sriov_vf(adev)` is true (lines 102-104).
- Otherwise it calls `amdgpu_ucode_ip_version_decode()` for `MP1_HWIP` to
  produce `ucode_prefix` (line 106).
- If `amdgpu_is_kicker_fw(adev)` is true, it requests
  `"amdgpu/%s_kicker.bin"`; otherwise it requests `"amdgpu/%s.bin"` (lines
  108-113). Both requests pass `AMDGPU_UCODE_REQUIRED`.
- A request error follows the error path (lines 114-117).

Directly established: Linux applies an IOV VF early-return condition, then
requests a required version-derived filename, with a conditional kicker
suffix. The exact prefix formatting depends on the decoder's behavior; this
pass does not re-audit that function's every mapping. The module's
`MODULE_FIRMWARE` declarations elsewhere in the file are declarations, not
proof of target selection or asset availability.

Not established: that the user's physical card reports MP1 13.0.4, that a
particular `smu_13_0_4` binary is present or appropriate, that firmware can
legally be redistributed, or that Linux firmware can be loaded by macOS.

## Evidence category and decision

| Category | Phase 5 result |
|---|---|
| `SOURCE_VERIFIED` | The two exact file blobs at the pinned Linux commit and the narrow generic dispatch/request claims above. |
| `BOARD_SPECIFIC` | None. No physical-card evidence was obtained. |
| `TARGET_TESTED` | None. No target test was performed. |

No hardware or firmware interface was accessed and no firmware binary was
downloaded. RX 7600 macOS support is not established. The project remains
paused; the next decision gate requires board-attributed evidence or a
separately authorized future task, not additional inference from generic
Linux branches.
