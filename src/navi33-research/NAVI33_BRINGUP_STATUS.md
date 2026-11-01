# Navi 33 bring-up implementation boundary

Status reviewed: 2026-10-30

This document describes the first hardware-independent contract layer added for
CherryRed. It is not a Radeon driver and does not enable RX 7600 acceleration.

## Implemented in this change

- Navi33BringupModel validates firmware metadata shape, including a bounded
  image length, a SHA-256-shaped digest string, explicit provenance, and an
  approved-license state. It does not hash, authenticate, locate, load, or
  redistribute firmware. A 64-character digest is only syntax validation.
- GpuVirtualAddressAllocator is a host-side range arithmetic model. It checks
  power-of-two alignment, integer overflow, and aperture containment. It does
  not allocate VRAM, configure GART, or program GPU page tables.
- validateQueueGeometry checks conservative ring geometry constraints.
  QueueIndexModel models producer/consumer arithmetic in host memory. It does
  not create a hardware queue, write a doorbell, submit packets, or provide
  GPU synchronization.
- Unit tests cover accepted/rejected metadata, range overflow, alignment,
  aperture exhaustion, queue geometry, full/empty conditions, and reuse.

All of these APIs are deterministic host-side code. They are deliberately
separate from Navi48Bringup.kext and contain no IOKit, DriverKit, MMIO, PCI
configuration writes, firmware-loading, or GPU command submission.

## Six requested driver layers: actual status

| Layer | Current implementation status |
|---|---|
| Navi33 hardware backend | Not implemented. Existing DriverKit code only opens the matched PCI provider and queries BAR metadata; it is not an AMD backend. |
| Firmware bring-up | Not implemented. Only metadata validation contracts exist; no firmware files are loaded or sent to PSP/SMU/GC/SDMA/MES. |
| Memory and queues | Host-only range allocator and ring-index model added. No VRAM/GART mapping, GPU page tables, queue registers, fences, or doorbells. |
| IOAccelerator | Not implemented. No accelerator service, user client, command submission path, or macOS graphics-family integration. |
| Metal | Not implemented. No Metal plugin, shader compiler/runtime, AIR translation, or registered Metal device. |
| Display | Not implemented. No display engine initialization, framebuffer, AGDC, modesetting, or WindowServer integration. |

## Evidence and gates

The firmware descriptor's sha256 field is validated for hexadecimal syntax
and length only. The model does not compute or verify a digest. Provenance and
license states are caller-provided assertions, not cryptographic or legal
verification. A real implementation must obtain firmware provenance, confirm
the exact Navi 33 ASIC/IP versions on the target board, and complete a human
license review before any firmware use.

The queue and address models use synthetic host values. Passing tests does not
prove the RX 7600's GPU virtual address format, alignment requirements, ring
geometry, or firmware ABI. Those must be sourced from compatible AMD
documentation/upstream implementations and validated against board evidence.

## Not attempted

No EFI/config.plist changes, no removal of -radvesa, no SIP/AMFI changes,
no kernel extension or DriverKit installation/activation, no firmware writes,
no PCI/MMIO register writes, and no reboot were performed.

A real driver still needs a separately reviewed, signed and entitled transport,
a validated Navi 33-specific firmware and initialization path, memory and queue
management, IOAccelerator/Metal integration, display integration, and
hardware tests with a tested recovery path. Do not load the Navi 48 bring-up
code against device 1002:7480.

## Validation performed on the connected Mac

- Apple Clang host test script: passed (35 research checks, 32 lifecycle
  checks, 21 new bring-up model checks, and IP Discovery bounds tests covering
  8 fixtures plus 512 malformed inputs).
- Navi 48 isolation static tests: 4 passed.
- DriverKit PCI probe static contract tests: 4 passed.
- DriverKit build-only check: passed; unsigned x86_64 DriverKit bundle built
  against SDK 25.5. It was not signed, installed, activated, or run.
- git diff --check: passed.
- CMake configure/build was not run because the host does not have the cmake
  executable installed. The direct Apple Clang test script above is the
  validation actually completed.

## Additional integration contracts added after the first test pass

- Navi33IntegrationContracts now models the strict dependency gate across PCI,
  firmware, memory, queue submission, IOAccelerator, Metal, and display. Only
  HardwareVerified satisfies the full-readiness check; Prototype,
  HostModelOnly, and ImplementedUnverified are intentionally rejected.
- The display layout calculator validates dimensions, refresh, pixel format,
  row alignment, pitch arithmetic, and total frame size. This is a CPU-side
  linear-layout calculator, not a framebuffer or modesetting implementation.
- FenceTimelineModel models monotonically increasing completion values on the
  host. It is not a GPU fence, interrupt, or synchronization primitive.

Additional tests: 16 integration-contract checks passed. The complete host
script now passes all C++ test groups and both Python isolation/probe suites.

These contracts make missing production work explicit; they do not implement
the hardware-facing layers. In particular, an IOAccelerator/Metal/display
readiness flag can only be marked HardwareVerified after real target tests,
not by changing a configuration value or passing a unit test.


## GFX11 backend foundation (2026-10-30)

Added `Navi33Gfx11Manifest` as the first isolated GFX11-specific backend gate.
It validates the exact intended PCI/subsystem identity, requires board-attributed
evidence and a matched VBIOS identity, rejects non-GFX11 major IP families,
checks firmware filename prefixes against the declared GC/PSP/SMU/SDMA versions,
and requires bounded records with SHA-256 syntax, upstream commit syntax,
board attribution, license approval, and unique required firmware roles.

The new manifest tests use deliberately synthetic fixture values to exercise
accept/reject logic. They are **not** a measured RX 7600 IP inventory, actual
firmware hashes, legal approval, or evidence that the candidate versions are
correct for this board. The validator does not hash files or authenticate
metadata; it validates caller-provided fields only.

Validation on the connected Mac: the new GFX11 manifest suite passes 10 checks
(including rejection of a GFX12 major family, wrong subsystem, missing VBIOS
match, Navi48/GFX12 firmware prefix, unapproved license, generic provenance,
missing role, and duplicate role). The complete host script passes, along with
the existing parser, Navi48 isolation, and DriverKit probe contract suites.
`git diff --check` passes.

**Still not implemented:** GFX11 PSP/SMU/GC initialization, register access,
firmware loading, VRAM/GART/VMID, hardware queue setup, PM4/IB construction,
doorbells, fences/interrupts, GPU reset/recovery, IOAccelerator, Vulkan/RADV,
or Metal integration. No hardware initialization was attempted. The current
Mac still has no Metal device, and RX 7600 Metal compute is not supported.


## Four GFX11-to-Metal workstreams: host-side foundations (2026-10-30)

Implemented a hardware-independent foundation for the four requested areas:

1. **GFX11 initialization sequence** — ordered, evidence-gated state machine:
   exact PCI/VBIOS identity -> firmware manifest -> PSP -> SMU -> GC/MES ->
   SDMA -> memory/GPUVM -> compute queue -> hardware compute verification.
   It is a model only; it does not execute any initialization command.
2. **VRAM/GART/GPUVM/VMID** — bounded region and mapping validation for
   alignment, address overflow, physical aperture bounds, VMID range, and
   access permissions. No page tables are allocated and no GPU mappings are made.
3. **GFX11 compute queue** — validates GFX11 pipe/queue geometry, ring pointers,
   monotonic submission sequence, ring capacity, and fence completion ordering.
   No ring memory is allocated, IB is encoded, doorbell rung, or fence observed.
4. **IOAccelerator/Metal bridge** — evidence-gated service -> user client ->
   accelerator -> Metal device state machine. It cannot publish an IOService or
   advertise a Metal device; it only checks the order/evidence model.

The queue model uses the upstream architectural fact that GFX11 has two GFX
pipes with two queues per pipe and that MES replaces KIQ on GFX11+. This is
generic GFX11 architecture evidence, not confirmation of the RX 7600 board's
exact enabled queue topology. See Linux AMDGPU driver-core documentation.

Validation on the connected Mac:
- New four-workstream host suite: 22 passed, 0 failed.
- Existing suites: research 35, lifecycle 32, bring-up 21, integration 16,
  display 18, compute 18, manifest 10, IP Discovery 8 fixtures + 512 malformed,
  Navi48 isolation 4, DriverKit probe contract 4; all passed.
- `git diff --check` passed.

No hardware initialization was attempted. The four workstreams are foundations,
not implemented GPU support. Missing prerequisites remain board-attributed
IP Discovery/VBIOS, verified firmware provenance/hashes/license, hardware
register/firmware protocol evidence, real queue/memory integration, a safe
recovery path, and an actual kernel/user-client accelerator implementation.
RX 7600 Metal compute remains unavailable on the current macOS host.


## RX 7600 Metal hardware acceptance probe (2026-10-30)

Added `metal-compute/RX7600MetalComputeProbe.swift` and
`metal-compute/test_rx7600_metal.sh`. The probe enumerates Metal devices,
requires an IOAccelerator registry entry whose registry ID matches the Metal
device, walks to its PCI ancestor, and accepts only the exact target identity
`1002:7480 / 148c:2421`. It then compiles the vector-add pipeline, allocates
Metal buffers, submits a dispatch, and verifies 4096 output elements. It has
no CPU fallback and does not accept a different GPU as proof of RX 7600 support.

Validation on this Mac:
- RX7600-specific Swift probe compiled successfully with Metal and IOKit.
- Runtime exited with code 2: `MTLCopyAllDevices() returned no devices`.
- Therefore no PCI-to-Metal registry match or GPU dispatch was possible.
- No kext/DriverKit extension was installed or activated; no firmware/MMIO,
  reset, EFI, SIP, or AMFI changes were made.

The probe improves the final acceptance test but does not create an AMD driver.
The upstream Cobalt64 roadmap currently marks RX 7600/Navi 33 as Planned, not
supported. Its working implementation is for a different GPU generation.
Board-attributed IP Discovery/VBIOS and firmware evidence are still absent
from the inspected project and Mac home directories. No hardware initialization
or Metal acceleration is claimed.


## Apple Tahoe driver-match audit (2026-10-30)

Read-only audit added at `tools/diagnostics/audit_apple_amd_gpu_support.py`.
It parsed the installed Tahoe 26.7.1 metadata for:
- `AMDRadeonX6000.kext`
- `AMDRadeonX6000Framebuffer.kext`
- `AMDRadeonX6000HWServices.kext`

All three report **NO** PCI personality match for RX 7600 `1002:7480`.
The accelerator personalities in the installed X6000 bundle cover Navi 10,
12, 14, 21 and 23 device IDs; none includes Navi 33 `7480`. The framebuffer
and HW-services match tables likewise omit `7480`.

Runtime corroboration on this Mac:
- `MTLCopyAllDevices()` returned an empty array.
- `ioreg -r -c IOAccelerator -l` returned no service.
- `kmutil showloaded` showed Lilu, WhateverGreen and AMDSupport, but no
  `AMDRadeonX6000` accelerator instance.

This identifies the immediate reason Apple’s stock driver does not attach:
the installed driver personality tables do not match this GPU. Adding only
`1002:7480` to a match list would not implement Navi 33 register programming,
firmware sequencing, memory management, GFX11 queue submission, framebuffer
bring-up, or the private IOAccelerator/Metal driver interface. Spoofing the
device ID or forcing a match is therefore not an acceleration fix and is not
safe to attempt.

The public Dortania GPU guide also lists Navi 3x / RX 7600 as unsupported,
while Apple's eGPU guidance states third-party aftermarket GPU drivers are
not compatible with macOS. These are compatibility constraints, not a
substitute for implementing a new, reviewed driver stack.

Next legitimate engineering gate: a board-attributed IP Discovery + VBIOS
record and a separately reviewed Navi 33 implementation plan covering PSP,
SMU, GC/GFX11, GMC/GPUVM, SDMA/MES, and macOS accelerator integration.
Until that work exists and is hardware-tested, Metal remains unavailable.


## Offline firmware-evidence intake work (2026-10-30)

Started the next safe Navi 33 implementation prerequisite without changing
macOS system state. Added:
- `tools/ipdisc/verify_gfx11_firmware_manifest.py`: bounded, read-only
  manifest verifier for exact RX 7600 PCI/subsystem identity, VBIOS and
  physical IP Discovery files, firmware-role coverage, filename/version
  consistency, byte lengths, SHA-256, and path containment.
- `tools/ipdisc/test_verify_gfx11_firmware_manifest.py`: nine synthetic
  tests covering valid structure/hash binding, wrong identity, wrong hash,
  wrong size, missing firmware roles, wrong prefixes, path traversal,
  symlinks, and unreviewed licence status.
- Documentation in `tools/ipdisc/README.md`.

Verification on the connected Mac:
- Firmware manifest verifier: 9/9 passed (synthetic files only).
- IP Discovery parser: 30/30 passed.
- Linux sysfs collector: 8/8 passed.
- Prior C++ Navi 33 host suites: 35 + 32 + 21 + 16 + 18 + 18 + 10 + 22
  passed, with the IP Discovery bounds, Navi 48 isolation, and DriverKit
  PCI-probe contract checks also passing.
- RX 7600-specific Metal probe compiles, but runtime still returns no Metal
  devices. No IOAccelerator service exists.
- Apple Tahoe X6000 accelerator, framebuffer, and HW-services metadata all
  omit PCI `1002:7480`.

This is evidence-intake tooling, not GPU initialization or acceleration. The
actual board-attributed IP Discovery capture, exact VBIOS, and firmware blobs
are still absent. No Kext/System Extension activation, firmware load, BAR
mapping, MMIO write, GPU reset, EFI change, SIP/AMFI change, or reboot was
performed. Hardware acceleration remains unavailable.

## Native Navi 33 survey kext build verification (2026-10-30)

As a safe prerequisite to a hardware-facing backend, rebuilt the existing
`src/navi33-survey` native x86_64 IOKit kext on the connected Tahoe host.
Compilation/linking, ad-hoc code signing, `codesign --verify --deep --strict`,
and Info.plist validation all succeeded. The output is
`build/navi33-survey/Navi33Survey.kext`.

This is still only the opt-in, read-only PCI survey service. Its `probe()`
requires the exact PCI/subsystem tuple `1002:7480 / 148c:2421` and boot arg
`navi33survey=1`; `start()` reads PCI identity/configuration and assigned BAR
metadata only. It does not map BARs, access GPU MMIO/VRAM, load firmware,
initialize the ASIC, publish IOAccelerator, or expose Metal. The kext was not
installed, activated, or loaded. A `kextutil` dry-run was not completed because
this Tahoe host reports that `kextutil -n` is not a supported `kmutil` mode;
no load attempt was made.

Re-ran the RX7600-specific Metal acceptance executable after the build. It
still exits 2 because `MTLCopyAllDevices()` returns no devices. Thus the
runtime blocker is unchanged: no macOS accelerator service or Metal device is
registered for this GPU.

Next implementation gate remains board-specific IP Discovery and VBIOS
artifacts, then an independently reviewed Navi 33 GFX11 firmware/init path and
native macOS accelerator integration. Do not repurpose Navi48 initialization
or install this survey kext as if it were an accelerator.

## macOS read-only PCI / ROM investigation (2026-10-30)

A further host-side audit was recorded in
`tools/ipdisc/evidence/macos_rx7600_hardware_audit_2026-10-30.md`.
It confirms the exact PCI tuple `1002:7480 / 148c:2421`, revision `0x00cf`,
PCI location `3:0:0`, negotiated PCIe Gen4 x8 (from link-status value
`0x1084`), 8 GiB platform-reported VRAM, and assigned PCI resource descriptors
(256 MiB, 2 MiB, 1 MiB, and 128 KiB). These are enumeration/resource facts only;
no BAR mapping or MMIO access was performed.

Important ROM finding: the `rom@0` node in `IODeviceTree` reports
`vendor=Acidanthera`, `rom-size=16 MiB`, and OpenCore firmware version
`2094.80.5.0.0`. This is OpenCore/Acidanthera boot-firmware metadata, **not**
the RX 7600 VBIOS. No GPU ROM byte payload, VFCT, ATOMBIOS, or AMD IP
Discovery payload was exposed by the inspected macOS interfaces. Do not use
that 16 MiB figure as the card VBIOS size or treat it as a VBIOS dump.

Metal remains unavailable (`MTLCopyAllDevices()` empty); no `IOAccelerator`
service is present. The remaining hardware-evidence gate is still a genuine,
board-attributed VBIOS plus AMD IP Discovery capture, preferably from a Linux
AMDGPU session. No system configuration, boot arguments, EFI, kext loading,
firmware, BAR mapping, MMIO, reset, or reboot was changed/performed.


## Hardware Metal dispatch and AMD IP Discovery follow-up (2026-10-30)

After Metal Toolchain 17F42 was installed, both existing Metal execution
paths were rebuilt and attempted on the connected Mac:

- Generic `build_and_run.sh`: shader, metallib, and Swift probe build PASS;
  runtime exit 2 because `MTLCreateSystemDefaultDevice()` returned nil.
- Exact `test_rx7600_metal.sh`: RX 7600-specific probe build PASS; runtime
  exit 2 because `MTLCopyAllDevices()` returned no devices. No exact PCI-to-
  IOAccelerator ancestry or hardware dispatch could be performed.
- `system_profiler SPDisplaysDataType` still enumerates the RX 7600
  (`1002:7480`, subsystem `148c:2421`, 8 GB), but reports **Kernel Extension
  Info: No Kext Loaded**. PCI enumeration/display presence is not Metal
  acceleration.

No safe application-side change can make a Metal device appear while the
required macOS accelerator driver is absent. Forcing a stock driver match,
spoofing IDs, or loading an unimplemented kext would not complete dispatch and
is not attempted. Hardware Metal dispatch remains **BLOCKED**, not PASS.

Added `tools/ipdisc/capture_rx7600_ip_discovery.sh`, a read-only Linux capture
helper that verifies exact PCI/subsystem identity and amdgpu binding, finds the
matching debugfs blob by PCI BDF, refuses overwrite, records SHA-256 and
provenance, and invokes the strict offline parser. It does not mount debugfs or
access BAR/MMIO/VRAM/firmware/reset interfaces. The connected Mac is Darwin and
cannot run this Linux capture. No physical IP Discovery binary was fabricated
or claimed. Actual capture remains **BLOCKED pending Linux + amdgpu**.

Validation: Metal shader/probe builds PASS; generic and exact runtime dispatch
both correctly fail closed with exit 2; capture script syntax and its macOS
rejection path are checked separately. No kext/DriverKit activation, firmware
load, BAR mapping, MMIO, GPU reset, EFI/SIP/AMFI change, or reboot was done.


## RX 7600 IP Discovery artifact recovered and parsed (2026-10-30)

A long-running read-only search of the Mac's mounted user volume found the
previously captured binary at
`/Volumes/GameAndFile/ALL/Framelpo ALL Project/Hackintosh AMD Driver/Cobalt64/RX7600-Linux-Evidence/amdgpu_discovery.bin`.
It is 10,240 bytes and its SHA-256 exactly matches the previously recorded
Linux capture digest:
`4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`.
The preserved package includes a manifest recording Archcraft, kernel
`7.2.11-arch1-1`, PCI BDF `0000:03:00.0`, `1002:7480`, subsystem
`148c:2421`, and `amdgpu` capture from `/sys/kernel/debug/dri/1/amdgpu_discovery`.
The same package includes a Linux boot inventory with matching PCI identity
and driver binding. These are user-maintained records and are not
cryptographically authenticated; provenance remains UNVERIFIED.

Copied byte-for-byte (without modifying the original) to
`tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin`, with
hash-bound provenance and strict parse JSON sidecars. Strict parser PASS:
10,240-byte input, binary offset 0, die offset 140, 39 IP records, 30 unique
numeric HWIDs. The header-declared binary region is 1,280 bytes. GC is
11.0.2; MMHUB 3.0.2; HDP 6.0.1; SDMA0 6.0.2; UMC instances 0-3 are 8.11.0;
MP0/MP1 13.0.7; NBIF 4.3.1; OSSSYS 6.0.2; DMU 3.2.1. The parser's enum
still labels 20 numeric HWIDs UNKNOWN; source-correlated mapping is recorded
separately and HWIDs 282/286 remain unresolved in the inspected source
snapshot. Do not infer register access from base metadata.

Full analysis: `tools/ipdisc/evidence/RX7600_IP_DISCOVERY_ANALYSIS_2026-10-30.md`.
The artifact is a physical-capture candidate, not independently authenticated
board evidence. Strict parsing proves structure/checksums only.

After Metal Toolchain 17F42 installation, both runtime probes were rerun:
- Generic Metal: shader/metallib/Swift build PASS; runtime exit 2 because
  `MTLCreateSystemDefaultDevice()` returned nil.
- Exact RX 7600: probe build PASS; runtime exit 2 because
  `MTLCopyAllDevices()` returned no devices.
Hardware Metal dispatch remains BLOCKED; no CPU fallback is counted. The
current macOS system profiler reports RX 7600 PCI presence but `No Kext Loaded`.
No safe application-side fix can create a missing accelerator driver.


## IP Discovery HWID naming closure — 2026-10-30

Expanded the offline parser's `HwId` enum with the 18 additional numeric names cross-referenced in the preserved Linux source snapshot: THM, SMUIO, FUSE, CLKA, PWR, UVD, ATHUB, DBGU_NBIO, DFX, SDMA1, DF, CLKB, DFX_DAP, PCIE, LSDMA, SYSTEMHUB, DIO, and DAZ. The source-snapshot limitation is documented in code and README: it is not proven byte-identical to the installed capture-kernel build. HWIDs 282 and 286 remain `UNKNOWN` and were not guessed.

Validation on the connected Mac:
- IP Discovery parser suite: 31/31 passed (one new mapping/unknown-ID regression test).
- Re-parsed the preserved 10,240-byte RX 7600 candidate binary in strict mode: 39 records; 28 now have source-correlated names; 282 and 286 remain unknown; GC remains 11.0.2.
- Linux inventory collector tests: 8/8 passed.
- GFX11 firmware manifest tests: 9/9 passed.
- Full Navi 33 macOS host suite: all groups passed (35 + 32 + 21 + 16 + 18 + 18 + 10 + 22), IP Discovery bounds (8 fixtures + 512 malformed inputs), Navi48 isolation (4/4), and DriverKit probe contracts (4/4).

This is parser/readability and evidence bookkeeping only. It does not change hardware initialization, firmware loading, MMIO, kext activation, IOAccelerator, Metal or display status. The candidate capture provenance remains UNVERIFIED; RX 7600 Metal dispatch remains BLOCKED because this macOS host still has no registered Metal device/accelerator.


## Board firmware manifest + VBIOS identity intake (2026-10-30)

Continued directly on the Tahoe host (no Remote Desktop hop was needed; the
session runs on the target Mac). The Cobalt64 evidence package on the mounted
user volume holds board-attributed records that had not been brought into this
repository. Added byte-preserving copies plus one derived record:

- `tools/ipdisc/evidence/rx7600-navi33-firmware-manifest-cobalt64.json` — the
  board firmware manifest: 14 required firmware files with source references,
  gfx_target gfx1102, linux-firmware package `20260916-1`, kernel source clone
  commit `64a90897d3c539033f24105968e3513957e70819`.
- `tools/ipdisc/evidence/rx7600-vbios-identity-2026-10-23.json` — VBIOS
  IDENTITY STRINGS from the board's own Linux boot journal: ATOM BIOS
  `113-EXT86626-001`, build `00062039`, ver `022.003.001.027.000001`,
  2023/04/25, hash-bound to the source journal
  (`db85fb970eb4bf69e31943e910c50b06625f0f087439c7f1c6d7f37228c9818a`), with
  corroborated runtime facts (8176M GDDR6 128-bit, SMU OK, DCN 3.2.384, DMUB
  `0x07003300`, VCN ENC 1.24/DEC 9 rev 34, SE 2 / SH 2 / CU 8 / 32 active).
- New `tools/ipdisc/verify_board_firmware_manifest.py` +
  `test_board_firmware_manifest.py`: cross-checks the manifest's 14 firmware
  filename prefixes against the versions actually parsed from the captured IP
  Discovery artifact (GC 11.0.2, PSP/SMU 13.0.7, SDMA 6.0.2, VCN 4.0.4, DCN
  3.2.1), requires full role coverage, keeps HWIDs 282/286 explicitly
  unresolved, and optionally SHA-256-binds the source boot journal. 17/17
  synthetic tests pass; the real-evidence run passes and is journal-bound.
- Re-verified the volume original of `amdgpu_discovery.bin` is byte-identical
  to the repo copy (same SHA-256).

Evidence-gate effect: the board now has BOTH a hashed IP Discovery artifact
AND a VBIOS identity record — the "board-attributed IP Discovery + VBIOS"
prerequisite for C0 is materially advanced. Still open: the VBIOS byte payload
(identity strings are not a ROM dump), independent authentication of the
user-maintained records, and per-blob firmware hash/license review. No firmware
blob was copied into the repository. No hardware access, kext load, firmware
load, MMIO/VRAM access, EFI/SIP/AMFI change, or reboot was performed.

Metal/acceleration status is unchanged: `MTLCreateSystemDefaultDevice()` nil,
`MTLCopyAllDevices()` empty, no IOAccelerator service, `No Kext Loaded` for the
RX 7600. RX 7600 hardware acceleration remains NOT achieved; this session adds
evidence tooling only.
