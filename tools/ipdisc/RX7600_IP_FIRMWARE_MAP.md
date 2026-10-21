# RX 7600 / Navi 33 IP and Firmware Evidence Map

**Milestone:** M2.1 — source-only IP and firmware mapping  
**Research date:** 2026-10-20  
**State:** Linux source dispatch and firmware request behavior are mapped.
The exact IP Discovery values and firmware set reported by a physical RX 7600
remain unresolved. This document does not claim RX 7600 macOS support.

## Scope and methodology

This review compares the user-specified target (Radeon RX 7600, PCI ID
`1002:7480`, Navi 33, gfx1102) with version-pinned Linux AMDGPU code and the
checked-out Cobalt64 Navi 48 implementation. It is source research only:

- No GPU, PCI configuration space, MMIO, VRAM, firmware interface, or system
  was accessed.
- No firmware binary was downloaded, opened, extracted, flashed, or executed.
- No driver source, personality, plist, EFI, boot argument, or system setting
  was changed.
- The Python parser and tests operate on synthetic fixtures, not an RX 7600
  capture.

Evidence labels:

- **VERIFIED** — directly established by a cited source at the pinned revision.
- **INFERRED** — follows conditionally from a source branch, but is not
  directly assigned to the user's physical card.
- **UNRESOLVED** — the cited sources do not establish the target-specific fact.

Linux's IP Discovery code reads block versions from the discovery data for each
IP instance and uses those values to dispatch IP implementations and firmware
prefixes. Therefore, a Linux switch case proves that Linux has a code path for
that version; it does **not** prove that a particular `1002:7480` board reports
that version. No trusted, board-specific RX 7600 IP Discovery binary was
available in this research.

## Pinned source revisions

| Repository | Revision | Use |
|---|---|---|
| [`torvalds/linux`](https://github.com/torvalds/linux/commit/d0c647a6aae2ce577372594a176c82733e1ed267) | `d0c647a6aae2ce577372594a176c82733e1ed267` | AMDGPU IP Discovery parsing, version dispatch, initialization and firmware request definitions. GitHub's commit API returned the full SHA, subject `drm/amdgpu/discovery: support new discovery binary header`, and committer date 2026-03-11. |
| [`pciutils/pciids`](https://github.com/pciutils/pciids/commit/22da7386ba818c037894f258248734770757b14f) | `22da7386ba818c037894f258248734770757b14f` | PCI ID family label for `1002:7480`; not a board-specific identification source. |

No firmware content revision is pinned here. The Linux code establishes request
names and conditional request behavior, not the version, hash, provenance, or
license status of a particular firmware binary. The source locations below are
all linked at the Linux commit above. This records the revision inspected for
this research; it does not establish which commit the earlier lookup against a
moving `master` reference used.

## Device identity

| Claim | Evidence | Status | Interpretation |
|---|---|---|---|
| PCI ID `1002:7480` is listed under a Navi 33 product group. | Pinned `pci.ids`, vendor/device entry at line 4238; entry names multiple product variants. | **VERIFIED** | The shared device ID does not alone establish desktop RX 7600 board variant, subsystem IDs, VBIOS, memory layout, or display wiring. |
| Linux has an explicit `0x7480` / `CHIP_NAVI33` row in the pinned AMDGPU PCI table. | Pinned [`amdgpu_drv.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c#L2178-L2191) instead has generic AMD display/accelerator class matches using `CHIP_IP_DISCOVERY`; a search of this pinned file found no `0x7480` or `CHIP_NAVI33` literal. | **VERIFIED** | This pinned revision's generic IP Discovery fallback is not a Navi 33-specific ID-to-version table. The PCI database family label and Linux dispatch code are separate evidence. |
| The particular device reports gfx1102. | Supplied by the task; the pinned PCI ID table does not establish the GPU's live GC IP version. | **UNRESOLVED** | Treat gfx1102 as user-supplied until an independently trusted device report or IP Discovery record establishes it. Do not convert the label into a version/register assumption. |

## IP block and version table

The “RX 7600 reported value” column refers to what a physical target's IP
Discovery record would report. Every such value remains unresolved without a
trusted target dump. “Linux branch” lists versioned implementation paths found
in the pinned source, not a hardware assignment.

| Block / role | RX 7600 reported value | Pinned Linux branch evidence | Status and Cobalt64 implication |
|---|---|---|---|
| IP Discovery format and reported versions | Binary header version, IP Discovery table version, die list, per-IP version, instance and register bases: **UNRESOLVED** for this card. | [`discovery.h`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/include/discovery.h) defines `binary_header`, counted `binary_header_v2`, `table_info`, `ip_discovery_header`, `die_info`, `die_header`, and variable-length `ip`, `ip_v3`, and `ip_v4` records. The packed `ip_discovery_header` has 16 `die_info` slots (80 bytes); `ip_v4` has a 32/64-bit base-address union selected by the table flag. [`amdgpu_discovery.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L1480-L1495) initializes discovery before iterating the table; lines 1614-1645 store major/minor/revision and later variant/subrevision from each record into `ip_versions`. | **VERIFIED** that Linux consumes per-instance discovery records; target values **UNRESOLVED**. Cobalt64's offline parser supports v4 32-bit bases but explicitly does not support v4 64-bit bases. Synthetic parser tests do not provide a hardware inventory. |
| GC / GFX, CP and compute | **UNRESOLVED** | The generic dispatcher routes GC `11.0.0` through `11.0.4` and `11.5.x` to GFX v11 (`amdgpu_discovery.c` lines 2436-2477); `11.0.2` is one supported case. GC `11.0.x` also routes to GMC v11 (lines 2117-2156). `gfx_v11_0.c` contains firmware names for GC 11.0.2 and its init routine requires PFP, ME and MEC, plus RLC outside SR-IOV. | **VERIFIED** Linux support for those conditional branches; their association with `1002:7480` is **UNRESOLVED**. Do not treat gfx1102 or a firmware prefix as proof that the card reports GC 11.0.2. Navi 48 Cobalt code is GFX v12-specific. |
| Memory controller / GMC, UMC, GFX Hub, MM Hub | GC and UMC/MMHUB versions: **UNRESOLVED** | GMC v11 is selected from GC version (`amdgpu_discovery.c` lines 2117-2165). [`gmc_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c#L552-L612) contains UMC `8.10.0` and `8.11.0` handling and selects MMHUB/GFXHUB behavior from their own versions. | **VERIFIED** version-dependent Linux handling; the target UMC, memory topology, BAR/VRAM size, and page-table configuration are **UNRESOLVED**. Cobalt64's GMC v12 / GART cannot be reused by inference. |
| SDMA and asynchronous DMA submission | **UNRESOLVED** | `amdgpu_discovery.c` lines 2496-2540 routes SDMA0 `6.0.0`-`6.0.3` to SDMA v6; `6.0.2` is a supported branch. Firmware selection is derived from the SDMA0 version. | **VERIFIED** generic Linux branch; target association **UNRESOLVED**. Cobalt64's SDMA v7 source is a different generation. |
| OSSSYS / IH interrupts | **UNRESOLVED** | `amdgpu_discovery.c` lines 2173-2200 routes OSSSYS `6.0.0`-`6.0.2` to IH v6.0. | **VERIFIED** Linux dispatch; target version, interrupt source configuration, MSI behavior and teardown are **UNRESOLVED**. Cobalt64 currently uses IH v7.0. |
| PSP / MP0 | **UNRESOLVED** | `amdgpu_discovery.c` lines 2220-2278 routes MP0 `13.0.4` to `psp_v13_0_4_ip_block`. That implementation's firmware initializer calls TOC and TA loaders (`psp_v13_0_4.c` lines 35-56). | **VERIFIED** Linux support and requests for that version; not verified for the target. Cobalt64 requires PSP 14.0.3 and its initialization is not a Navi 33 path. |
| SMU / MP1 power management | **UNRESOLVED** | `amdgpu_discovery.c` lines 2291-2345 routes MP1 `13.0.4` through SMU v13. | **VERIFIED** Linux dispatch; exact target MP1 revision, power tables, clocks, thermal and board behavior are **UNRESOLVED**. Cobalt64 requires SMU 14.0.3. |
| UVD / VCN and JPEG media blocks | **UNRESOLVED** | `amdgpu_discovery.c` lines 2573-2665 routes UVD `4.0.3` to VCN v4.0.3 and JPEG v4.0.3; this is a version branch, not a device mapping. | **VERIFIED** Linux dispatch; target media IP versions/harvest state are **UNRESOLVED**. Cobalt64's media capability is not evidence for this target. |
| Display Core / DCE-DMU / DMUB | **UNRESOLVED** | Pinned `amdgpu_discovery.c` lines 2375-2397 recognizes DCE `3.2.0` and `3.2.1`. Pinned `display/amdgpu_dm/amdgpu_dm.c` lines 5769-5835 maps those versions to distinct DMUB firmware names. | **VERIFIED** version-to-path behavior in Linux; the card's display version, connector routing, PHYs, VBIOS data, and board configuration are **UNRESOLVED**. Cobalt64 uses a separate DCN 4.1 implementation. |
| MES scheduler / command queues | **UNRESOLVED** | `amdgpu_discovery.c` lines 2674-2697 routes GC 11.x to MES v11.0. Firmware request is conditional on the MES scheduler path being initialized; the request itself is required when that path is taken. | **VERIFIED** generic Linux dispatch/request behavior; whether this RX 7600 enables or needs the path is **UNRESOLVED**. Cobalt64's MES v12.1 is not interchangeable. |
| Other IPs (NBIO, DF, audio, ISP, VPE, board-specific engines) | **UNRESOLVED** | The discovery structure can report multiple hardware IDs and instances; the sources listed above do not establish a complete RX 7600 per-IP inventory. | **UNRESOLVED**; do not infer that the partial table is a complete hardware block list. |

## Firmware dependency table

Firmware names below are generated or selected by Linux code. A candidate
filename is applicable only if the actual discovery version selects that path.
These version-derived names are not tied to PCI ID `1002:7480` in the cited
request code; they are Linux paths for any device that reports the matching IP
version. “Required” describes Linux's request flag on that path; it does not
establish that the binary is present, licensed for redistribution, compatible
with macOS, or the right revision for the user's card.

| Component / role | Source-defined name or pattern | Linux request condition | Navi 33 association / macOS unknowns |
|---|---|---|---|
| GC graphics/compute engines: PFP, ME, MEC, RLC | If GC reports `11.0.2`, the version-derived prefix is `gc_11_0_2`, yielding `amdgpu/gc_11_0_2_pfp.bin`, `_me.bin`, `_mec.bin`, and `_rlc.bin`. `gfx_v11_0.c` advertises the exact 11.0.2 names at lines 95-98. | PFP, ME and MEC use `AMDGPU_UCODE_REQUIRED` in `gfx_v11_0.c` lines 730-753 and 786-800. RLC is required for non-SR-IOV at lines 762-784; kicker firmware changes the suffix. RLC backdoor-auto mode may request TOC. | The mapping to RX 7600 is **UNRESOLVED** until GC's reported version is known. Linux blob revision/hash and macOS loading/authorization are not established. |
| SDMA | If SDMA0 reports `6.0.2`: `amdgpu/sdma_6_0_2.bin`; names for SDMA 6.0.x are advertised in `sdma_v6_0.c` lines 49-57. | `amdgpu_sdma_init_microcode()` derives the version prefix and requests the selected instance firmware with `AMDGPU_UCODE_REQUIRED` (`amdgpu_sdma.c` lines 203-224); SDMA v6 calls it (`sdma_v6_0.c` lines 1311-1313). | Target version and exact instance count are **UNRESOLVED**. Firmware binary revision and macOS suitability are unknown. |
| PSP / MP0 | If MP0 reports `13.0.4`: `amdgpu/psp_13_0_4_toc.bin` and `amdgpu/psp_13_0_4_ta.bin`; advertised in `psp_v13_0_4.c` lines 32-33. Version decoding maps MP0 to a `psp_<major>_<minor>_<revision>` prefix (`amdgpu_ucode.c` lines 1445-1489). | The specialized 13.0.4 routine requests TOC and TA (`psp_v13_0_4.c` lines 35-50). Shared TOC and TA helpers use `AMDGPU_UCODE_REQUIRED` (`amdgpu_psp.c` lines 3516-3524 and 3967-3980). TA may use the kicker suffix. | MP0 13.0.4 for the RX 7600 is **UNRESOLVED**. The path's TOC/TA requests do not prove which binaries the user's board needs. No firmware was accessed. |
| SMU / MP1 | Version-derived `smu_<major>_<minor>_<revision>.bin`; for MP1 `13.0.4`, this formats to `amdgpu/smu_13_0_4.bin`. A kicker build requests `_kicker.bin`. | Required by `smu_v13_0.c` lines 93-114 except its SR-IOV VF early return. | MP1 13.0.4 is a Linux-supported branch, not verified for RX 7600. Board power table, firmware revision, trust chain and macOS delivery remain unknown. |
| VCN | If UVD reports `4.0.3`: `amdgpu/vcn_4_0_3.bin`; the exact macro and advertised name are in `amdgpu_vcn.c` lines 59 and 85-94. Version decoding maps UVD to `vcn_<major>_<minor>_<revision>`. | The VCN early init requests selected instance firmware with `AMDGPU_UCODE_REQUIRED` (`amdgpu_vcn.c` lines 99-124); per-instance suffixes depend on `per_inst_fw` and instance count. | Target UVD version and number of VCN instances are **UNRESOLVED**. No macOS decode/encode implementation or permitted binary source was verified. |
| MES scheduler | For GC 11.x, source advertises `gc_11_0_2_mes.bin`, `gc_11_0_2_mes_2.bin`, and `gc_11_0_2_mes1.bin` (`mes_v11_0.c` lines 36-60). MES code can also form a `*_uni_mes.bin` name when unified MES is enabled. | `amdgpu_mes_init_microcode()` requests the selected image with `AMDGPU_UCODE_REQUIRED`. For GC 11.x it uses scheduler/pipe suffix logic and retries the scheduler image as `_mes.bin` on the specified failure (`amdgpu_mes.c` lines 667-700). This is conditional on the MES path being selected. | No claim that Navi 33 enables MES or needs a particular image. Target GC version, scheduler mode and firmware version are **UNRESOLVED**. |
| Display DMUB | DCE `3.2.0` maps to `amdgpu/dcn_3_2_0_dmcub.bin`; DCE `3.2.1` maps to `amdgpu/dcn_3_2_1_dmcub.bin`. Definitions are in `display/amdgpu_dm/amdgpu_dm.c` lines 132-135 and selection in lines 5769-5835. | When DMUB support and a recognized DCE version select the path, `dm_init_microcode()` requires the selected firmware; unsupported DCE cases return without a DMUB request (`lines 5774-5835`). | DCE version for this target is **UNRESOLVED**. This does not settle connector-specific display behavior or prove the binary is usable in macOS. |
| IP Discovery sideload | `amdgpu/ip_discovery.bin` is listed by `amdgpu_discovery.c` line 125. | Forced when the Linux `amdgpu_discovery` parameter equals 2. Otherwise the version-specific firmware switch handles legacy ASICs and its default returns no filename; the normal path then reads discovery data from memory (`amdgpu_discovery.c` lines 461-490 and 516-546). | Not a Navi 33 firmware dependency established by this map. A real RX 7600 discovery binary is unavailable. |
| UMC/GMC, IH, DMU, other engines | No RX 7600-specific firmware filename established by the inspected source set. | **Unknown** unless a version-selected source path explicitly requests a binary. | Do not treat absence from this list as proof that no additional firmware is required; exact target data and more source tracing would be needed. |

## Initialization and dependency relationships

The pinned Linux sources establish this **data-flow relationship**, not a
complete portable initialization recipe:

1. The AMDGPU PCI table contains generic display/accelerator-class matches using
   `CHIP_IP_DISCOVERY` (`amdgpu_drv.c:2178-2191`). The same pinned file has no
   explicit `1002:7480` or `CHIP_NAVI33` row. The external PCI database labels
   the ID, but does not supply live IP versions.
2. `amdgpu_discovery_reg_base_init()` calls discovery initialization before
   walking the IP Discovery table (`amdgpu_discovery.c:1461-1495`). The table
   contains hardware IDs, instances, IP revisions, and register bases.
3. Linux stores reported versions and register offsets from those records
   (`amdgpu_discovery.c:1592-1645`). The implementation then selects IP blocks
   via version-based switches (for example, GC, PSP, SMU, SDMA, UVD and DCE).
4. Firmware names are derived from block type and reported version in
   `amdgpu_ucode_ip_version_decode()` (`amdgpu_ucode.c:1445-1489`) and are
   requested by each block with required/optional flags and feature-specific
   conditions.
5. Discovery acquisition in Linux can read reserved memory or use a selected
   sideload file. The `amdgpu_discovery_get_fw_name()` switch has named
   firmware for older ASICs and a default of no file; `amdgpu_discovery_init()`
   then selects file or memory (`amdgpu_discovery.c:461-490,516-546`). This is
   Linux behavior only and does not establish a safe macOS acquisition method.

The above is not an assertion of one strict global boot order for all IP blocks.
The lifecycle, recovery, dependencies, power sequencing and firmware trust
model must be traced for any future implementation. Linux kernel APIs and
firmware loading cannot be copied into a macOS kext by assumption.

## Comparison with Cobalt64's Navi 48 implementation

Repository evidence from the current checkout:

| Area | Existing Cobalt64 path | Difference / implication |
|---|---|---|
| Device match | `src/navi48-bringup/Info.plist:32,47,69,86` matches `0x7550` and `0x7551`; `Navi48Bringup.cpp:7327-7338` also gates `probe()` on boot arg `navi48bringup` and those IDs. | RX 7600 ID `0x7480` is intentionally absent. Do not add it to this personality or route it into this initialization path. |
| Firmware | `src/navi48-bringup/Makefile:27-28` requires `gc_12_0_1_*`, `psp_14_0_3_{sos,ta}`, `sdma_7_0_1`, and `smu_14_0_3`. | Those are Navi 48 development-path requirements, not Navi 33 firmware. Linux source request names are a research reference, not proof the blobs may be embedded or loaded by macOS. |
| Bring-up ladder | `src/navi48-bringup/src/amd/amdgpu_init.h:33-51` identifies IP Discovery, IH, GMC, PSP, SMU, IMU, RLC, CP, MES, GFX, SDMA and test stages. | Similar block names do not establish compatible registers, firmware interfaces, sequencing, or IP versions. |
| Display | `src/dcn41/dcn41.h:1` labels the code DCN 4.1 / Navi 48, gfx1201. | This is not evidence for any Navi 33 display-engine or DMUB version. |
| Hardware access | `Navi48Bringup.cpp:7342-7366` proceeds from `start()` to PCI memory enable, BAR mapping, MMIO checks and discovery loading. | Never test an RX 7600 by forcing this path. Its init has hardware side effects and lacks Navi 33 validation. |

## Verified facts versus inferences

| Finding | Status | Boundary |
|---|---|---|
| Pinned PCI database associates `1002:7480` with a multi-product Navi 33 group. | **VERIFIED** | Does not distinguish board variant or live IPD values. |
| Pinned Linux generic PCI class fallback uses `CHIP_IP_DISCOVERY`; its `amdgpu_drv.c` has no explicit `0x7480`/`CHIP_NAVI33` row. | **VERIFIED** | Does not prove that every board or macOS system binds successfully. |
| Linux records IP versions from IP Discovery, then selects code paths by version. | **VERIFIED** | Source-level behavior, not a physical target capture. |
| GC 11.0.2, SDMA 6.0.2, OSSSYS 6.0.2, MP0 13.0.4, MP1 13.0.4, UVD 4.0.3, and DCE 3.2.0/3.2.1 have relevant supported branches in the pinned source. | **VERIFIED** | The version list is a set of Linux dispatch cases, **not** a reported RX 7600 inventory. |
| Version-derived Linux firmware names follow from an IP block's version if that branch is selected. | **INFERRED** | Conditional naming/request behavior; not proof of target selection, binary availability, revision, legal use, or macOS compatibility. |
| The user's card reports gfx1102 and a particular combination of IP versions, bases, dies, or harvested blocks. | **UNRESOLVED** | Requires a trusted card-specific source; no hardware was inspected. |
| Linux firmware can be directly used by a macOS kernel extension. | **UNRESOLVED** | Requires firmware provenance/licensing review and a separate macOS loading/security design. |
| The Navi 48 implementation supports RX 7600. | **Not supported by evidence** | This report explicitly rejects that inference. |

## Unresolved questions and blockers

1. What are the exact binary-header and IP Discovery table versions on the
   user's particular board?
2. What are the actual IP version, instance count, variant/subrevision, harvest
   state and register-base records for GC, UMC/GMC, SDMA, OSSSYS/IH, MP0/PSP,
   MP1/SMU, UVD/VCN, DCE/DMUB, and any other present blocks?
3. Which `1002:7480` product variant, subsystem IDs, VBIOS, memory arrangement,
   and display-output wiring are present?
4. Which firmware binary revisions are requested by a real RX 7600 record,
   what are their hashes and permissible distribution/use terms, and could a
   macOS driver legally and safely load them?
5. What macOS version-specific IOKit/kernel interfaces, power sequencing,
   memory ownership, command queues, interrupts, display integration and
   recovery behavior would be required?
6. No isolated macOS installation, tested recovery path, required SDK/toolchain,
   MacKernelSDK, firmware set or hardware test environment is established.
   No build, kext load, GPU initialization, acceleration, or Metal result is
   claimed.

## Acceptance criteria for a future implementation milestone

Before any driver-source change:

1. Obtain a trusted, provenance-documented RX 7600 IP Discovery artifact
   without probing hardware in this milestone; record its hash and identify
   board/subsystem/VBIOS provenance. If no such artifact is available, keep
   target IP values unresolved.
2. Parse it offline with strict validation and independently cross-check the
   observed records against the pinned Linux version-dispatch code. Do not
   invent missing or inferred records.
3. Verify every firmware request selected by those actual versions, plus
   binary revision/hash, source and licensing constraints; do not download or
   execute firmware as part of source research.
4. Write a separate milestone proposal containing measurable offline tests,
   a separate Navi 33 initialization boundary, an exact affected-file list,
   failure containment, and a recovery plan. Explicitly exclude matching
   `0x7480` through the Navi 48 personality.
5. Establish the required isolated macOS test and recovery environment before
   requesting any hardware-dependent testing. Hardware access is not
   authorized by this research map.

## Source references

All Linux references below use the pinned commit
[`d0c647a6aae2ce577372594a176c82733e1ed267`](https://github.com/torvalds/linux/commit/d0c647a6aae2ce577372594a176c82733e1ed267).

- [PCI binding and generic IP Discovery fallback: `amdgpu_drv.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_drv.c#L2178-L2191)
- [Packed discovery structures: `discovery.h`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/include/discovery.h)
- [Discovery acquisition, validation, IP record decoding and block dispatch: `amdgpu_discovery.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c)
- [Firmware prefix/version decoder: `amdgpu_ucode.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_ucode.c#L1445-L1489)
- [GFX11 firmware declarations and required requests: `gfx_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c#L84-L106)
- [GMC/UMC/GFXHUB/MMHUB version handlers: `gmc_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c#L552-L612)
- [SDMA6 firmware families: `sdma_v6_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c#L49-L57)
- [Required version-derived SDMA firmware: `amdgpu_sdma.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_sdma.c#L203-L224)
- [PSP13.0.4 TOC/TA selection: `psp_v13_0_4.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c#L32-L56)
- [Required PSP TOC/TA request helpers: `amdgpu_psp.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_psp.c#L3516-L3524)
- [SMU13 version-derived required firmware request: `smu_v13_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c#L93-L114)
- [Version-derived required VCN firmware: `amdgpu_vcn.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_vcn.c#L57-L124)
- [MES firmware names: `mes_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/mes_v11_0.c#L36-L60)
- [Conditional required MES firmware request: `amdgpu_mes.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_mes.c#L667-L700)
- [DMUB firmware version selection: `display/amdgpu_dm/amdgpu_dm.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm.c#L5769-L5835)
- [Pinned PCI ID group: `pci.ids`, entry `1002:7480`](https://github.com/pciutils/pciids/blob/22da7386ba818c037894f258248734770757b14f/pci.ids#L4238)

## Outcome

M2.1 source research is complete to the limit of the available evidence. The
verified result is a set of Linux-supported, version-conditional IP and
firmware paths. **No exact Navi 33 IP-version inventory or complete RX 7600
firmware set has been verified for the user's physical card.** This is a
documented source limitation, not a failed parser test or permission to
substitute Navi 48 assumptions.
