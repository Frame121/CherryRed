# RX 7600 kernel source reconciliation

Date: 2026-10-24

## Source tree verified

The local checkout at `source-investigation/arch-linux-v7.2.11/` is a complete Git checkout of the Arch Linux kernel source tag `v7.2.11-arch1`:

- Tag commit: `64a90897d3c539033f24105968e3513957e70819`
- Commit tree: `59570aa312f26fe90aa4c44f564bf87c3cf99b8e`
- Commit date: 2026-10-16
- Subject: `Arch Linux kernel v7.2.11-arch1`
- Kernel Makefile: `VERSION = 7`, `PATCHLEVEL = 2`, `SUBLEVEL = 11`, `EXTRAVERSION = -arch1`

These facts establish that the checked-out source is the Arch `7.2.11-arch1` source revision. They do not establish that the installed `linux 7.2.11.arch1-1` binary was built from this exact checkout, package recipe, configuration, or patch set.

## Package recipe gap

The only captured `PKGBUILD.trunk` in `source-investigation/` declares `pkgver=6.3.2.arch1`. It is not the recipe for the installed `7.2.11.arch1-1` package and must not be used to claim binary-to-source parity. No matching `7.2.11.arch1-1` PKGBUILD, package `.BUILDINFO`, build configuration, or package signature record is present in the captured evidence directory.

To close the binary provenance gap, capture the installed package's `.BUILDINFO` (or the cached package archive's `.BUILDINFO`) and the exact PKGBUILD/patch/config inputs used for `linux 7.2.11.arch1-1`. Record package checksum and signature verification separately. Until those are compared with the source checkout above, status remains **source tag verified; installed binary match unverified**.

## HWID 282 and 286 check

The exact tagged checkout was inspected at:

- `drivers/gpu/drm/amd/include/soc15_hw_ip.h`
- `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c`

Neither numeric ID `282` nor `286` has a corresponding named HWID definition in `soc15_hw_ip.h`; no matching entry appears in the Discovery name mapping in `amdgpu_discovery.c`. This exact-tag check therefore **does not resolve** the two IDs. Keep them as `UNKNOWN_HWID_282` (version `13.0.2`) and `UNKNOWN_HWID_286` (version `4.0.2`); do not infer names from the versions or unrelated numeric constants elsewhere in the source tree.

The captured live sysfs versions and raw discovery artifact remain evidence for the numeric IDs and versions. This source lookup adds evidence that the public Arch tag's generic mapping tables do not name them; it does not rule out local patches, generated mappings, or another AMD-specific table outside the inspected files.

## Firmware evidence boundary

The same source checkout is suitable for correlating source call sites and firmware declarations with the observed Navi 33 IP versions. The existing `firmware-source-correlation.md` already reports 14 path references and local filename candidates. That is a static source-to-name inventory; the kernel journal does not expose every successful firmware request, and the package-to-source match is still unverified. Do not describe filename candidates as confirmed loaded payloads or as macOS dependencies.

## Next evidence to collect from Linux

1. Obtain the installed/cached `linux 7.2.11.arch1-1` package's `.BUILDINFO`, checksum, and signature verification result.
2. Obtain the exact PKGBUILD, kernel config, and patch list for that package build.
3. Compare those inputs with commit `64a90897d3c539033f24105968e3513957e70819`.
4. If the user can provide a complete boot journal or kernel firmware request trace, map only explicit request/load records to the source call sites; leave requests that are not observable marked unconfirmed.

No kernel, firmware, hardware register, package, or boot setting was modified for this reconciliation. No tests were run.
