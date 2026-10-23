# Cobalt64 Linux validation - 2026-10-23

## Passed
- Python IP Discovery and Windows-driver scanner suite: 37/37.
- Portable Navi33 research C++ tests: 35 passed, 0 failed.
- Portable Navi33 driver lifecycle tests: 32 passed, 0 failed.
- IP Discovery bounds tests: 8 fixture cases plus 512 malformed inputs passed.
- Navi48 isolation tests: 4/4.
- Navi33 survey safety tests: 5/5.
- Portable C++ targets compiled on Linux with g++ using C++17, -Wall, -Wextra, -Werror, -pedantic, -fno-exceptions, and -fno-rtti.

## Not yet verified
- OpenGL was previously recorded PASS in userspace-gpu-validation.md.
- Vulkan not tested: vulkaninfo and required packages are absent; installing packages requires interactive administrator authorization.
- CTest unavailable on this Linux host; the existing CTest build folder is an MSVC/Windows build.
- Exact Arch kernel source-to-binary match not established; HWIDs 282 and 286 remain unresolved.
- No macOS kext was built, loaded, or tested on macOS. These tests validate the portable research code and safety gates only.

## Safety
No system packages, GPU firmware, kernel, boot settings, or hardware registers were changed by this validation run. Temporary test binaries were placed under /tmp/cobalt64-navi33-linux-check.
