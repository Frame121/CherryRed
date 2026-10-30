# CherryRed Metal compute probe

This is the first real Metal-side compute workload in the Navi 33 research track. It contains a bounds-checked vector-add kernel and a Swift host that compiles a pipeline, allocates shared buffers, submits work, waits for completion, and verifies every output element.

## Build

From this directory on macOS with Xcode Command Line Tools:

```sh
./build_and_run.sh --build-only
```

## Build and execute

```sh
./build_and_run.sh
```

The execution path requires `MTLCreateSystemDefaultDevice()` to return a real Metal device. It intentionally has no CPU fallback. A successful run proves that the selected Metal device executed this generic kernel; it does **not** prove that the RX 7600 was selected or supported. The current RX 7600 host inventory reports no Metal device and no IOAccelerator service, so runtime execution is expected to stop with exit code 2 until the driver stack exists.

## Scope

- Generic Metal compute smoke test; no AMD-specific register or packet assumptions.
- No fake `MTLDevice`, IOAccelerator service, kernel extension, DriverKit activation, firmware load, MMIO access, or GPU reset.
- This is an application-level Metal test, not yet the CherryRed Navi 33 backend or a replacement Metal runtime.

## Build validation suite

```sh
./test_build.sh
```

This validates source contracts, AIR output, metallib structure and kernel symbol, plus the Swift host build. It reports hardware dispatch as `SKIP` when the host has no working Metal device; it never counts a skipped dispatch as a GPU pass. To attempt a real dispatch, use `./build_and_run.sh`.


## Exact RX 7600 acceptance probe

`test_rx7600_metal.sh` builds and runs `RX7600MetalComputeProbe.swift`.
Unlike the generic smoke test above, this probe only reports success if:
- a Metal device exists;
- its IOAccelerator registry ID can be resolved to a PCI ancestor;
- the ancestor matches the exact captured board identity
  (vendor/device `1002:7480`, subsystem `148c:2421`); and
- a 4096-element Metal vector-add dispatch completes and every result is
  verified.

A different AMD/Intel/virtual Metal device is never accepted as RX 7600.
No device, unresolved registry ancestry, identity mismatch, command failure,
or result mismatch is a hard failure. A successful compile is not a runtime
pass. Current host result: the probe builds, but `MTLCopyAllDevices()`
returns no devices, so hardware acceptance remains blocked.


## Latest host execution — 2026-10-30

After installing Metal Toolchain 17F42, both runtime paths were rebuilt and
executed again on the connected macOS Tahoe host:

- `build_and_run.sh`: shader, metallib, and Swift host build PASS; runtime
  exits 2 because `MTLCreateSystemDefaultDevice()` returns nil.
- `test_rx7600_metal.sh`: exact-board probe build PASS; runtime exits 2
  because `MTLCopyAllDevices()` returns no devices.
- `system_profiler SPDisplaysDataType` lists RX 7600 PCI identity
  `1002:7480`, 8 GB VRAM, but says `Kernel Extension Info: No Kext Loaded`.

This is a missing macOS accelerator-driver prerequisite, not a shader build
failure. Metal Toolchain installation does not supply an AMD IOAccelerator
backend. Hardware dispatch is **BLOCKED**, not passed. Do not spoof device
identity, accept another GPU, add a CPU fallback, or force-load an unimplemented
kext to turn this into a false pass.
