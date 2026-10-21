# Navi 33 offline research model

This standalone C++17 module models identity and evidence for the RX 7600
research target. It is not part of `Navi48Bringup.kext`, has no IOKit or
hardware-access dependencies, and contains no initialization, firmware-loading,
register, memory, or macOS integration code.

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

The tests use only synthetic values. They do not access a GPU or any device.
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
