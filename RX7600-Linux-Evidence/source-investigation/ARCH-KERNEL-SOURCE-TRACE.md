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

The tag matches the kernel version family, but the installed package is `7.2.11.arch1-1` and the Git tag alone does not prove that the local package was built from that exact commit with no downstream packaging/config differences. The source checkout is being investigated separately; do not call this a verified binary-to-source match until package build metadata/PKGBUILD and source commit are reconciled.

Direct raw-file requests from GitHub began returning HTTP 429 during this session, so source-level symbol lookup was not completed by the initial fetch. The previously checked upstream `master` and `v7.2` mapping tables did not resolve HWIDs 282 and 286. No names are guessed.

## Next checks

1. Complete checkout of the tag's peeled commit.
2. Inspect `drivers/gpu/drm/amd/include/soc15_hw_ip.h` and `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` at that commit.
3. Compare exact Arch package recipe, config and build source revision.
4. Inspect firmware declaration/request sites in the same source tree.

No system packages, GPU firmware, kernel files, or boot configuration were modified by this trace.
