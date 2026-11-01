# Cobalt64 next dependency/source investigation

## Verified host stack (2026-10-23)
- PCI GPU: `1002:7480`, subsystem `148c:2421`
- Kernel: `7.2.11-arch1-1`; Arch package `linux 7.2.11.arch1-1`
- Kernel build metadata: packager Jan Alexander Steffens (heftig); build date 2026-10-16
- Firmware package: `linux-firmware-amdgpu 20260916-1`
- Userspace packages: `mesa 1:26.2.5-1`, `libdrm 2.4.134-1`
- `vulkan-radeon` was not returned by `pacman -Q vulkan-radeon`; package may not be installed or may have a different name on this distro. Do not assume Vulkan userspace is present.

## Next investigations
1. Retrieve the exact Arch `linux` source package/patch for `7.2.11.arch1-1`; compare `soc15_hw_ip.h` and `amdgpu_discovery.c` to determine whether HWID 282/286 have new symbols or aliases.
2. Check the package's `MODULE_FIRMWARE` declarations and actual firmware-loader callsites; compare them against current boot log. The current journal confirms DMUB and VCN firmware, but doesn't expose every successful firmware request.
3. Check installed graphics userspace (Mesa DRI driver, Vulkan ICD and loader) separately from kernel driver and firmware. Kernel GPU bring-up alone does not prove all userspace APIs work.
4. Keep a strict platform boundary: Linux amdgpu firmware and interfaces are evidence for hardware behavior, not directly reusable macOS dependencies. Any macOS implementation needs Apple/IOKit-compatible initialization, memory management, command submission, display and firmware handling, plus legal review of any firmware redistribution.

## Not done / no claims
- No firmware flash or modification.
- No claim that all firmware blobs were loaded.
- No claim of native macOS acceleration or working kext.
- HWID 282/286 names remain unknown until exact source evidence is obtained.
