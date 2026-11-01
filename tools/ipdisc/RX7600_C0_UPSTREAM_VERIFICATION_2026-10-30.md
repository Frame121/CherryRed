# RX 7600 C0 — upstream dispatch and firmware verification

**Date:** 2026-10-30  
**Scope:** read-only source research and local evidence inventory. No GPU access, firmware download, ROM/MMIO read, driver edit, EFI edit, kext load, or reboot.

## Outcome

C0 is **partially complete**. Source-level firmware families can be listed as conditional candidates, but the exact IP versions and firmware set of the physical RX 7600 remain **UNRESOLVED** until a provenance-documented IP Discovery/VBIOS artifact is obtained and attributed to the board.

The Mac checkout was searched for IP Discovery/VBIOS-like binary artifacts. No candidate board dump was found. The tools/ipdisc/evidence/ directory contains prior Windows/Mac inventory JSON and reports, not an IP Discovery binary. The previously referenced /home/framelpo/rx7600-evidence/amdgpu_discovery.bin is not present in this Mac checkout or under the inspected /Users/framelpo tree.

## Hardware identity evidence

The recovery checkpoint records a Windows PnP identity:
PCI\VEN_1002&DEV_7480&SUBSYS_2421148C&REV_CF.
This establishes presence and PCI identity on that Windows host according to the saved checkpoint. It does **not** establish GC/SDMA/PSP/SMU IP Discovery versions, firmware requests, VBIOS identity, or macOS support.

The string gfx1102 remains a supplied architecture label, not a measured IP Discovery version. Do not turn it into an assumption that GC must report IP 11.0.2.

## Upstream source interpretation

Linux AMDGPU obtains per-block versions from IP Discovery and dispatches IP implementations/firmware prefixes from those records. A version switch case proves only that Linux has a path for that reported version; it does not assign that version to PCI ID 1002:7480.

The Linux amdgpu_discovery.c source documents the discovery flow and version dispatch. gfx_v11_0.c advertises GC 11.0.x firmware names. smu_v13_0.c obtains the SMU firmware prefix from the MP1 IP version. Current Linux source pages are rolling upstream references; they are not a substitute for re-checking every line against the project's historical pinned Linux commit.

References:
- https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c
- https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c
- https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/pm/swsmu/smu13/smu_v13_0.c
- https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware/+/6569484e6b57bc1c688b383ba6719bf2e25574c4/WHENCE

## Conditional firmware candidate inventory

These names are candidate names only when the corresponding block version is actually reported. Presence in linux-firmware does not establish that this board requests them.

| IP block / condition | Conditional firmware names | Evidence status |
|---|---|---|
| GC 11.0.2 | gc_11_0_2_pfp.bin, gc_11_0_2_me.bin, gc_11_0_2_mec.bin, gc_11_0_2_rlc.bin; additional IMU/MES files exist in WHENCE | Candidate files listed in linux-firmware WHENCE; board's GC version and whether optional paths are used remain unresolved |
| SDMA 6.0.2 | sdma_6_0_2.bin | Candidate file listed; board's SDMA version/instance count unresolved |
| MP0 / PSP 13.0.4 | psp_13_0_4_toc.bin, psp_13_0_4_ta.bin | Candidate files listed; board's MP0 version and exact TA requirements unresolved |
| MP1 / SMU 13.0.7 | smu_13_0_7.bin | Candidate file listed; do not assume MP1 13.0.7 without IP Discovery evidence |
| UVD / VCN 4.0.4 | vcn_4_0_4.bin | Candidate file listed; board's UVD version and VCN instance count unresolved |
| DCE / display 3.2.1 | dcn_3_2_1_dmcub.bin | Candidate file listed; board's display IP version, connectors, PHY and VBIOS routing unresolved |

### Important correction to earlier notes

The previous IP/firmware map's example pairing of MP1 13.0.4 with smu_13_0_4.bin should **not** be carried forward as a known available firmware blob. The inspected linux-firmware WHENCE snapshot lists smu_13_0_0.bin and smu_13_0_7.bin, but not smu_13_0_4.bin. Keep MP1 13.0.4 as an unresolved source/version case until the exact Linux commit's version decode and firmware request are cross-checked; do not invent or fetch a .4 blob. The current SMU13 source uses the decoded MP1 prefix for the firmware filename.

Likewise, gc_11_0_2_*, sdma_6_0_2, psp_13_0_4_*, vcn_4_0_4, and dcn_3_2_1 are not a verified RX 7600 manifest. They are merely names present in the reviewed firmware inventory.

## Firmware licensing/provenance

The reviewed linux-firmware WHENCE snapshot labels the grouped AMDGPU firmware set Redistributable and points to LICENSE.amdgpu. This is a useful upstream permission/provenance lead, not a complete macOS redistribution decision. Before copying any binary into CherryRed, retain the exact firmware repository commit, matching license text, each blob's WHENCE origin/version metadata, and each binary's SHA-256. No firmware binary was downloaded or copied in this milestone.

## C0 decision

- **GO:** continue source-only cross-checking and strict offline parsing if an authentic board-attributed artifact is supplied.
- **NO-GO:** selecting an exact RX 7600 IP/firmware manifest from PCI ID, architecture label, generic Linux branches, or synthetic fixtures.
- **NO-GO:** implementing GFX11 initialization, queue/IB submission, PSP/SMU sequencing, firmware loading, MMIO, ROM BAR access, or kext activation without board-specific evidence and a separately reviewed recovery plan.
- **No change:** Navi48 personality/matches/initialization remain untouched. No RX 7600 support or Metal acceleration is claimed.

## Next required input

An existing, provenance-documented IP Discovery dump or VBIOS image for this exact board, with acquisition method, original SHA-256, and matching PCI IDs including subsystem IDs. If the previously mentioned Linux artifact exists on another machine or disk, copy that original file unchanged into the project evidence folder and record its original hash/custodian; do not recapture hardware as part of this step.
