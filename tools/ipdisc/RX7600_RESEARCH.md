# RX 7600 / Navi 33: Offline Architecture Research

**Milestone:** M2.0 — architecture and feasibility research  
**Research date:** 2026-10-20  
**State:** Complete as a documentation/research milestone only. No driver
implementation or hardware validation is claimed.

## Scope and evidence labels

This report records an offline source review for the user-specified RX 7600,
PCI ID `1002:7480`, Navi 33 / gfx1102 target. The PCI identifier and hardware
configuration are supplied by the user unless a source is cited below. This
session did not access a GPU, collect an RX 7600 IP Discovery dump, build or load
a kext, or verify display/Metal behavior.

- **Repository evidence** means a claim found in the checked-out project. It
  documents project intent and code paths, not independent reproduction of the
  repository's hardware claims.
- **Pinned upstream evidence** means a source file fetched from the exact
  commit/revision linked below.
- **User-supplied target** means input from the task, not independently measured
  hardware evidence.
- **Inference/open** means a question not resolved by this review and not safe
  to turn into a driver assumption.

## Findings

### Device and architecture boundary

The pinned `pci.ids` data identifies AMD device `1002:7480` as “Navi 33
[Radeon RX 7600/7600 XT/7600M XT/7600S/7700S / PRO W7600]”. This confirms that
the PCI ID is shared across multiple board/product variants; the device ID
alone cannot establish desktop RX 7600 board identity or its subsystem, VBIOS,
memory size, or display wiring. The user identifies the particular target as
RX 7600, 8 GB, gfx1102; this is not independently measured here.

The repository's stated supported family is RDNA 4 / Navi 48, with RX 9070 XT
as its development card; it explicitly calls RDNA 3 / RX 7000 future work
(`README.md:36-38`). Its bring-up personality and C++ probe are deliberately
restricted to device IDs `0x7550` and `0x7551`
(`src/navi48-bringup/Info.plist:32,47,69,86`;
`src/navi48-bringup/src/Navi48Bringup.cpp:7327-7338`). Probe also requires
boot argument `navi48bringup`; after matching, `start()` enables PCI memory,
maps BAR registers, probes VRAM, and reads on-die discovery
(`Navi48Bringup.cpp:7342-7366`). Adding `0x7480` to those personalities would
therefore route a different ASIC into code which programs hardware. It is
explicitly not proposed or performed.

### Navi 33 versus Navi 48

The architecture labels are not interchangeable:

| Area | RX 7600 / Navi 33 target | Existing Cobalt64 bring-up |
|---|---|---|
| GPU generation | User-specified RDNA 3 / gfx1102; pinned PCI database associates `1002:7480` with Navi 33. | Repository identifies the active target as RDNA 4 / Navi 48 / gfx1201 (`README.md:36-38`; `Navi48Bringup.hpp:2`). |
| Graphics firmware generation | Pinned Linux `gfx_v11_0.c` declares GFX firmware `gc_11_0_2_pfp`, `me`, `mec`, and `rlc` (lines 95-98). This establishes a GFX 11.0.2 firmware family in Linux, not a complete per-board firmware selection. | The Makefile requires `gc_12_0_1_*`, `psp_14_0_3_*`, `sdma_7_0_1`, and `smu_14_0_3` (`src/navi48-bringup/Makefile:20,27-29`). |
| IP block programming | Exact per-IP versions and board-dependent features must be read from a verified Navi 33 discovery record and checked against generation-specific Linux implementations. | Bring-up ladder and source modules are generation-specific: IP Discovery, IH, GMC, PSP, SMU, IMU, RLC, CP, MES, GFX, SDMA, PM4, and compute (`src/navi48-bringup/src/amd/amdgpu_init.h:33-51`; `amd/` sources use v12/v14/v7). |
| Display | Navi 33 display IP revision, connectors, PHY setup, and VBIOS/AtomBIOS dependencies were not established for this specific board. | Cobalt's display implementation is DCN 4.1 / gfx1201 (`src/dcn41/dcn41.h:2`; `src/navi48-bringup/src/dcn/`). |
| macOS integration | No Apple-supported RX 7600 path is established by this repository. | Existing IOKit kext, private kernel interfaces, native user client, Mesa RADV port, and Metal bundle target the Navi 48 path (`README.md:72-76`). |

Linux sources share subsystem abstractions across generations, but their
version-specific IP function tables, firmware, register layouts, queue
protocols, power sequencing, interrupt blocks, and display engine are selected
by IP version. Reusing a function because it has a similar name is not evidence
of register compatibility. The existing Cobalt64 `gfx_v12_0.cpp`,
`gmc_v12_0.cpp`, `ih_v7_0.cpp`, `mes_v12_1.cpp`, and `sdma_v7_0.cpp` are not a
Navi 33 port.

### IP Discovery

AMD's pinned `discovery.h` defines the packed binary formats, including the
fixed six-entry v1 table list, counted v2 table list, 80-byte IP Discovery
header, die descriptors, and variable-length IP records. The offline Python
parser in this directory validates synthetic v1/v2 examples and strict
structural bounds; its tests do not establish what a physical RX 7600 reports.

Pinned Linux `amdgpu_discovery.c` documents additional hardware-dependent
behavior: discovery data may be read from a reserved VRAM TMR region or system
memory, and IP versions/register offsets are then used by later initialization.
The Cobalt64 kext's `loadOnDieDiscovery()` path is coupled to its BAR/MMIO
bring-up (`Navi48Bringup.cpp:486,7361-7366`). Thus parser success is only a
format-level prerequisite. The exact Navi 33 IP list, checksums, die layout,
harvest state, and register bases remain unverified absent a trusted card dump
or target-system read-only capture.

### Firmware, memory, queues, interrupts, and display

- **Firmware:** The existing Cobalt64 Makefile hard-requires Navi 48 GFX 12,
  PSP 14, SDMA 7, and SMU 14 blobs. Pinned Linux GFX 11 source lists the
  `gc_11_0_2_*` GFX firmware names; Linux PSP 13 and SDMA 6 sources list
  generation-specific alternatives. The M2.1
  [IP and firmware evidence map](RX7600_IP_FIRMWARE_MAP.md) records the
  version-derived Linux request paths, request conditions, and exact pinned
  source locations. These are conditional Linux code paths, not the exact
  firmware set reported by the physical RX 7600 or evidence of macOS
  compatibility. Firmware binaries are not in this checkout;
  `BUILDING.md:22-41` says the Navi 48 firmware has a separate AMD licence and
  must be supplied locally.
- **Memory management:** Cobalt64's source has Navi 48 GMC/GART and VRAM
  allocation paths (`src/navi48-bringup/src/amd/gmc_v12_0.cpp`,
  `amdgpu_gart.cpp`, `amdgpu_vram.h`). No Navi 33-specific memory-controller,
  VRAM topology, BAR sizing, or page-table validation exists in this target.
  Similar GART concepts do not prove compatible register programming.
- **Command submission:** The present ladder uses Navi 48 CP/MES/GFX12 code and
  tests a GFX12 PM4 write/fence and compute dispatch (`amdgpu_init.h:45-51`;
  `cp_v12_0.cpp`, `mes_v12_1.cpp`, `gfx_v12_0.cpp`). No gfx1102 ring/queue path
  has been implemented or tested in this repository.
- **Interrupts:** The bring-up's IH implementation is `ih_v7_0.cpp`, attached
  to the Navi 48 ladder. The RX 7600 interrupt source selection, MSI delivery,
  ring layout, and teardown behavior are unverified. Linux source should be
  used to identify the exact Navi 33 IP revision and corresponding handlers
  before designing a macOS port.
- **Display initialization:** Existing Cobalt64 helpers are DCN 4.1. The RX
  7600's exact DCN revision, PHY/connector configuration, VBIOS data, mode
  programming, hotplug, link training, and recovery path were not determined
  for the user's specific card. This is a separate generation-specific blocker.
- **Acceleration / Metal:** The repository describes a Navi 48 native path
  based on Mesa RADV plus a Metal bundle and reports results on one RX 9070 XT
  test system (`README.md:33-52,72-76`). These are repository claims, not
  independently reproduced in this task. Neither those results, parser tests,
  nor compilation can establish RX 7600 acceleration or Metal support. The
  optional Apple accelerator experiment is a distinct, documented
  Navi 48-specific risk; it must not be used as a fallback for Navi 33.

### macOS and build constraints

`src/navi48-bringup/Makefile:1-10,102-109` targets an x86_64 kernel extension,
uses a macOS SDK (`xcrun`) and external MacKernelSDK, and declares macOS 11 ABI
minimum. `BUILDING.md:3-5,11-20,43-47` warns that the kext directly writes
hardware registers and that a wrong/different card can hang or restart the
machine or corrupt display state. The README reports its current test machine
as macOS Tahoe 26.6.2, but this research did not reproduce that claim.

This session runs on Windows 11. Python 3.12 is available; `clang`, `clang++`,
`make`, `xcrun`, and `codesign` are unavailable. The checked-out project lacks
the external MacKernelSDK, required firmware directory, and kext build output.
Consequently, no macOS kext build, signing, load, or target-system test was
possible or attempted. The CPU is user-reported as a Ryzen 5 7500F with no
iGPU, so a black screen or hang would have no CPU-graphics fallback. A
separate isolated macOS installation plus a tested recovery path is a
prerequisite for any future hardware experiment; none was performed.

### Licensing and source reuse

No upstream source code or firmware was copied during this research. The
repository's `src/navi48-bringup/NOTICE` says its discovery/BAR/VRAM survey
originates from RDNA4FB (BSD 3-Clause), its bring-up sequences derive from
mac-amdgpu (MIT), and Linux amdgpu is used as documentation/reference only,
with no GPL code copied into that kext. `src/navi48-bringup/LICENSE` is
BSD-3-Clause; the repository root `LICENSE` is MIT. Mesa patches target the
pinned Mesa commit recorded in `mesa-patches/README.txt`; the optional
metal2vulkan integration is separately LGPL-3.0-or-later per root `README.md`.
Any future code reuse must be reviewed file-by-file, retain all notices and
meet the source licence; firmware blobs have separate licensing and are not
covered by these source-code licences.

## RX 7600 feature matrix

Statuses describe the **RX 7600 target**, not the Navi 48 development card.
“Implemented and tested” is intentionally not assigned to any RX 7600 feature.
Where a Navi 48 implementation exists, it is identified separately and is
“implemented but not tested on target”; it is not treated as RX 7600 support.

| Feature | RX 7600 status | Existing Navi 48 baseline / missing acceptance evidence |
|---|---|---|
| Device identification | **Not implemented** | Navi 48 personalities accept `1002:7550` / `1002:7551` only. Pinned pci.ids maps `1002:7480` to a product group, but no isolated RX 7600 driver target exists. |
| IP Discovery | **Partially implemented** | Offline strict parser and synthetic tests exist. No trusted RX 7600 capture, live target integration, or verified Navi 33 register-base table. |
| Firmware | **Not implemented** | Navi 48 baseline requires Navi 48 blobs. Linux references list GFX/PSP/SDMA generation alternatives, but exact RX 7600 selection and permitted firmware provenance are open. |
| Memory management | **Not implemented** | Navi 48 GMC/GART implementation exists; no RX 7600 BAR/VRAM/page-table support or target acceptance test. |
| Command submission | **Not implemented** | Navi 48 CP/MES/PM4/compute implementation exists; no gfx1102 ring/queue implementation or target test. |
| Interrupts | **Not implemented** | Navi 48 IH v7 implementation exists; no verified RX 7600 IH/MSI setup, event processing, or unload/recovery test. |
| Display initialization | **Not implemented** | Navi 48 uses DCN 4.1; RX 7600 display IP and board routing are not established. |
| GPU acceleration | **Not implemented** | The repository reports Navi 48 acceleration on RX 9070 XT; no RX 7600 hardware validation. |
| Metal integration | **Not implemented** | The repository's Metal/RADV path is Navi 48-specific; no RX 7600 device discovery or Metal workload evidence. |
| RX 7600 macOS build/validation | **Blocked** | Windows environment lacks `clang`, `make`, `xcrun`, `codesign`, MacKernelSDK, required firmware, and target macOS installation. |

## Milestone M2.0 acceptance and outcome

Acceptance criteria were to inspect project identity/matching, initialization
order, generation-specific firmware/modules, build dependencies, target
limitations, licensing, and hardware-safety risks; produce a feature matrix
and evidence-backed next-step gate; run the available offline parser suite;
and make no driver or hardware changes. These criteria were met as research
only. RX 7600 macOS support is not complete.

## Milestone M2.1 — IP and firmware source map

M2.1 adds the detailed
[RX 7600 IP and firmware evidence map](RX7600_IP_FIRMWARE_MAP.md), pinned to
Linux commit
`d0c647a6aae2ce577372594a176c82733e1ed267` and PCI ID database commit
`22da7386ba818c037894f258248734770757b14f`. The map identifies Linux's
version-driven IP dispatch and firmware request patterns for GC/GFX, GMC/UMC,
SDMA, OSSSYS/IH, PSP, SMU, VCN/JPEG, DMUB and MES, with per-source request
conditions.

**Evidence boundary:** no exact IP Discovery values, IP instance inventory, or
complete firmware set has been verified for the user's physical RX 7600. The
listed version branches are supported Linux paths, not a target inventory.
The task's gfx1102 label remains user-supplied rather than independently
established here. Firmware binaries were not accessed, and Linux firmware
requests do not establish macOS compatibility or legal use. M2.1 made no
driver or hardware changes.

The M2.1 offline verification command was `python -B test_ipdiscovery.py` from
`tools/ipdisc/`; the observed result was `Ran 22 tests in 0.314s` followed by
`OK`, with no failures or skips. M2.2 reran the suite; its final observed result
is recorded below. The detailed chronology and final Git state are recorded in
`WORKLOG.md` and `RESUME.md`.

## Milestone M2.2 — offline evidence audit

The M2.2
[offline evidence-gap report](RX7600_EVIDENCE_GAPS.md) records an audit of
repository-local binaries, dumps, logs, parser fixtures, and hardware-report
references. No trustworthy, board-specific RX 7600 IP Discovery artifact or
hardware report was found. The only on-disk `.bin` files are shader payloads
under `src/navi48-bringup/shaders/`; the parser's 22 fixtures are generated
synthetically in memory. No separate artifact was supplied with the task.

The report does not change M2.1's source findings: Linux IP version dispatch
branches are not measurements of this card. Exact board IP records, subsystem
IDs/VBIOS attribution, firmware binary provenance/licensing, and macOS
compatibility remain unresolved. Parser success validates supported structure
only, not artifact authenticity or board identity. The offline suite passed
22 tests (`Ran 22 tests in 0.331s`, `OK`); M2.2 made no parser or driver
changes and performed no hardware or firmware access.

### Proposal gate before any driver-source edit

**No driver-source implementation is authorized or included in M2.0, M2.1, or
M2.2.** Before editing a driver file, the next implementation proposal must define a
milestone, measurable offline acceptance criteria, an isolated Navi 33
initialization boundary, all files to add/change, tests, and a recovery plan.
It must explicitly exclude routing `1002:7480` into the existing Navi 48
personality or initialization. The exact implementation file list is not yet
defensible because the RX 7600's live IP Discovery data and exact per-IP,
firmware, and display mapping are unknown.

The safest next action is to obtain, if available, an existing trusted
RX 7600 IP Discovery artifact with documented board provenance for offline
analysis. Do not probe the physical GPU. If no such artifact is available,
retain the unresolved version mapping. See the M2.2 evidence-gap report for
artifact attribution criteria. Keep `1002:7480` absent from Cobalt64
personalities until a separate proposal, evidence review, and required
isolated recovery environment exist.

## Sources

All Linux URLs below are pinned to
[`torvalds/linux` commit `d0c647a6aae2ce577372594a176c82733e1ed267](https://github.com/torvalds/linux/commit/d0c647a6aae2ce577372594a176c82733e1ed267).
GitHub's commit API reports this commit's subject as “drm/amdgpu/discovery:
support new discovery binary header”, author date 2025-11-07, committer date
2026-03-11. These references support format and code-family research only;
they do not imply Linux driver code was copied.

- [AMD IP Discovery definitions: `discovery.h`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/include/discovery.h)
- [Linux discovery data acquisition and version handling: `amdgpu_discovery.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c)
- [GFX 11 firmware names and GFX 11 implementation: `gfx_v11_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c)
- [PSP 13 firmware version families: `psp_v13_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/psp_v13_0.c)
- [SDMA 6 firmware names and queue implementation: `sdma_v6_0.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/sdma_v6_0.c)
- [Pinned PCI ID database at `pciutils/pciids` commit `22da7386ba818c037894f258248734770757b14f`](https://github.com/pciutils/pciids/blob/22da7386ba818c037894f258248734770757b14f/pci.ids) (`1002:7480`, line 4238). It identifies a family/product group and is not an AMD board-specific hardware dump.
- [AMD RX 7600 product page](https://www.amd.com/en/products/graphics/desktops/radeon/7000-series/amd-radeon-rx-7600.html) was attempted but timed out in this environment; no specifications from that page are relied on here.

## Remaining blockers

1. No independently verified RX 7600 IP Discovery binary or per-IP version
   inventory for the user's physical card.
2. No board-specific subsystem/VBIOS, memory map, BAR, display-output, or
   firmware provenance evidence.
3. No Navi 33 macOS kernel-interface or initialization implementation, and no
   isolated macOS test installation/recovery plan.
4. No macOS SDK, Xcode command-line tools, MacKernelSDK, firmware blobs, or
   target hardware in this Windows session.
5. No target-system testing for firmware, memory, command submission,
   interrupts, display, acceleration, or Metal.
