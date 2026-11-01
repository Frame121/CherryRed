# Navi 33 Metal and Display Integration Plan

Reviewed: 2026-10-30
Target: RX 7600 (1002:7480, subsystem 148c:2421), x86_64 macOS Tahoe 26.7.1 / build 25G241

## Executive decision

CherryRed must implement two related but distinct paths:

1. Display scanout: connector detection/EDID, display-controller initialization, plane/framebuffer allocation, timing, vblank/flip, hotplug, power and WindowServer/IOFramebuffer publication.
2. Metal acceleration: a real IOAccelerator service and user client, GPU memory/resources, command queues and synchronization, a Metal user-space driver/runtime, shader compilation, feature/capability reporting and reset/error handling.

Neither path exists for Navi 33 in the current project. The existing framebuffer byte-layout calculator is arithmetic only. The current Metal readiness field is a gate only. The Mac currently reports no RX 7600 accelerator kext attached, no IOAccelerator service and zero Metal devices in the previous probe. This plan does not claim support.

## Hardware identity and strict separation

- Target PCI identity: 1002:7480, board subsystem 148c:2421.
- Target family: Navi 33 / GFX11, subject to final target IP Discovery capture.
- src/dcn41 explicitly documents DCN 4.1 / Navi 48 / gfx1201 and is not a Navi 33 display implementation.
- src/navi48-bringup is a Navi 48 / GFX12 research implementation. Its display and Metal nub code must not be linked into or selected by the Navi 33 path.
- A register name shared by generations is not proof that the register offset, segment base, reset sequence, or semantics are shared.

## macOS integration architecture

### Display path (required)

```text
RX 7600 PCI function
  -> Navi 33 PCI/ASIC backend
  -> validated IP Discovery + DCN/DMUB firmware and display IP revisions
  -> DCN display resource/power/clock/PHY/HPD/EDID setup
  -> framebuffer memory and scanout plane
  -> vblank/flip/interrupt/hotplug/power management
  -> macOS IOGraphicsFamily / IOFramebuffer-compatible service
  -> IODisplay / WindowServer / compositor
```

The framebuffer service must publish real modes and timing only after the hardware controller is initialized and the scanout buffer is valid. A CPU memory buffer or a synthetic IORegistry node does not prove that the display engine is scanning out that buffer. A successful one-time image is also insufficient: flip completion, vblank, sleep/wake, hotplug, multi-monitor and recovery need validation.

### Metal path (required)

```text
Metal application
  -> Apple Metal framework / MTLDevice discovery
  -> AMD/Navi 33 user-space Metal driver integration
  -> IOAccelerator service + validated user-client ABI
  -> GPU resource/memory manager (VRAM + GART)
  -> command queue / command buffers / packet validation
  -> GFX11 command processor + SDMA + firmware
  -> completion fences, interrupts, fault and reset recovery
```

Apple documents MTLDevice as the application's primary GPU interface and command queues/buffers as the work submission mechanism. CherryRed must provide a functioning hardware-backed implementation beneath those APIs before it can report an RX 7600 Metal device. Metal shader translation alone does not create this stack.

## What existing CherryRed code can and cannot be reused

| Existing component | Safe reuse | Not safe to assume |
|---|---|---|
| Navi33IntegrationContracts | Strict hardware-verification gate and host-side layout arithmetic | No IOFramebuffer or Metal implementation |
| Navi33BringupModel | Host-only validation patterns and test style | No real VRAM, queue or firmware operation |
| src/dcn41 | C coding style, bounded arithmetic, test methodology, only after per-register provenance review | Its DCN 4.1 offsets/segments/IRQ mapping are Navi 48-specific |
| src/navi48-bringup Display/Metal code | Architectural study of separation between display and GPU bring-up | No copying classes, register tables, DMUB protocol, display geometry or runtime hooks into Navi 33 |
| DriverKit PCI probe | PCI match and BAR metadata discovery | Does not map MMIO, initialize GPU, or expose IOFramebuffer/IOAccelerator |
| metal2vulkan | Possible future Metal-shaped API translation on top of a Vulkan 1.2 device | Does not provide Vulkan ICD, PCI driver, firmware, display or macOS Metal registration |

## Work packages and acceptance gates

### D1 — Navi 33 display evidence pack (blocked until evidence is complete)

- Trace RX 7600's exact ASIC and IP Discovery selection in upstream Linux.
- Record DCN, DMUB, DMCUB, DCE/PHY, HPD, AUX/DDC and display resource versions for the exact ASIC path.
- Trace exact firmware request names, versions, ABI and load/authentication order for display-related firmware.
- Capture the target card's connector topology and EDID through a supported, read-only path.
- Pin upstream source commit and firmware WHENCE/license/hash provenance.
- Do not reuse DCN 4.1 Navi 48 tables as a substitute.

Exit criteria: a reviewed, source-pinned Navi 33 display IP/firmware manifest with unknown fields explicitly marked unknown.

### D2 — Display backend contracts (host-only until D1 passes)

- Add a separate navi33-display module, independent of Navi 48.
- Model validated connector, EDID, display mode, pitch, scanout aperture, flip and vblank state.
- Require a capability token derived from verified IP revisions before any future MMIO backend can be constructed.
- Keep all hardware callbacks absent in host tests. Reject unknown register windows, unknown pipe counts, unknown format modifiers and unverified scanout apertures.
- Add tests for EDID checksums, mode bounds, pitch overflow, aperture containment, flip state transitions and stale completion events.

Exit criteria: deterministic host tests and review of every hardware-facing constant against pinned Navi 33 sources. This does not yet enable scanout.

### D3 — Real display hardware bring-up (not currently safe to start)

- Only after the firmware/ASIC/backend gates pass, implement the smallest read-only DCN/DMUB status probe.
- Add a reviewed MMIO mapping and register-access lifetime with bounds checks and explicit device ownership.
- Bring up clocks, power domains, PHY, HPD/AUX/DDC and timing in documented order.
- Add a single known-safe mode and a tested scanout allocation; then vblank and flip.
- Keep native WindowServer/IOFramebuffer publication disabled until scanout, interrupts and recovery are independently verified.

Exit criteria: target hardware demonstrates stable scanout, vblank/flip completion, EDID/mode correctness, and recoverability across repeated cold boot and sleep/wake tests.

### M1 — macOS graphics-family ABI research

- Inventory the exact Tahoe build's IOAccelerator, IOGraphicsFamily, IOFramebuffer, AMD user-space bundles and matching kernel services.
- Record service classes, personalities, matching properties, user-client connection types and ABI assumptions from the installed OS build.
- Separate public API facts from private/reverse-engineered ABI. Do not hardcode private vtable slots based on another macOS build.
- Determine whether a supported DriverKit route can satisfy the required graphics-family service model. PCIDriverKit alone only provides PCI device access; it does not automatically publish a Metal-capable accelerator or IOFramebuffer.
- Establish code-signing, DriverKit/PCI entitlements, system-extension and kernel-extension constraints before choosing the architecture.

Exit criteria: a build-specific, cited macOS service/ABI map and a legally/technically viable loading model. No service spoofing.

### M2 — IOAccelerator service and command execution (blocked by GPU backend)

- Implement resource and memory ownership, VM mappings, command-buffer validation, queue submission, fences, interrupts, fault reporting and reset.
- Implement a real user-client ABI with versioning, capability negotiation, bounds validation and per-process isolation.
- Publish IOAccelerator only after hardware command execution and memory lifetime rules pass stress tests.
- Add process teardown, GPU reset, timeout and device removal tests.

Exit criteria: verified hardware command execution, completion and resource isolation on the RX 7600. A stub service or fake IORegistry node does not pass.

### M3 — Metal user-space integration (blocked by M2)

- Identify the appropriate macOS AMD Metal driver bundle/plugin contract for this OS build.
- Implement device discovery, feature sets, resource formats, pipeline compilation, command queues/buffers, synchronization, timestamps and error handling.
- Build or port an appropriate GFX11 shader compiler/runtime and validate Metal AIR/MSL semantics against Apple's Metal results where legally and technically possible.
- metal2vulkan may be evaluated only if CherryRed first has a working Vulkan 1.2 ICD/device; its README states the runtime needs a Vulkan 1.2 device and is alpha. It is not a substitute for M2.
- Do not report MTLDevice availability until real GPU command submission and rendering/compute validation pass.

Exit criteria: Metal device enumeration, compute and render tests, shader correctness, resource lifetime, synchronization and sustained workloads on the target RX 7600.

## Required validation ladder

1. Static source and firmware provenance review.
2. Host tests with synthetic inputs.
3. Build-only DriverKit/kext/user-space compilation.
4. Read-only target PCI/IP/firmware inventory.
5. Controlled hardware initialization with a tested independent recovery path.
6. Stable scanout and display hotplug/sleep/wake tests.
7. GPU memory/queue/fence/reset tests.
8. IOAccelerator user-client stress and isolation.
9. Metal compute/render conformance and long-running stability.

Passing a lower stage never implies a higher stage.

## Current project status (this session)

- No Navi 33 IOFramebuffer implementation.
- No Navi 33 display controller or DMUB/PHY initialization.
- No Navi 33 IOAccelerator service or user-client.
- No Navi 33 Metal driver bundle, shader compiler integration or MTLDevice.
- No hardware-backed VRAM/GART or GPU command submission.
- src/dcn41 and src/navi48-bringup are not valid Navi 33 implementations.
- Existing host-side layout and readiness tests remain models only.

## Safety boundary

Until D1 and M1 are complete and reviewed: do not map/write GPU registers, load firmware, publish display/accelerator services, install or activate a new extension, change SIP/AMFI, remove -radvesa, edit EFI or reboot. The current EFI backup is file-verified on the same USB but has not been boot-tested as a recovery path.

## Primary references

- Apple Graphics and Displays API collection: https://developer.apple.com/documentation/kernel/hardware_families/graphics_and_displays
- Apple IOGraphicsLib description of IOFramebuffer and IOAccelerator connections: https://developer.apple.com/documentation/iokit/iographicslib_h
- Apple PCIDriverKit overview and entitlement: https://developer.apple.com/documentation/pcidriverkit
- Apple IOPCIDevice PCI/MMIO and device lifecycle API: https://developer.apple.com/documentation/pcidriverkit/iopcidevice
- Apple MTLDevice: https://developer.apple.com/documentation/metal/mtldevice
- Apple Metal command submission: https://developer.apple.com/documentation/metal/setting-up-a-command-structure
- metal2vulkan README and Vulkan runtime requirements: https://github.com/steelbrain/metal2vulkan


## Implementation update — 2026-10-30

Added `include/Navi33DisplayContracts.hpp`, `src/Navi33DisplayContracts.cpp`, and `tests/test_Navi33DisplayContracts.cpp`. This host-only module validates EDID base-block header/checksum, scanout buffer/aperture bounds with evidence gating, and a monotonic pending/completed flip timeline. CMake and `tools/run_macos_host_tests.sh` build and execute its tests.

Validation: display contract tests 18/18 passed; full host suite passed (research 35, lifecycle 32, bring-up model 21, integration contracts 16, display contracts 18, IP Discovery 8 fixtures + 512 malformed inputs, Navi48 isolation 4, DriverKit probe contract 4). No hardware initialization was attempted.

This is a host-only display contract slice, not a physical display driver. No EDID/DDC/AUX access, DCN/DMUB/PHY programming, framebuffer allocation, scanout, vblank interrupt, IOFramebuffer, IOAccelerator, Metal device, or hardware rendering has been implemented. Real display bring-up remains blocked on target-attributed Navi33 IP/firmware evidence and macOS graphics-service ABI review.


## Compute-first priority update — 2026-10-30

The immediate user objective is RX 7600 compute offload, not merely GPU-composited display. The implementation order is therefore compute-first: exact board/IP/firmware evidence, GFX11 bring-up, VRAM/GART, GFX compute queue and minimal write-buffer shader, macOS user-client, RADV/Vulkan, then Metal-facing runtime. Full display/WindowServer integration remains a separate lower-priority track. See `NAVI33_COMPUTE_FIRST_PLAN.md`.

The current macOS host was re-probed: RX 7600 is enumerated as PCI device 1002:7480 with 8 GB, but `system_profiler` reports no kext loaded for it; the Metal probe returns zero devices and nil default device; no IOAccelerator service was found; no Vulkan loader/tools or ICD were found in the inspected standard paths. This confirms there is no usable RX 7600 compute path on this macOS installation yet.

The separate `Navi33ComputeContracts` module is host-only validation scaffolding. It does not initialize hardware or submit a compute job. The GFX11 firmware names in the compute-first plan are candidate research leads only; the board-specific IP Discovery and exact firmware manifest remain unresolved.
