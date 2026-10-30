# Navi 33 compute-first bring-up plan

Date: 2026-10-30
Target: RX 7600, PCI 1002:7480, subsystem 148c:2421, macOS Tahoe 26.7.1 x86_64

## Objective

Prioritise GPU compute so applications can eventually submit work to the RX 7600 instead of falling back to CPU rendering/compute. Display/WindowServer integration is a separate milestone and is not a prerequisite for a headless compute proof, although the macOS accelerator and Metal integration still need a real kernel/user-space device path.

## Verified current host state

- macOS detects the PCI graphics function and reports RX 7600, 8 GB, device 0x7480.
- system_profiler reports Kernel Extension Info: No Kext Loaded for the RX 7600.
- A native Metal probe reports zero devices and MTLCreateSystemDefaultDevice() == nil.
- No IOAccelerator service was found by the current registry probe.
- No Vulkan loader/tools or Vulkan ICD were found in the inspected standard paths.
- -radvesa remains enabled. It supplies a basic display path, not GPU compute.
- Therefore the machine currently has no usable RX 7600 compute path from macOS. Merely adding a Metal API wrapper or a fake IORegistry service would not change this.

## Architecture decision

Build a compute-first, headless path before full display:

RX 7600 PCI -> exact Navi 33 backend -> PSP/SMU/GC/SDMA firmware and reset -> VRAM/GART/VMID -> GFX compute queue + IB submission -> fence/interrupt/reset -> macOS accelerator user-client -> RADV/Vulkan device -> Metal-facing runtime

Do not route Navi 33 through Navi 48 classes, register tables, firmware manifests, or user-client ABI. The existing Navi 48 Mesa patches are coupled to N48N ABI and gfx1201 firmware; they are reference material only until a separately reviewed GFX11 backend and ABI exist.

## Candidate Linux firmware mapping — not approved for loading

Upstream Linux/firmware references identify a Navi 33 candidate family around GC 11.0.2, PSP 13.0.7, SMU 13.0.7, SDMA 6.0.2, VCN 4.0.4 and DCN 3.2.1. This is a research lead, not yet the RX 7600 board manifest. In particular, the board PCI revision 0xcf must not be treated as a proven GC IP revision without tracing the exact Linux ASIC selection and IP Discovery data.

Candidate compute firmware families to trace against the exact ASIC path:
- PSP 13.0.7: SOS and TA/TOC payloads as required by the selected PSP path.
- SMU 13.0.7: power-management firmware.
- GC 11.0.2: PFP, ME, MEC, RLC and any selected IMU/MES payloads.
- SDMA 6.0.2: copy-engine firmware.

No firmware is copied into the repo, embedded, loaded or redistributed by this change. Verify exact filenames, firmware header versions, load order, hashes, upstream commit and per-file licence before any hardware use. Firmware families are not interchangeable across ASICs.

## Work packages

### C0 — Exact target identity and firmware manifest
- Trace PCI ID 1002:7480 + subsystem 148c:2421 + revision through upstream Linux amdgpu ASIC selection.
- Reconcile ASIC revision with IP Discovery and record GC/PSP/SMU/SDMA versions independently.
- Pin Linux source commit and linux-firmware WHENCE revision.
- Identify exact firmware files, header ABI, authentication path and licence.
- Exit: reviewed board-specific manifest; unknown values remain unknown.

### C1 — GFX11 device bring-up, compute only
- Implement a separate Navi33 PCI/ASIC backend.
- Establish safe device ownership, BAR mapping bounds and reset/power sequencing only after C0.
- Bring up PSP and the minimum GFX11 firmware needed for the compute engine.
- Prove read-only status first, then a controlled reset/firmware load with recovery.
- Exit: repeatable target hardware init without display programming.

### C2 — Memory and queue
- VRAM discovery and ownership; GART page tables, VMID and GPU virtual address mapping.
- Ring allocation, queue scheduling, doorbell and command processor state.
- Keep host pointer import and user mappings disabled until memory lifetime and isolation are proven.
- Exit: bounded GPU memory mapping and a queue that can be started/stopped safely.

### C3 — Minimal compute submission
- Use a minimal GFX11 compute IB and a write-to-buffer test shader.
- Validate packet lengths/opcodes against GFX11 source headers; never reuse gfx1201 packet assumptions blindly.
- Add fence completion, timeout, GPU fault capture and reset recovery.
- Exit: GPU writes a known result into a buffer repeatedly, with CPU verifying the result. This is the first genuine compute milestone.

### C4 — macOS kernel/user-client bridge
- Decide KEXT versus DriverKit only after confirming the service model and entitlements for this macOS build.
- Implement versioned, bounds-checked user-client calls for allocation, map, submit, wait and teardown.
- Publish no accelerator service until C3 passes on hardware.
- Exit: an unprivileged test client can submit and verify a compute job with per-process resource isolation.

### C5 — Vulkan/RADV runtime
- Port or adapt a Mesa RADV backend for the new Navi33 user-client ABI.
- Build and test a Vulkan 1.2 ICD on macOS; implement device discovery, memory, queues, sync and shader compilation.
- Existing mesa-patches/0001..0009 are explicitly Navi48/N48N-specific and must not be applied as a Navi33 solution without a new backend and ABI.
- Exit: Vulkan compute test executes on RX 7600, not CPU software rasterizer.

### C6 — Metal-facing runtime
- Only after C5: integrate a Metal-compatible runtime/device implementation and shader translation path.
- Report MTLDevice only after the device dispatches real compute and rendering work.
- Exit: Metal compute correctness and sustained workload test on RX 7600.

### D — Display (parallel, lower priority)
- Continue the separate Navi33 DCN/DMUB/PHY and IOFramebuffer path.
- Keep display register programming out of the compute bring-up until the compute engine is stable.

## What was implemented in this session

- Navi33ComputeContracts adds host-side checks for queue geometry, IB bounds, target evidence, alignment policy, size limits, address overflow and monotonically increasing submission sequence.
- Alignment and size policy are passed in as target-derived values; the tests use explicitly synthetic policy values and do not claim GFX11 hardware constants.
- CMake and the macOS host test script include the new module.
- These checks are validation scaffolding only. They do not allocate GPU memory, create a hardware queue, ring a doorbell, submit packets, or run a shader.

## Acceptance ladder

1. Board-specific identity and firmware provenance.
2. Build-only driver and user-client.
3. Read-only target PCI/IP inventory.
4. Controlled hardware init with verified recovery.
5. VRAM/GART and queue start/stop.
6. GPU write-buffer compute test with fence and reset recovery.
7. macOS user-client isolation.
8. Vulkan compute on RX 7600.
9. Metal compute on RX 7600.

No lower stage implies a higher stage. No fake Metal device or CPU fallback is counted as GPU success.

## Safety boundary

Until C0 is reviewed: no BAR/MMIO writes, firmware loading, GPU reset, kext activation, DriverKit activation, SIP/AMFI changes, EFI changes, removal of -radvesa, or reboot. Existing Navi48 hardware code is not to be loaded on the RX 7600.

## Metal application probe added (2026-10-30)

Added `metal-compute/VectorAdd.metal`, a bounds-checked vector-add kernel, plus `MetalComputeProbe.swift` to create a Metal pipeline, allocate shared buffers, submit the dispatch, wait for completion, and verify all 4096 results. `build_and_run.sh` builds the AIR/metallib and Swift host; `--build-only` skips device execution. Swift host type-check and shell syntax validation pass. The Metal shader build is currently blocked because this Xcode installation lacks the optional Metal Toolchain component (`xcodebuild -downloadComponent MetalToolchain` is suggested by Xcode). No component was downloaded or installed. The host also has no available Metal device for runtime execution. This probe is a generic application-level Metal test and does not constitute RX 7600 support or the CherryRed Navi33 backend.


Metal probe follow-up after Metal Toolchain installation (2026-10-30): `build_and_run.sh --build-only` now succeeds; Xcode produced `VectorAdd.air`, `CherryRedCompute.metallib`, and the Swift executable. Running the probe exits with code 2 at the explicit device guard: `MTLCreateSystemDefaultDevice() returned nil; no Metal device is available. No CPU fallback is used.` This confirms the shader and host build, but no GPU dispatch occurred. `git diff --check` passes.
