# RX 7600 / Navi 33 IP Block Dispatch Specification

**Research date:** 2026-10-21
**Target under investigation:** AMD Radeon RX 7600, reported PCI ID `1002:7480`
**Linux reference:** [`torvalds/linux@d0c647a6aae2ce577372594a176c82733e1ed267`](https://github.com/torvalds/linux/commit/d0c647a6aae2ce577372594a176c82733e1ed267), commit subject `drm/amdgpu/discovery: support new discovery binary header`, dated 2026-03-11.
**PCI-name reference:** [`pciutils/pciids@22da7386ba818c037894f258248734770757b14f`](https://github.com/pciutils/pciids/commit/22da7386ba818c037894f258248734770757b14f).

## Scope and conclusion

This is a map of **conditional Linux source dispatch paths**, not an IP
inventory for a physical RX 7600. The pinned Linux implementation reads IP
versions from IP Discovery records and dispatches on those values. The
reviewed `amdgpu_drv.c` source uses generic IP-Discovery-capable PCI class
matches; it does not establish a `1002:7480`-to-IP-version mapping. The pinned
PCI database groups `7480` with several Navi 33 product names, but that label
does not identify a specific board or its IP records.

Accordingly, every version below means **“Linux has a route if the device
reports this version”**. It does not mean the RX 7600 reports it. No
board-attributed IP Discovery binary, subsystem/VBIOS evidence, or physical
card report was available. The supplied `gfx1102` label is not independently
verified here. No dispatch table should be hard-coded for Navi 33 from this
specification.

## Conditional IP dispatch

The versions are the cases in the pinned generic Linux switches; they are
not asserted as Navi 33 observations.

| IP area / Linux dispatch | Conditional source-supported versions and route | RX 7600 status |
|---|---|---|
| GC common / GFX | `amdgpu_discovery_set_common_ip_blocks()` and `amdgpu_discovery_set_gc_ip_blocks()` route GC `11.0.0`–`11.0.4` and `11.5.0`–`11.5.4` to `soc21_common_ip_block` and `gfx_v11_0_ip_block`, respectively. | GC version, GFX revision, instance count, variant, and harvest state **unknown**. `11.0.2` is a source-supported branch, not a measured RX 7600 value. |
| GMC / memory hubs | `amdgpu_discovery_set_gmc_ip_blocks()` routes the same GC 11.x ranges to `gmc_v11_0_ip_block`. `gmc_v11_0.c` contains UMC `8.10.0` and `8.11.0` handling; this does not establish which UMC or MMHUB/GFXHUB versions a board reports. | GC, MMHUB, UMC versions, memory topology, and enabled instances **unknown**. No board-specific mapping is established. |
| IH / interrupts | `amdgpu_discovery_set_ih_ip_blocks()` routes OSSSYS `6.0.0`–`6.0.2` to `ih_v6_0_ip_block`; `6.1.0`–`6.1.1` route to `ih_v6_1_ip_block`. | OSSSYS/IH revision and interrupt configuration **unknown**. |
| PSP / MP0 | `amdgpu_discovery_set_psp_ip_blocks()` routes MP0 `13.0.4` to `psp_v13_0_4_ip_block`; other listed MP0 13.x cases use the generic `psp_v13_0_ip_block`. | MP0 revision and PSP route **unknown**. `13.0.4` is not assigned to `1002:7480` by this source. |
| SMU / MP1 | `amdgpu_discovery_set_smu_ip_blocks()` routes listed MP1 `13.0.x` versions—including `13.0.4`—to `smu_v13_0_ip_block`. | MP1 revision, board power data, and SMU route **unknown**. |
| SDMA | `amdgpu_discovery_set_sdma_ip_blocks()` routes SDMA0 `6.0.0`–`6.0.3` and `6.1.0`–`6.1.4` to `sdma_v6_0_ip_block`. | SDMA version, instance count/mask, and route **unknown**. |
| VCN / JPEG | With no VCE block, `amdgpu_discovery_set_mm_ip_blocks()` routes UVD `4.0.3` to `vcn_v4_0_3_ip_block` and `jpeg_v4_0_3_ip_block`. The switch has other version-specific branches. | UVD version, VCN instance count, codec fuses/harvest state, and route **unknown**. |
| Display | When the display path is enabled, `amdgpu_discovery_set_display_ip_blocks()` recognizes DCE `3.2.0` and `3.2.1` among its supported versions and can add `dm_ip_block` (with an SR-IOV display alternative). | DCE/DC version, DMUB path, VBIOS, connector/PHY wiring, and board outputs **unknown**. A source branch is not evidence of working RX 7600 display. |
| MES | `amdgpu_discovery_set_mes_ip_blocks()` routes GC 11.x cases in the GFX row to `mes_v11_0_ip_block` and enables MES/KIQ flags. | GC revision, MES runtime selection, and firmware use **unknown**. |

The pinned parser supports data structures used for IP Discovery, but neither
the Linux source nor Cobalt64's synthetic parser fixtures provide the missing
physical-board values. In particular, do not substitute Navi48/GFX12,
GMC12, IH7, SDMA7, PSP14, SMU14, or DCN 4.1 values from this repository.

## Conditional Linux firmware paths

Each example below is a **source-derived possibility if the stated IP version
and code path are selected**. Linux request flags and names describe Linux
behavior only. Exact board selection, blob provenance/hash, blob license, and
macOS loading/compatibility are unresolved for every row.

| Family | Source-level selection/request condition | Example name(s) only when condition holds | Board selection / binary provenance / license / macOS |
|---|---|---|---|
| GFX / GC | `gfx_v11_0` derives a firmware prefix from reported GC IP version. Its load paths request PFP, ME, MEC, and (outside SR-IOV) RLC; request flags/alternate kicker paths depend on the specific loader branch. | If GC is `11.0.2`: `gc_11_0_2_{pfp,me,mec,rlc}.bin`. RLC backdoor-auto mode may request TOC. | **Unresolved.** A declared/requested name does not show that this board selects the branch or that the blob is present, licensed, or usable by macOS. |
| SDMA | `amdgpu_sdma_init_microcode()` derives the selected SDMA firmware name from the reported SDMA version and requests selected instance firmware as required by that path. | If SDMA0 is `6.0.2`: `sdma_6_0_2.bin`. | **Unresolved** for target selection, instance requirements, blob identity/rights, and macOS. |
| PSP / MP0 | The MP0 `13.0.4` block initializer calls the TOC and TA firmware helpers. Those helpers make required Linux firmware requests; TA naming can vary with kicker mode. | If MP0 is `13.0.4`: `psp_13_0_4_toc.bin`, `psp_13_0_4_ta.bin` (possible kicker variant is conditional). | **Unresolved** for target selection, exact blobs/versions, license, and macOS. |
| SMU / MP1 | `smu_v13_0_init_microcode()` returns without a request for an SR-IOV VF. Otherwise it decodes MP1 into a prefix and requests the required `<prefix>.bin`, or `<prefix>_kicker.bin` when kicker firmware is selected. | If MP1 is `13.0.4`: `smu_13_0_4.bin` or conditional `smu_13_0_4_kicker.bin`. | **Unresolved** for target selection, exact blob, license, and macOS. |
| VCN | `amdgpu_vcn_early_init()` decodes UVD version and requests firmware as required. A per-instance filename is used for nonzero instances only when `per_inst_fw` is enabled; otherwise instances share instance 0's firmware. | If UVD is `4.0.3`: `vcn_4_0_3.bin` (per-instance suffix only under the stated condition). | **Unresolved** for target version/count, exact blob, license, and macOS. |
| Display DMUB | The display code selects DMUB firmware from supported DCE revisions when the corresponding display/DMUB path is active; unsupported or inactive paths do not establish a request. | DCE `3.2.0` or `3.2.1` selects the correspondingly versioned `dcn_3_2_*_dmcub.bin` family in the cited Linux code. | **Unresolved** for target selection, exact blob, license, board routing, and macOS. |
| MES | GC 11.x dispatch enables the MES v11 block. Firmware names are declared by `mes_v11_0.c`; the request is conditional on MES firmware initialization/scheduler path. | GC `11.0.2` name declarations include `gc_11_0_2_mes.bin`, `gc_11_0_2_mes_2.bin`, and `gc_11_0_2_mes1.bin`; declaration alone is not selection. | **Unresolved** for target enablement, selected scheduler image, exact blob, license, and macOS. |
| GMC/MMHUB and IH | The cited dispatch sources establish versioned code paths; this review did not establish a target-specific firmware filename/request for these blocks. | None asserted. | **Unresolved**; absence here is not proof that no firmware or other prerequisite exists. |

**Licensing boundary:** The inspected AMD source files carry permissive
source-file license notices (including MIT notices); Cobalt64's local
third-party record treats Linux driver implementation as reference-only, and
the user has required GPL Linux code to remain reference-only. This work copies
no Linux implementation. Firmware blobs are separate materials: their
individual licenses, hashes, provenance, and redistribution/use terms were not
verified and must not be inferred from source-file licenses or filenames.

## Source evidence table

All Linux source links below are pinned to the full commit
`d0c647a6aae2ce577372594a176c82733e1ed267`. Links identify source locations,
not target measurements.

| Exact file / source | Relevant source symbols or definitions | Evidence established and limit |
|---|---|---|
| [`drivers/gpu/drm/amd/include/discovery.h`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/include/discovery.h) | `binary_header`, `binary_header_v2`, `TOTAL_TABLES`, `ip_discovery_header`, `ip`, `ip_v3`, `ip_v4`, `die_header` | Defines the generic packed formats and variable records. Does not provide physical-card field values. File header gives an MIT permission notice. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c#L2178-L2191) | Generic PCI class match entries with `CHIP_IP_DISCOVERY` | Supports the distinction between generic IP Discovery binding and a direct `1002:7480`-to-IP inventory. Prior pinned-source search recorded no literal `0x7480` or `CHIP_NAVI33` row in this file. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c) | `amdgpu_discovery_init`, `amdgpu_discovery_reg_base_init`, `amdgpu_discovery_set_common_ip_blocks`, `amdgpu_discovery_set_gmc_ip_blocks`, `amdgpu_discovery_set_ih_ip_blocks`, `amdgpu_discovery_set_psp_ip_blocks`, `amdgpu_discovery_set_smu_ip_blocks`, `amdgpu_discovery_set_gc_ip_blocks`, `amdgpu_discovery_set_sdma_ip_blocks`, `amdgpu_discovery_set_mm_ip_blocks`, `amdgpu_discovery_set_display_ip_blocks`, `amdgpu_discovery_set_mes_ip_blocks` | Source-level acquisition/record decode and version-switch behavior. It dispatches from reported IP versions; these functions do not attribute a particular set to this PCI ID. File header gives an MIT permission notice. |
| [`drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c#L84-L106) and [firmware load path](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c#L682-L800) | `gfx_v11_0_ip_block`, `MODULE_FIRMWARE`, GFX microcode initialization | Version-family names and conditional GFX requests, not target selection. |
| [`drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c#L552-L612) | UMC `IP_VERSION(8, 10, 0)` and `IP_VERSION(8, 11, 0)` cases | Generic GMC v11 handling. No board-specific GMC/MMHUB/UMC values. |
| [`drivers/gpu/drm/amd/amdgpu/ih_v6_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/ih_v6_0.c) | `ih_v6_0_ip_block`, IH v6 implementation | The dispatcher can select this implementation for OSSSYS 6.0.x; no target observation. |
| [`drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c#L32-L56) and [`amdgpu_psp.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_psp.c#L3516-L3524) | `psp_v13_0_4_init_microcode`, TOC/TA request helpers | Conditional PSP 13.0.4 firmware path; not proof of the board's MP0 version or blob rights. |
| [`drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c#L93-L117) and [`amdgpu_ucode.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_ucode.c#L1445-L1489) | `smu_v13_0_init_microcode`, `amdgpu_ucode_ip_version_decode` | Required version-derived request except the SR-IOV VF early return; kicker choice is conditional. Not a target-specific selection. |
| [`drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c#L49-L57) and [`amdgpu_sdma.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_sdma.c#L203-L224) | `sdma_v6_0_ip_block`, `amdgpu_sdma_init_microcode` | SDMA v6 version family and conditional per-instance required request. No target version/count. |
| [`drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c#L57-L124) | `FIRMWARE_VCN4_0_3`, `amdgpu_vcn_early_init` | Version-derived required request and per-instance/shared firmware conditions. No target version/count. |
| [`drivers/gpu/drm/amd/amdgpu/mes_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/mes_v11_0.c#L36-L60), [`amdgpu_mes.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_mes.c#L667-L700) | MES v11 firmware declarations and `amdgpu_mes_init_microcode` | GC 11.x MES names and conditional loader path. A declaration does not prove the image is requested for this board. |
| [`drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c#L5769-L5835) | DMUB DCE-version selection and `dm_init_microcode` | Conditional DCE/DMUB selection. Does not identify card outputs or prove display operation. |
| [`pci.ids`](https://github.com/pciutils/pciids/blob/22da7386ba818c037894f258248734770757b14f/pci.ids#L4238-L4239) | Device `1002:7480` and a listed subsystem entry | Database product grouping only. Does not establish the user's board/subsystem identity or IP Discovery records. |

The AMD Linux C files inspected for this map contain permissive MIT source
notices. The project and this task nevertheless keep Linux implementation
code as reference-only; no Linux source was copied or adapted. This does not
establish firmware blob licensing.

## Evidence gaps and next milestone

Still missing:

1. A provenance-documented original IP Discovery artifact attributed to the
   actual RX 7600 board, including its acquisition method and hash.
2. Independently linked board model, subsystem vendor/device IDs, and VBIOS
   identity; do not infer them from `1002:7480` or a database's product list.
3. Parsed die/IP versions, instances, variant/subrevision, harvest state, and
   register bases from that artifact.
4. For versions actually observed, exact firmware request branches, blob
   identity/hash/provenance, authoritative blob-specific license terms, and
   any platform-specific loading constraints.

**Next milestone:** Review an already-existing, user-supplied, provenance-
documented board artifact offline with strict parsing, then cross-reference
only its observed versions against this pinned Linux commit. Do not probe the
physical GPU to create the missing evidence. No implementation or hardware
initialization is authorized by this specification.
