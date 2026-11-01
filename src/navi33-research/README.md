# Navi 33 offline research model

This standalone C++17 module models identity, evidence, and hardware-independent
bring-up/integration contracts for the RX 7600 research target. It is not part
of `Navi48Bringup.kext`, has no IOKit or hardware-access dependencies, and
contains no hardware initialization, firmware-loading, register access, VRAM/GART
management, IOAccelerator, Metal, or display integration implementation.

The `1002:7480` identity is accepted only into the `Navi33ResearchOnly` route.
Navi 48 is modeled as a separate family and is represented by
`Navi48NoInitialization`; no route in this module can initialize hardware.
A PCI ID match is not proof of board identity or compatibility. The physical
card's IP versions remain unknown.

`Unknown` describes missing hardware evidence. `Unsupported` describes behavior
not implemented by this module, not a measured silicon limitation. This
research-phase capability type intentionally has no positive `Supported` value,
so it cannot encode an unverified support claim.

IP blocks default to an unknown version. Unknown is a structurally valid but
incomplete record; it is not reported as a complete claim. A version claim is
rejected unless it has a source, a recognized confidence value, and an
`EvidenceKind`. The categories distinguish synthetic fixtures, generic
upstream sources, board-attributed artifacts, macOS interface reviews, and
target hardware tests. `evidenceKindClaimsBoardAttribution` classifies only
the last two board-related claim kinds; it does not verify the attribution.
Firmware records use the same categories. These are caller-supplied labels:
they do not authenticate a source, establish board identity, or prove that an
interface review was correct.

`EvidenceValidation::claimsComplete` and
`firmwareEvidenceClaimsComplete` only indicate that the record's required
fields are populated with recognized enum values. A complete claim is not
verified evidence and does not permit firmware loading or redistribution.

`DriverStage` contains research-stage labels only. No transition executor,
device handle, or hardware-facing API is present. `hardwareInitializationAllowed`
always returns false.

## Build and run the offline unit tests

The module requires standard C++17 (`std::optional`, nested namespace
syntax, and inline variables are used). The local CMake project disables
compiler-specific language extensions, exceptions, and RTTI to match the
relevant C++ restrictions in the existing Navi48 kext build; it uses only the
standard library and keeps generated files in a separate build directory. This
does not establish that the standard-library ABI or these objects can link into
an Apple kernel extension. Every source and test translation unit includes a
compile-time guard that rejects the build if the compiler still enables
exceptions or RTTI. MSVC's Visual Studio generator supplies `/EHsc` by default;
the final `/EHs-c-` option intentionally disables that default and emits D9025
override warnings. From the repository root, with CMake 3.16 or newer
and a supported C++ compiler already available:

```text
cmake -S src/navi33-research -B build/navi33-research
cmake --build build/navi33-research --config Debug
ctest --test-dir build/navi33-research -C Debug --output-on-failure
```

For a direct clang++ build on a Unix-like host, run from the repository root
and keep the output in that same dedicated build directory:

```text
mkdir -p build/navi33-research
clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc/navi33-research/include src/navi33-research/src/Navi33Research.cpp src/navi33-research/tests/test_Navi33Research.cpp -o build/navi33-research/navi33_research_tests
build/navi33-research/navi33_research_tests
```

On Windows, use a Visual Studio Developer Command Prompt and the same CMake
configure/build/test sequence above. Multi-configuration generators require
`--config Debug` for both build and `ctest`; single-configuration generators
also accept these commands.

The CTest suite also registers a static check of the separate Navi48 driver's
PCI-routing isolation contract when Python 3 is available. It can be run
without a C++ toolchain or hardware, from the repository root:

```text
python -B src/navi33-research/tests/test_navi48_isolation.py
```

This checks the checked-in plist match set and the `Navi48Bringup::probe`
source for the Navi48 opt-in and PCI ID guards. It is a static source check,
not a compiled or runtime IOKit test.

CTest also runs `ipdiscovery_bounds_tests`, which compiles the separate
freestanding Navi48 IP Discovery parser and tests synthetic table/record
extents in five fixture cases, including valid v1/v2 layouts. It does not
initialize a GPU or establish that an RX 7600 reports
those IP Discovery versions or layouts.

The tests use synthetic fixtures, including a copied snapshot of Linux sysfs PCI resource metadata from the RX 7600. They do not access a GPU during test execution. The live metadata was collected separately using read-only sysfs access and is recorded under `RX7600-Linux-Evidence/pci-resource-survey.md`.
Passing them verifies this research model only; it does not establish a macOS
driver component, RX 7600 support, or functioning GPU hardware.

The research-model unit cases cover the research-only `1002:7480` route,
Navi 33/Navi 48 separation, rejection of unknown devices and invalid enum
states, incomplete IP/firmware evidence, non-authentication of caller-provided
completeness claims, and the invariant that no selection permits hardware
initialization. The lifecycle test executable covers valid identity, invalid
vendor/device identities, illegal and repeated transitions, prepare/start
failures, cleanup, and identification without platform callbacks.
The evidence fixtures are synthetic placeholders, not RX 7600 observations.

`Navi33DriverCore` adds a portable lifecycle boundary over the existing
identity validator. It requires the explicit sequence
`Uninitialized -> Identified -> Prepared -> Running`, rejects invalid or
repeated transitions, fails closed on platform callback failures, and requests
cleanup on failure, explicit stop, or destruction. `Navi33Platform` is only an
abstract callback boundary; the repository contains no production macOS or
hardware backend for it. The host mock verifies lifecycle mechanics only.
`Running` means only that the supplied platform callback returned success; it
does not establish GPU initialization, display, acceleration, or Metal.
Identification itself calls no platform operation, and the Navi 33 CMake
target has no dependency on the Navi 48 kext.

The existing Navi48 `ipdiscovery.cpp` is not an RX 7600 compatibility proof or
a strict parser for untrusted input. It validates a binary checksum and now
confines its IP Discovery table and lazy record reads to the declared binary
and table bounds. The parser still follows its compatibility-oriented format
handling and Navi48 board-ROM provenance; any Navi33 use must be gated on
separate format evidence.

## Preliminary host dependency audit

After configuring and building the CMake project, run:

```text
python -B src/navi33-research/tools/audit_portable_core.py --build-dir build/navi33-research-msvc --config Debug
```

The script reads generated MSVC compile logs and uses `dumpbin` when available
to report final RTTI/exception flags, unresolved static-library symbols,
selected symbol ownership by Navi33 object file, and the test executable's
imported DLLs. Object-file undefined references are not final-link failures:
the successfully linked Windows test executable resolves them against its
imports, which are reported separately. The audit is read-only and labels the
output as preliminary Windows evidence, not an Apple-kernel ABI test. A Debug
build can include debug-runtime symbols; Release output should be audited
separately.

The portable records use borrowed `std::string_view` inputs; callers must keep
their backing text alive through validation. API-produced diagnostics are
static, null-terminated `const char*` strings and do not own storage. This
removes owning `std::string` construction, including lifecycle transition
message concatenation, without changing lifecycle states, callback order, or
diagnostic text. Evidence still uses `std::optional`, and `Navi33Platform`
still uses virtual dispatch, so the API is not claimed to be kernel-neutral.
Validators read the views synchronously and do not retain them; the lifecycle
core retains only `DeviceValidation`, whose API-produced diagnostic is static
text. Regression tests use local string owners that expire after a call, then
inspect only the independent result. They do not construct or access dangling
views.
The reviewed core sources declare no thread primitives or direct `new`/`delete`
expressions; the device IDs are `inline constexpr` and no dynamic global
initialization was identified. Neither the reduced Windows symbol set nor the
source review proves whether the core can link against Apple's kernel C++
runtime. The audit explicitly states that Windows results neither perform nor
imply a macOS compile or link.

## Bring-up plan gate model (offline only)

`include/Navi33BringupPlan.hpp` and `src/Navi33BringupPlan.cpp` add a compile-tested, fail-closed milestone gate model for identity, resource survey, firmware audit, memory management, interrupts, command submission, display, acceleration, and Metal. The model rejects missing/duplicate stages, unknown state values, and any request to permit hardware writes. Even when every milestone is marked validated, it does not authorize hardware writes. This is planning/control logic only; it does not probe PCI, map BARs, load firmware, touch registers, or replace a macOS kernel backend.

Standalone host test command (from repository root):

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti \
  -Isrc/navi33-research/include \
  src/navi33-research/src/Navi33BringupPlan.cpp \
  src/navi33-research/tests/test_Navi33BringupPlan.cpp \
  -o /tmp/cobalt64-bringup-plan-test
/tmp/cobalt64-bringup-plan-test
```

## macOS host-side validation

On an x86_64 macOS host with Apple Command Line Tools installed, run:

```sh
src/navi33-research/tools/run_macos_host_tests.sh
```

The script compiles the portable Navi 33 research model, lifecycle tests,
firmware metadata/range/queue host models, and integration contracts with Apple
Clang. It runs the IP Discovery parser bounds suite and checks that Navi 33
routing remains isolated from Navi 48 initialization and that the DriverKit probe
is limited to its documented read-only BAR metadata scope. Build products are
created in a temporary directory and removed on exit.

**Scope limit:** these are host-side tests only. The added address allocator,
queue/fence, display-layout, firmware metadata, and readiness logic are models;
they do not allocate VRAM, configure GART, submit GPU packets, load firmware,
create IOAccelerator services, register a Metal device, or drive a display. They
do not build or load a kernel extension, inspect or initialize GPU registers,
validate a physical card's IP Discovery table, or enable graphics acceleration.
Navi 33 hardware initialization and macOS integration remain unimplemented.


## DriverKit PCI identity probe (build only)

With Xcode and the DriverKit SDK installed on the x86_64 Mac, run:

```sh
src/navi33-research/driverkit-pci-probe/build_compile_check.sh
```

This invokes Apple's IIG compiler and Clang for the DriverKit 25.5 target,
compiles the PCI personality stub for `1002:7480` / subsystem `148c:2421`, links
an unsigned `.dext` bundle, and validates its property list and DriverKit Mach-O
target. Build products go under the repository's ignored `build/` directory.

**Scope limit:** this build-only DriverKit prototype has no host app or approved
entitlements. It is not signed, installed, or activated, and it does not access
the GPU. If activated in a future, separately reviewed test, it only logs that
its matching PCI personality started. It does not implement GPU initialization,
firmware, memory management, queues, IOAccelerator, display, or Metal, so it is
not RX 7600 acceleration support. Host-side contracts elsewhere in this module
are planning/test models only and do not extend this probe's hardware behavior.


## Read-only macOS graphics lag diagnostic

From the repository root, run:

```sh
tools/diagnostics/macos_gpu_lag_report.sh
```

The script writes a short report to the system temporary directory. It records the macOS build, GPU/VRAM and kext status, PCI and subsystem IDs from a filtered IORegistry query, a summary of registered `IOAccelerator` services, public Metal device enumeration, a one-time `WindowServer` CPU sample, current Reduce Motion/Reduce Transparency settings, and recent matching WindowServer log messages. It intentionally avoids full IORegistry dumps and does not change system settings, modify EFI, or load/unload any kext.

A missing `IOAccelerator` service plus a 7 MB reported VRAM value and `Kernel Extension Info: No Kext Loaded` is strong evidence that the RX 7600 does not have a functioning macOS graphics driver in the current boot. This diagnostic is not itself a fix or a GPU driver. The portable Navi 33 tests remain research-only and must not be described as acceleration support.


## Metal and display integration roadmap

The build-specific architecture, implementation work packages, dependency gates, reuse boundaries, and hardware acceptance criteria for RX 7600 Metal and display are documented in `NAVI33_METAL_DISPLAY_IMPLEMENTATION_PLAN.md`. This is a plan and evidence boundary, not an IOFramebuffer, IOAccelerator, Metal driver, or display implementation. In particular, `src/dcn41` and `src/navi48-bringup` are Navi 48 references and are not safe Navi 33 backends.


## Navi 33 display contracts (host model only)

`Navi33DisplayContracts` adds bounded validation for a 128-byte EDID base block (header and checksum), scanout buffer dimensions/pitch/length and aperture containment, and a monotonic flip timeline that accepts completion only for the matching pending sequence. Generic-upstream evidence is insufficient to authorize a scanout buffer; only board-attributed or hardware-verified evidence passes this model gate. These contracts do not read EDID from a connector, allocate a framebuffer, map a BAR, program DCN/PHY/DMUB, wait for a vblank interrupt, or expose IOFramebuffer/WindowServer. All tests use synthetic fixtures.


## Compute-first path

The RX 7600 goal is now prioritised as a headless compute bring-up before full display integration. See `NAVI33_COMPUTE_FIRST_PLAN.md` for the staged GFX11 firmware, memory/GART, queue, minimal compute submission, macOS user-client, RADV/Vulkan and Metal milestones. `Navi33ComputeContracts` is host-only validation scaffolding; it does not submit work to the GPU. Its synthetic alignment/size policy values are test fixtures, not asserted GFX11 hardware constants.


## GFX11 backend foundation (2026-10-30)

`Navi33Gfx11Manifest` is the first GFX11-specific foundation module. It is a
strict, host-only manifest gate for the target PCI/subsystem identity, board
evidence, VBIOS attribution, IP major-family separation, version-matched
firmware filename prefixes, SHA-256/commit field syntax, per-file provenance
and license review flags, and required firmware roles. It deliberately rejects
GFX12/Navi48 firmware families.

The test manifest is synthetic. It does not describe the physical RX 7600,
verify any actual firmware hash, or establish license approval. The module does
not open firmware files, authenticate metadata, access PCI/BAR/MMIO, initialize
PSP/SMU/GC/SDMA, allocate VRAM/GART, build a hardware queue, submit an IB, or
expose IOAccelerator/Metal. It is a prerequisite gate only, not GFX11
initialization or RX 7600 acceleration.

The host test script now builds and runs this GFX11 gate. Passing the tests is
not evidence of hardware support. The next actual hardware-backend milestone
still requires a provenance-documented IP Discovery/VBIOS artifact for the
exact board and a reviewed, testable recovery path.


## Four GFX11-to-Metal workstreams

`Navi33Gfx11Bringup` adds host-only models for (1) ordered, evidence-gated
GFX11 bring-up stages; (2) VRAM/GART/GPUVM region and mapping validation;
(3) GFX11 pipe/queue, ring cursor, submission sequence and fence timeline
validation; and (4) the IOService/user-client/IOAccelerator/Metal publication
evidence sequence.

These are safety and architecture contracts, not device operations. They do
not load firmware, authenticate PSP, communicate with SMU, access registers,
allocate/map VRAM, build page tables, create rings, encode PM4/IB commands,
ring doorbells, receive interrupts, publish IOAccelerator, or create a Metal
device. The RX 7600 is not accelerated by these changes.

The GFX11 queue topology assumptions are generic upstream architecture notes
(two GFX pipes and two queues per pipe; MES replaces KIQ on GFX11+), and must
not be mistaken for measured RX 7600 board topology. All four model suites are
compiled and tested by `tools/run_macos_host_tests.sh`.
