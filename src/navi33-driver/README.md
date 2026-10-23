# Navi 33 driver foundation

This component implements firmware intake and software resource preparation
for the `1002:7480` research target. **It does not initialize a GPU or provide
display/Metal acceleration.** It is separate from the existing offline
research API and from the passive `Navi33Probe.kext` diagnostic.

## Implemented

- **Firmware envelope parser:** little-endian byte decoding without alignment
  assumptions; exact file-size, header and payload bounds checks; metadata
  copied out without retaining the caller's buffer. Failed parsing clears
  output. Common-header version and IP fields are observations only.
- **VRAM address allocator:** fixed capacity (64 occupied ranges), explicit
  aperture and reserved ranges, aligned first-fit allocation, checked 64-bit
  arithmetic, fragmentation reuse, and rejection of foreign/stale/double-free
  handles. Failed configuration is atomic. This plans addresses; it does not
  map, allocate physical VRAM, establish residency, or access the device.
- **Preparation transaction:** exact AMD/Navi33 identity guard, duplicate
  firmware-role rejection, explicit expected header/IP fields, allocation of
  staging addresses for each complete input file, and rollback if any
  allocation fails. `Prepared` denotes a software plan. `start()` returns
  `HardwareBackendUnavailable`; `stop()` releases the plan and is idempotent.
- **Host firmware inspection CLI:** bounded (64 MiB) local-file intake with
  exit codes 0 for a bounded envelope, 1 for rejected data, and 2 for usage/I/O
  errors. It reports the declared CRC without claiming verification.
- **Kernel-header compile check:** compiles the three production library
  sources and two probe sources against pinned MacKernelSDK headers for
  x86_64 macOS. The core uses no STL, dynamic allocation, exceptions, RTTI,
  virtual callbacks, or IOKit headers. Generated compiler memory helpers may
  still require kernel symbols at link time.

## Build and test

Requires a C++17 compiler, CMake 3.16+, and Python 3. From the repository root:

```sh
cmake -S src/navi33-driver -B build/navi33-driver -DCMAKE_BUILD_TYPE=Debug
cmake --build build/navi33-driver --parallel
ctest --test-dir build/navi33-driver --output-on-failure
build/navi33-driver/navi33_firmware_inspect /path/to/firmware.bin
```

Tests include 2,000 deterministic malformed firmware envelopes and 5,000
allocator operations compared against an independent occupancy model, as
well as preparation rollback, metadata lifetime, and CLI integration checks.
All fixtures are synthetic; their IP values deliberately do not describe a
physical Navi33. Tests use checks that remain active in Release builds.

To compile against real kernel headers with Clang on Linux or macOS:

```sh
git clone https://github.com/acidanthera/MacKernelSDK.git /path/to/MacKernelSDK
git -C /path/to/MacKernelSDK checkout --detach 3f750085caa17ec3a7880f11c11bf4f48cd6a164
bash src/navi33-driver/tools/check_kernel_compile.sh /path/to/MacKernelSDK
```

This emits Mach-O objects, not a linked/loaded driver. The
[`Navi33 host and macOS build`](../../.github/workflows/navi33.yml) workflow
also runs host tests, builds/signs the diagnostic kext on macOS, and publishes
an experimental artifact if successful. It never loads a kext or exercises a
GPU. The foundation library is not linked into the passive diagnostic, which
has no firmware/preparation inputs.

## Caller contract

Use `Firmware.hpp`, `VramArena.hpp`, and `Preparation.hpp`. Calls are serialized
by the caller; there is no internal locking. Arena/Preparation objects cannot
be copied or moved. Release live allocation handles before overwriting them;
they are scoped to their owning arena's lifetime. Prepared-item pointers
remain valid only while the plan is prepared and the object is alive.

The caller must establish the aperture and all exclusions independently,
including boot framebuffer, firmware-reserved memory, and driver-owned data.
The library cannot discover missing reservations. No memory range, firmware
version, filename, register base, or IP inventory is guessed from the PCI ID.

`Preparation` validates only the caller's requested firmware set. The set is
not asserted to be complete. Equality with caller-supplied expected versions
does not authenticate them. The common header does not contain the full IP
revision, and its major/minor fields cannot replace IP Discovery evidence.
Neither successful envelope parsing nor a prepared plan verifies CRC,
signature, provenance, licensing, IP-specific subimages, PSP load order, or
compatibility. A future loader must independently validate those properties
and the bytes it actually submits; this library retains metadata only.

## Remaining hardware work

The original [evidence gate](../../tools/ipdisc/RX7600_NAVI33_EVIDENCE.md)
and `hardwareInitializationAllowed == false` invariant of the separate
research model still apply. Missing work includes:

1. Board-attributed IP Discovery intake and verified register/IP dispatch.
2. Exact per-IP firmware layouts, signatures/provenance and loading sequences.
3. IOKit device ownership, BAR mapping, DMA/GART, interrupts and reset handling.
4. PSP/SMU/GFX/SDMA initialization, rings and synchronization on actual hardware.
5. Display/connector initialization and Metal userspace integration.

These are substantial unfinished parts of a graphics driver. Passing host
tests or producing a kext artifact does not establish a percentage of GPU
support or working acceleration.
