# Exact Arch kernel source trace — Cobalt64 RX 7600

Date: 2026-10-23

## Verified metadata

- Installed kernel package: `linux 7.2.11.arch1-1`
- Installed headers package: `linux-headers 7.2.11.arch1-1`
- Kernel build timestamp: `2026-10-16 00:28:58 UTC`
- Arch Linux kernel source repository: `https://github.com/archlinux/linux`
- Repository tag found through GitHub API: `v7.2.11-arch1`
- Tag object: `ca35c8c6d827d1d8b75211b41e4ce4062509760a`
- Peeled commit: `64a90897d3c539033f24105968e3513957e70819`

## Important caveat

The complete local source checkout is at the peeled commit above. The checked-in `PKGBUILD.trunk` is for `6.3.2.arch1`, not the installed `7.2.11.arch1-1` package, so it cannot establish the build recipe for the installed kernel. The Git tag and source tree therefore still do **not** prove binary-to-source identity or exclude package-specific patches/configuration differences.

The exact tagged source tables have now been inspected locally. `drivers/gpu/drm/amd/include/soc15_hw_ip.h` has no named `*_HWID` constants for numeric IDs 282 or 286; `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` has no explicit name-table entries at indices 282 or 286. The two captured RX 7600 records therefore remain semantically unresolved in this source snapshot. No names are guessed. See `ARCH-V7.2.11-HWID-AUDIT.md`.

## Next checks

1. Obtain the exact `7.2.11.arch1-1` package recipe/build logs or installed build metadata to compare its source revision, patches, and config with this tag.
2. Inspect firmware declaration/request sites in the same source tree and correlate with the recorded RX 7600 boot evidence.

No system packages, GPU firmware, kernel files, or boot configuration were modified by this trace.
