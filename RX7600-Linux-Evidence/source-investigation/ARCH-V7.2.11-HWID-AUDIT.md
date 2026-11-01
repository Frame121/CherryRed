# Arch Linux v7.2.11 source snapshot: RX 7600 HWID audit

Date: 2026-10-24

## Source identity

- Local source tree: `arch-linux-v7.2.11/`
- Tag: `v7.2.11-arch1`
- Peeled commit: `64a90897d3c539033f24105968e3513957e70819`
- Installed RX 7600 IP Discovery records: captured from Linux `7.2.11.arch1-1`; see `../ipdiscovery-analysis.md`.

The tag is a matching version-family source snapshot. The available `PKGBUILD.trunk` declares `pkgver=6.3.2.arch1`, so it is not the recipe for the installed kernel. The local source snapshot is not proven to be byte-for-byte the source used to build that installed binary.

## Results

The RX 7600 capture contains HWID 282 instance 0 version 13.0.2 and HWID 286 instance 0 version 4.0.2.

Inspection of `drivers/gpu/drm/amd/include/soc15_hw_ip.h` found no named `*_HWID` constant assigned to 282 or 286. Inspection of the `hw_id_names` initializer in `drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c` found no explicit entries at either numeric index. Thus this source snapshot does not provide semantic names for those records. This is a bounded source-table finding; it does not identify their hardware function or prove that no other AMD documentation names them.

All 30 numeric IDs in the capture are inventoried; 28 have source-correlated names and these two remain explicitly unknown. Do not infer function from the IP version number, a nearby numeric ID, or unrelated register/symbol matches elsewhere in the kernel tree.

## Limits and next evidence

- This resolves whether the checked Arch source snapshot contains those names; it does not resolve installed-binary provenance.
- It provides Linux IP Discovery interpretation only. It is not a macOS register map, firmware recipe, initialization sequence, or proof of RX 7600 macOS compatibility.
- To close the package provenance gap, obtain the exact `7.2.11.arch1-1` PKGBUILD/patch set, `.BUILDINFO`, and/or reproducible build records, then compare their source revision and config with the local tag.
