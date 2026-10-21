# RX 7600 macOS source feasibility

**Review date:** 2026-10-20
**Mode:** Bounded public-source verification and local read-only audit
**Target:** PCI `1002:7480`, reported as RX 7600 / Navi 33
**Result:** No verified, board-specific source-and-platform path is ready to
initialize Navi 33. Cobalt64 has reusable parsing and research-model pieces,
and a separate Navi 48 x86 IOKit implementation, but no Navi 33 hardware
driver.

## Pinned mac-amdgpu source and platform compatibility

The previously cited short hash `3bdeed2` resolves through the public GitHub
commit API to
[`lemonade-sdk/mac-amdgpu@3bdeed2de940504ad6bd1bac718d5de2f65ddb83`](https://github.com/lemonade-sdk/mac-amdgpu/commit/3bdeed2de940504ad6bd1bac718d5de2f65ddb83)
(tree `7101e9c18d41b22a77c5d06f452a99f50f5a5e6e`, commit message version
`v0.1.48`, dated 2026-05-22). GitHub reports the commit signature as
**unsigned**. The exact `LICENSE` file at this revision is MIT (blob
`473fec8f4a7313c7417e6e3405c6b770ddde6d4b`). This establishes the referenced
revision and its checked-in license text, not legal advice or independent
authentication of the source author.

The pinned [`project.yml`](https://github.com/lemonade-sdk/mac-amdgpu/blob/3bdeed2de940504ad6bd1bac718d5de2f65ddb83/project.yml)
defines a DriverKit `driver-extension`, uses `SDKROOT=driverkit` and
`PCIDriverKit.framework`, targets arm64 only, and sets macOS 26.2 /
DriverKit 25.2 deployment targets. The upstream
[`dext/MacAMDGPU.iig`](https://github.com/lemonade-sdk/mac-amdgpu/blob/3bdeed2de940504ad6bd1bac718d5de2f65ddb83/dext/MacAMDGPU.iig)
declares an `IOService` subclass with `Start`, `Stop`, and `NewUserClient`,
including `PCIDriverKit/IOPCIDevice.iig`. Its
[`dext/Info.plist`](https://github.com/lemonade-sdk/mac-amdgpu/blob/3bdeed2de940504ad6bd1bac718d5de2f65ddb83/dext/Info.plist)
matches PCI providers and declares a DriverKit user-client. Its
[`dext/README.md`](https://github.com/lemonade-sdk/mac-amdgpu/blob/3bdeed2de940504ad6bd1bac718d5de2f65ddb83/dext/README.md)
describes an R9700 (`1002:7551`) target over Thunderbolt 5 and calls out
Xcode, PCI DriverKit entitlements, a host app, and an activation flow.

Apple's current
[`IOPCIDevice` documentation](https://developer.apple.com/documentation/pcidriverkit/iopcidevice)
describes it as the DriverKit PCI provider for configuration-space and
memory-mapped I/O; the endpoint driver is responsible for enabling PCI Memory
Space and Bus Master settings when configuring the device. Apple's
[`IOService` documentation](https://developer.apple.com/documentation/driverkit/ioservice)
documents DriverKit service lifecycle and `NewUserClient`. These are real
DriverKit integration interfaces, but they do not document a GPU
acceleration, Metal, or AMD graphics userspace ABI.

The upstream root README reports that R9700 hardware reached its listed
initialization stages, while still listing first real PM4 submission,
production BO/userspace ABI, and Mesa winsys as future work. The same pinned
revision's `dext/README.md` still calls the dext a Phase 1A skeleton without
DMA/MSI-X. These project-authored status descriptions are inconsistent and
are not independent test evidence; the commit message reports an SDMA ring
submission but is unsigned. Treat the extent of upstream runtime validation
as a project claim, not independently verified proof.

The upstream source inventory contains generation-specific modules including
`dext/amdgpu/gfx_v12_0.cpp`, `gmc_v12_0.cpp`, `ih_v7_0.cpp`,
`sdma_v7_0.cpp`, `smu_v14_0.cpp`, and `psp_v14_0.cpp`. These are not Navi33
implementations. The root README's mention of GFX11 firmware does not prove
that this revision implements or runs an RX 7600 / Navi33 code path.

Cobalt64's own `src/navi48-bringup/Makefile` targets x86_64 and builds an
IOKit kext with `-DKERNEL_PRIVATE`, `-DDRIVER_PRIVATE`, `-fapple-kext`, and an
external MacKernelSDK. Its `Info.plist` PCI matches and
`Navi48Bringup::probe()` are limited to `1002:7550`/`1002:7551`. Thus the
upstream DriverKit code is not an API-compatible drop-in for the existing
Navi48 kext, and neither path is evidence of Navi33 support. Local Git blob
hashes for Cobalt64's `amdgpu_init.cpp`, `sdma_v7_0.cpp`, `gfx_v12_0.cpp`,
and `amdgpu_gart.cpp` differ from their pinned upstream blobs; the local
files are adaptations, not verbatim copies.

The upstream commit and license reference are now resolved. The separate
RDNA4FB parser provenance remains unverified: the checked-in notice identifies
the project and BSD-3-Clause but no exact upstream revision or source blob.
The kernel source files remain GPL-2.0 references only per the checked-in
third-party license record; no additional kernel implementation was copied.

Existing provenance reports record narrow, previously retrieved Linux
source evidence at
`torvalds/linux@d0c647a6aae2ce577372594a176c82733e1ed267`. This audit uses
those reports as existing local records; it did not re-fetch upstream files.
The useful verified facts are generic Linux behavior:

- `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` selects discovery tables
  by reported format version and dispatches generic IP implementations by
  reported IP version. The recorded sections include
  `amdgpu_discovery_reg_base_init()` and the IP-block dispatch functions.
- `drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c` has a version-derived
  SMU firmware request with an SR-IOV VF early return and kicker/non-kicker
  filename branches in `smu_v13_0_init_microcode()`.
- The recorded Linux switches include GFX11, GMC11, IH6, SDMA6, SMU13,
  PSP13.0.4 and selected DCN/VCN/MES versions. See
  `RX7600_REMAINING_SOURCE_CLAIMS.md` for exact URLs, line ranges, blob IDs,
  hashes, and explicit claim limits.

These Linux facts describe Linux source dispatch and firmware request
behavior. They do not identify the user's board's IP Discovery records,
establish that any listed firmware is required by it, or provide macOS IOKit,
DriverKit, Metal, or userspace interfaces. No board-specific or
target-tested evidence exists in this review.

## Subsystem compatibility matrix

| Subsystem | Checked-in Cobalt64 evidence | Upstream evidence and license boundary | Feasibility / remaining work |
|---|---|---|---|
| GPU discovery and architecture identity | `src/navi48-bringup/src/Navi48Bringup.cpp`: `Navi48Bringup::probe()` checks AMD and IDs `0x7550`/`0x7551`; `loadOnDieDiscovery()` and `surveyIPs()` feed its Navi48 path. `src/navi33-research/` is an offline identity/evidence model only. `tools/ipdisc/ipdiscovery.py` parses supplied bytes, not hardware. | Linux `amdgpu_discovery.c` at the pinned commit dispatches generically from reported discovery/IP versions. Linux `pci.ids` is an aggregated label, not a card measurement. Parser provenance is attributed to RDNA4FB/BSD-3 in `NOTICE`, but its exact upstream revision was not verified. | **Potentially reusable parsing/model logic; macOS-specific discovery/provider path required; board evidence missing.** Obtain a provenance-documented artifact first; do not assign Navi33 versions from generic switches. |
| Firmware and PSP | Navi48 requires `psp_14_0_3_*`, GFX12, SDMA7 and SMU14 blobs in `src/navi48-bringup/Makefile`; `src/navi48-bringup/src/psp.cpp` and `src/amd/psp_v14_0.cpp` implement its generation-specific path. | Linux generic discovery/SMU request claims are recorded at the pinned Linux revision. Linux source is GPL-2.0 reference material per `third-party/THIRD-PARTY-LICENSES.txt`; the pinned mac-amdgpu tree is MIT, but its firmware and RDNA4 code do not establish Navi33 requirements. | **Navi33 firmware requirements and exact assets missing.** Linux filename declarations/request logic cannot establish a physical card requirement, license for a blob, or a compatible macOS loader. |
| Graphics engine and command submission | Navi48 uses `src/navi48-bringup/src/amd/gfx_v12_0.cpp`, `cp_v12_0.cpp`, `mes_v12_1.cpp`, and staged `BringupStage::CPInit/GFXInit/MESInit` in `amdgpu_init.cpp`. The code uses GFX12-specific register, packet, MQD and firmware logic. | Exact MIT DriverKit source at mac-amdgpu commit `3bdeed2de940504ad6bd1bac718d5de2f65ddb83` contains GFX12/MES12 code, targeting an R9700/GFX12.0.1 report. Its own README still lists first PM4 submission and Mesa winsys as future work. | **Potentially reusable hardware algorithms only after version/evidence and license review; DriverKit/Xcode/entitlement and a graphics userspace path are distinct work.** No Navi33 command path exists. |
| GMC, GPU VM and VRAM | `src/navi48-bringup/src/amd/gmc_v12_0.cpp`, `amdgpu_gart.cpp`, and `amdgpu_vram.h` implement Navi48/GFX12 memory paths. `amdgpu_gart.cpp` explicitly describes the arm64 DriverKit `IOBufferMemoryDescriptor`/`IODMACommand` path being replaced with x86 IOKit `SysMem` assumptions. | Linux GMC11 is a generic reference recorded in `RX7600_UPSTREAM_SOURCE_VERIFICATION.md`; it is not an implementation contract for XNU or proof of the target's VRAM geometry. | **Generation-specific GPU page-table formats may be useful references; host DMA, IOMMU, memory allocation, cache/coherency and VM APIs require macOS-specific implementation and target validation.** Navi33 memory evidence missing. |
| Interrupts and synchronization | Navi48 uses `src/navi48-bringup/src/amd/ih_v7_0.cpp`, IOKit interrupt event sources, rings/fences, and explicit stage state. | Linux IH6 dispatch is generic source evidence only. | **macOS interrupt registration, delivery, synchronization and recovery remain platform work; Navi48 IH7 is not evidence for Navi33 IH6.** |
| SDMA | `src/navi48-bringup/src/amd/sdma_v7_0.cpp` implements the Navi48 SDMA7 path and includes test dispatch in `amdgpu_init.cpp`. | Linux SDMA6 version dispatch and firmware declarations are recorded for the pinned commit; no target's actual SDMA version/count has been measured. | **Navi33 engine version/count and firmware unknown; packet/register logic and macOS queue, DMA and synchronization interfaces need separate validation.** |
| SMU and power management | `src/navi48-bringup/src/amd/smu_v14_0.cpp` is SMU14-specific; its full clock/power setup is opt-in in the stage context. | Pinned Linux SMU13 request behavior is recorded in `RX7600_REMAINING_SOURCE_CLAIMS.md`, not as a macOS ABI or proof of the target's firmware. | **No Navi33 SMU or power path verified.** Requires board IP/firmware evidence and a macOS power-management integration plan; do not infer from filenames. |
| Display Core and connectors | `src/navi48-bringup/src/dcn/navi48_dcn.cpp` and `src/navi48-bringup/src/Navi48DisplayNub.cpp` implement Navi48 display-facing experiments and IOKit services. | Pinned Linux discovery source has generic display-version branches; Linux DRM/KMS/DCN interfaces are not Apple framebuffer/AGDC contracts. | **Navi33 display IP/PHY/connector data absent.** Apple framebuffer, display, and connector services need separately verified interfaces and board topology. |
| Kernel and userspace graphics integration | Navi48 is an x86_64 IOKit kext: `src/navi48-bringup/Makefile` sets `-DKERNEL_PRIVATE`, `-DDRIVER_PRIVATE`, `-fapple-kext`, links the MacKernelSDK, and `Info.plist` publishes IOKit personalities. `BUILDING.md` specifies external SDK/firmware prerequisites. | Apple documents `DriverKit::IOService`/`PCIDriverKit::IOPCIDevice`; the pinned mac-amdgpu project uses a DriverKit dext, PCIDriverKit framework, arm64-only target, Xcode, PCI transport entitlement, host app, and user client. | **Two materially different integration models exist in the source: private x86 IOKit kext versus arm64 DriverKit dext.** A Navi33 path must choose and validate its target platform and obtain entitlements; neither existing project demonstrates AMD GPU userspace/Metal integration for Navi33. |
| Metal and acceleration | The repo contains separate Navi48 acceleration/display experiments and `tools/native` components; these are not evidence of a supported RX 7600 Metal stack. The isolated Navi33 model sets initialization/integration to unsupported. | Linux/Mesa source cannot establish Apple Metal compatibility. No pinned and audited Navi33-to-Metal interface source is recorded here. | **Blocked by absent Navi33 kernel integration, userspace ABI, shader/compiler/runtime evidence, and target tests.** No acceleration or Metal claim is justified. |
| Build and licensing | `src/navi33-research/CMakeLists.txt` builds only the research model; `src/navi48-bringup/Makefile` targets x86_64 and requires Apple-specific SDK tools, MacKernelSDK and firmware. This environment lacks CMake and C++ compilers. | `NOTICE` and `third-party/THIRD-PARTY-LICENSES.txt` record BSD-3, MIT and GPL reference boundaries. The mac-amdgpu commit and MIT license are pinned above; this does not establish compatibility of its DriverKit code with the x86 IOKit target. | **Navi33 model compile is unverified here; production kext build requires macOS/Xcode-compatible tooling and SDK inputs.** Hardware-source reuse still requires Navi33-specific evidence and license review. |

## Candidate architecture comparison

| Candidate | Fit for x86_64 macOS and this repository | Concrete blockers | Decision |
|---|---|---|---|
| Separate Navi33 implementation inside Cobalt64's existing Navi48 IOKit kext | Shares the repository's x86_64 `IOKit` provider/service build shell. `Navi48Bringup::probe()` and `start()` now both gate the two Navi48 IDs before hardware setup. | Navi48 IP/firmware and register sequences are GFX12/GMC12/IH7/SDMA7/SMU14/PSP14 and must not be reused as Navi33 logic. A separate class, lifecycle state, IP blocks, firmware policy, memory/VM and queue paths would still need actual Navi33 evidence; the kext uses private APIs and cannot be compiled in this environment. | **Best provisional integration shell only**, if a separately validated x86_64 IOKit target is chosen. It is not a driver base whose Navi48 initialization can be extended by adding `1002:7480`. |
| New standalone x86_64 macOS driver | Could keep the target's lifecycle and failure domain separate from the large Navi48 experiment. | Would duplicate PCI/service/kernel integration and still require an x86_64 SDK, signing/loading path, and private or otherwise unavailable graphics integration. No local C++/Apple build tools or supported test host are available. No verified public Apple API for third-party GPU acceleration/Metal was found. | **Not preferable today**: a larger platform rewrite without closing the GPU/userspace interface gap. Reconsider only if a minimal x86_64 service build proves isolation or deployment requires it. |
| Adapt an existing open-source macOS graphics project | WhateverGreen at [`093e76fec5655001f38b367dd22459c756ee89fa`](https://github.com/acidanthera/WhateverGreen/tree/093e76fec5655001f38b367dd22459c756ee89fa) is BSD-3-Clause and has `RAD::processKernel`, `RAD::processKext`, `RAD::processHardwareKext`, and hardware-index tables for Apple Radeon kexts (`kern_rad.hpp/.cpp`). It is a patching/compatibility layer around Apple's existing AMD stack, not an independent AMD GPU driver. NootedRed at [`9ff85a6636a3f01d3d1dc02ae09d057605b76c3c`](https://github.com/ChefKissInc/NootedRed/tree/9ff85a6636a3f01d3d1dc02ae09d057605b76c3c) describes itself as glue for existing AMD drivers, but its checked-in license explicitly restricts AI use; do not adapt it. | WhateverGreen depends on Apple hardware kexts and private symbols and does not implement the missing Navi33 PCI/IP, firmware, memory, queue, interrupt, or display stack. NootedRed's license is not a suitable permission basis for this AI-assisted reuse. Linux amdgpu's GPL-2.0 implementation is reference-only under the repository's recorded license boundary; Mesa's RADV requires a DRM kernel interface absent here. | **No drop-in compatible implementation found.** WhateverGreen may be relevant only after an Apple driver exists for the target; do not infer one from spoofed IDs or property patches. |
| No viable implementation supported by current evidence/interfaces | Matches the evidence state: no board-attributed IP Discovery artifact, no demonstrated Navi33 macOS stack, and no build/integration validation. | Missing board inventory/firmware provenance; no verified macOS AMD graphics userspace ABI; no available x86_64 Apple toolchain/test host. A public PCI provider API alone does not implement a GPU stack. | **Selected current conclusion for a functioning driver.** Keep a Navi33-only integration shell as a guarded future direction, but defer hardware logic until those blockers are removed. |

The source-backed status distinction matters: WhateverGreen's existing
hardware-index hooks and device/property patches can adapt an Apple-provided
driver but cannot substitute for the driver's generation-specific hardware
support. The `NootedRed` candidate was rejected without code reuse because its
published license explicitly prohibits AI use; no conclusion about its
technical merits beyond its own README scope is needed.

### Smallest experiments for the unresolved architecture gates

1. **x86_64 kernel/service viability:** In an authorized Intel macOS build
   environment, compile and link a no-MMIO `IOService`/`IOPCIDevice` test
   service against the exact intended macOS SDK/MacKernelSDK. This resolves
   toolchain and service ABI viability only; it does not test a GPU.
2. **Board/IP and firmware requirements:** Obtain a user-provided,
   hash-preserved, provenance-documented RX 7600 IP Discovery artifact and
   parse it offline in strict mode. This cannot itself authenticate the
   artifact's board provenance.
3. **Graphics integration:** Before implementing queues or display, identify
   an authoritative Apple interface/provider contract for third-party GPU
   acceleration and a compatible userspace client. Public PCI DriverKit
   documentation alone is insufficient. If no such contract exists, the
   deliverable may be a bring-up/compute research driver, not Metal support.

## Dependency order and smallest credible vertical slice

Do not begin with MMIO or initialization. The minimum evidence-led sequence is:

1. Obtain a genuine, provenance-documented IP Discovery artifact and preserve
   its original hash. Parse it offline in strict mode; this identifies only
   the contents of that artifact, not automatically its physical source.
2. Independently establish that the artifact belongs to the target board.
   Only then map observed IP records to versioned implementation requirements
   using pinned upstream sources and license review.
3. Build a hardware-independent, tested dispatch model from those observed
   records. Keep unknown or unsupported IP blocks explicit; do not link
   Navi48/GFX12 implementations as fallbacks.
4. In a separately prepared and authorized macOS development environment,
   implement a single isolated OS-facing integration boundary and validate
   compile/link behavior before any hardware operation.
5. Treat firmware, memory/VM, command submission, interrupts, display,
   power, and userspace/Metal as dependent vertical slices with individual
   evidence and recovery gates.

The best first hardware-independent vertical slice remains step 3, but its
input artifact is missing. The mac-amdgpu revision is now pinned, but it is
RDNA4/DriverKit source and does not resolve Navi33's version map. A mapping
written from those unrelated inputs would encode assumptions, so this pass
does not add Navi33 IP or firmware logic.

## Decision

- **Verified locally:** Cobalt64 has a Navi48-only IOKit path and an isolated
  Navi33 research model. Navi48 PCI matching and runtime checks exclude
  `1002:7480`.
- **Verified against public pinned sources:** The old `3bdeed2` citation
  resolves to mac-amdgpu commit
  `3bdeed2de940504ad6bd1bac718d5de2f65ddb83` (MIT text present; unsigned
  commit). Its documented path is an Apple-Silicon DriverKit dext for the
  R9700/RDNA4; its generation-specific source list is GFX12/GMC12/IH7/
  SDMA7/SMU14/PSP14. Apple documents DriverKit PCI/service interfaces, but
  not an AMD GPU acceleration or Metal ABI.
- **Architecture decision:** No complete driver architecture is currently
  supported by sufficient evidence. If the project proceeds on x86_64, use
  a separate Navi33 class and generation-specific modules behind the
  repository's existing IOKit service shell; keep all Navi48 routines and
  IDs Navi48-only. This is a provisional integration choice, not a claim the
  existing kext can support Navi33. WhateverGreen is a BSD-3 patch layer for
  Apple Radeon kexts, not an independent GPU implementation; NootedRed is
  not reused due its published AI-use restriction.
- **Recorded as source-verified in prior bounded audits:** Narrow generic
  Linux behavior at pinned commit
  `d0c647a6aae2ce577372594a176c82733e1ed267`, with hashes/URLs in the cited
  reports. This audit did not re-fetch that source.
- **Unresolved:** Full upstream provenance of the RDNA4FB-imported parser;
  physical RX 7600 IP
  inventory, firmware requirements and board topology; availability of a
  compatible macOS SDK/compiler environment; and a verified Apple graphics
  userspace/Metal integration contract.
- **Next action:** Obtain a genuine provenance-documented RX 7600 IP Discovery
  artifact; independently prove x86_64 service build feasibility in an
  authorized macOS SDK environment. Keep `1002:7480` on the research-only
  route until a separate Navi33 driver path is implemented and verified; do
  not infer support from GFX11 firmware mentions or route it through Navi48.
