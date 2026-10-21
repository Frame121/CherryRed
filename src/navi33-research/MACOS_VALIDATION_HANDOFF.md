# First macOS validation handoff

This procedure is for an authorized macOS host with Xcode Command Line Tools
or Xcode already installed. It does not install tools, load a kext, access
hardware, or modify system security settings. Run it from the repository root.

## Local prerequisite inventory

Status below describes the inspected Cobalt64 checkout and current Windows
host, not a separate Mac that has not yet been inspected.

| Prerequisite | Current evidence/status |
|---|---|
| macOS host | **Missing here**: this checkout is being inspected on Windows. Availability on the authorized validation host is unverified. |
| Xcode Command Line Tools / `xcrun` / Apple Clang | **Missing here**. `src/navi48-bringup/Makefile` requires `xcrun`, `clang`, and `clang++`; their availability and versions on a Mac must be checked there. |
| macOS SDK | **Unverified/missing here**. The Makefile obtains it from `xcrun --sdk macosx --show-sdk-path`; no SDK path can be established on this Windows host. |
| GNU Make and standard inspection tools | **Missing here/unverified on target**. The kext Makefile needs `make`; its link checks also invoke `nm`, `c++filt`, `grep`, `sed`, `find`, `xattr`, and `codesign`. |
| Portable Navi33 sources and CMake project | **Present**: `src/navi33-research/` contains the portable C++17 core and tests. The standalone diagnostic below does not require CMake. |
| MacKernelSDK headers | **Missing in this checkout**: the referenced `src/RDNA4FB/MacKernelSDK/Headers` path does not exist. |
| MacKernelSDK link libraries | **Missing in this checkout**: `src/RDNA4FB/MacKernelSDK/Library/x86_64` does not exist; the Makefile expects `libkmodc++` and `libkmod` there. |
| Navi48 kext build files | **Present**: `src/navi48-bringup/Makefile`, `Info.plist`, sources, and `tools/embed-firmware.py`. `src/navi48-bringup/BUILDING.md` and `tools/fetch-firmware.sh` are absent. |
| Required Navi48 firmware | **Missing in this checkout**: `src/navi48-bringup/firmware/` is absent. The Makefile names ten mandatory blobs: `gc_12_0_1_imu`, `gc_12_0_1_me`, `gc_12_0_1_mec`, `gc_12_0_1_pfp`, `gc_12_0_1_rlc`, `gc_12_0_1_uni_mes`, `psp_14_0_3_sos`, `psp_14_0_3_ta`, `sdma_7_0_1`, and `smu_14_0_3` (each `.bin`). |
| Optional shader inputs | **Missing/unverified here**: `shaders/*.bin` and optional firmware files were not present in the inspected kext tree. The Makefile can generate empty stubs only for its explicitly optional firmware set; those stubs do not satisfy mandatory firmware. |
| Local Cobalt64 license/notice | **Present**: `src/navi48-bringup/LICENSE` is BSD 3-Clause and `NOTICE` records RDNA4FB BSD-3 and mac-amdgpu MIT attribution, including pinned mac-amdgpu commit `3bdeed2de940504ad6bd1bac718d5de2f65ddb83`. Linux amdgpu is cited as GPL-2.0 reference material only, not copied code. |
| MacKernelSDK / firmware license review | **Unverified**: the SDK and firmware blobs are absent, so their exact license texts, provenance, and redistribution conditions have not been checked. Review the exact artifacts and preserve their notices before using them. |

The kext Makefile targets **x86_64, macOS 11.0 minimum** (`ARCH := x86_64`,
`DEPLOY := 11.0`), with `-target x86_64-apple-macos11.0` and
`-mmacosx-version-min=11.0`. It requests C++17, exceptions/RTTI disabled,
`-fcheck-new`, `-fno-c++-static-destructors`, `-nostdinc`, private kernel
defines, `-mkernel`, and `-fapple-kext`; its link uses `-nostdlib`, `-Xlinker
-kext`, `-lkmodc++`, and `-lkmod`. Those settings belong to the Navi48 kext
build and are not automatically suitable for a standalone user-space
diagnostic.

The checked-in `Info.plist` identifies the bundle as
`com.navi48.bringup`, sets KPI library versions to `20.0.0`, and matches only
Navi48 IDs `1002:7550` and `1002:7551`. It is not a Navi33 personality.

## Portable-core compile/link diagnostic

This experiment uses Xcode's macOS SDK and ordinary Apple Clang C++ runtime.
It compiles and links the portable Navi33 lifecycle test as an x86_64
executable without MacKernelSDK, firmware, IOKit, or device access. It is
intentionally **not** a kext build and does not test kernel ABI compatibility.

```sh
xcode-select -p
xcrun --find clang++
xcrun --sdk macosx --show-sdk-path
mkdir -p build/navi33-macos-diagnostic
xcrun --sdk macosx clang++ \
  -isysroot "$(xcrun --sdk macosx --show-sdk-path)" \
  -arch x86_64 -mmacosx-version-min=11.0 \
  -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fno-exceptions -fno-rtti -fno-c++-static-destructors \
  -Isrc/navi33-research/include \
  src/navi33-research/src/Navi33Research.cpp \
  src/navi33-research/src/Navi33DriverCore.cpp \
  src/navi33-research/tests/test_Navi33DriverCore.cpp \
  -o build/navi33-macos-diagnostic/navi33_driver_core_tests
file build/navi33-macos-diagnostic/navi33_driver_core_tests
otool -L build/navi33-macos-diagnostic/navi33_driver_core_tests
```

On an Intel Mac, the resulting test executable can also be run directly:

```sh
build/navi33-macos-diagnostic/navi33_driver_core_tests
```

On Apple Silicon, this x86_64 artifact is a compile/link result only unless an
authorized x86_64 execution path (such as already-installed Rosetta) is
available. Do not install one as part of this procedure. The diagnostic's
`otool -L` output is expected to describe the normal macOS executable runtime;
it must not be represented as evidence about `libkmodc++` or the kernel C++
ABI.

## Separate kernel-module build gate

The existing `make -C src/navi48-bringup` target builds and ad-hoc signs the
**Navi48** bundle; it is not a Navi33 build. Before attempting it, an authorized
operator must verify the exact MacKernelSDK distribution, license, headers,
and x86_64 `libkmodc++`/`libkmod` files; provide and license-review all ten
mandatory firmware blobs; verify required host tools; and review the Makefile's
generated-file and signing steps. The Makefile writes into its build and
generated-source paths. Do not run its `clean` target as part of validation:
it removes the build directory and generated `src/fw/fw_*.c` files.

A successful Navi48 kext compile/link would prove only that that particular
Navi48 source/configuration links with the supplied SDK and firmware. It would
not compile Navi33 integration code, prove a Navi33 service exists, establish
that Navi33 IP/firmware assumptions are correct, or demonstrate RX 7600
hardware functionality. A standalone Navi33 kernel-link probe would be a
separate future experiment and must first establish that the SDK's headers and
kernel C++ runtime support the core's `std::optional`, `std::string_view`, and
virtual callback ABI under these flags.

## Evidence boundaries

The portable diagnostic can verify Apple Clang parsing, x86_64 code generation,
normal executable linking, and (where executable) host-side lifecycle tests.
It cannot establish kernel-module compilation/linking or kernel ABI
compatibility. A kernel-module compile/link still does not prove that a driver
can initialize Navi33 hardware. Hardware initialization, display output,
acceleration, and Metal each require separate implementation and authorized
validation evidence; none is demonstrated by this handoff.
