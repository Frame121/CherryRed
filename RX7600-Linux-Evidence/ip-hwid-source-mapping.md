# RX 7600 IP Discovery — HWID source mapping

**Capture device:** AMD Radeon RX 7600 (`1002:7480`, subsystem `148c:2421`)  
**Capture kernel:** `7.2.11-arch1-1`  
**Raw artifact:** `amdgpu_discovery.bin`  
**Raw SHA-256:** `4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`

## Result

The captured IP Discovery payload has **39 IP records across 30 unique numeric HWIDs**. The binary checksum and IPDS table checksum passed validation. The mapping below cross-references the numeric IDs against Linux upstream's `soc15_hw_ip.h` HWID constants and `amdgpu_discovery.c` name table. The upstream files fetched are `master`, not yet proven byte-for-byte identical to the Archcraft kernel's source revision; therefore treat symbol mapping as source-correlated, not a claim that this is the exact build source.

| Numeric HWID | Name from source table | Source symbol | Instance(s) | Version(s) | Records |
|---:|---|---|---|---|---:|
| 1 | MP1 | `MP1_HWID` | 0 | 13.0.7 | 1 |
| 3 | THM | `THM_HWID` | 0 | 13.0.7 | 1 |
| 4 | SMUIO | `SMUIO_HWID` | 0 | 13.0.8 | 1 |
| 5 | FUSE | `FUSE_HWID` | 0 | 13.0.7 | 1 |
| 6 | CLKA | `CLKA_HWID` | 0, 1, 2, 3, 4, 5, 6 | 13.0.7 | 7 |
| 10 | PWR | `PWR_HWID` | 0 | 9.0.0 | 1 |
| 11 | GC | `GC_HWID` | 0 | 11.0.2 | 1 |
| 12 | UVD | `UVD_HWID` | 0 | 4.0.4 | 1 |
| 34 | MMHUB | `MMHUB_HWID` | 0 | 3.0.2 | 1 |
| 35 | ATHUB | `ATHUB_HWID` | 0 | 3.0.2 | 1 |
| 36 | DBGU_NBIO | `DBGU_NBIO_HWID` | 0 | 3.0.0 | 1 |
| 37 | DFX | `DFX_HWID` | 0 | 5.0.0 | 1 |
| 40 | OSSSYS | `OSSSYS_HWID` | 0 | 6.0.2 | 1 |
| 41 | HDP | `HDP_HWID` | 0 | 6.0.1 | 1 |
| 42 | SDMA0 | `SDMA0_HWID` | 0 | 6.0.2 | 1 |
| 43 | SDMA1 | `SDMA1_HWID` | 0 | 6.0.2 | 1 |
| 46 | DF | `DF_HWID` | 0 | 4.4.0 | 1 |
| 47 | CLKB | `CLKB_HWID` | 0 | 13.0.7 | 1 |
| 49 | DFX_DAP | `DFX_DAP_HWID` | 0 | 2.0.0 | 1 |
| 70 | PCIE | `PCIE_HWID` | 0 | 7.6.0 | 1 |
| 91 | LSDMA | `LSDMA_HWID` | 0 | 6.0.2 | 1 |
| 108 | NBIF | `NBIF_HWID` | 0 | 4.3.1 | 1 |
| 128 | SYSTEMHUB | `SYSTEMHUB_HWID` | 0 | 2.3.0 | 1 |
| 150 | UMC | `UMC_HWID` | 0, 1, 2, 3 | 8.11.0 | 4 |
| 255 | MP0 | `MP0_HWID` | 0 | 13.0.7 | 1 |
| 271 | DMU | `DMU_HWID` | 0 | 3.2.1 | 1 |
| 272 | DIO | `DIO_HWID` | 0 | 127.127.63 | 1 |
| 274 | DAZ | `DAZ_HWID` | 0 | 127.127.63 | 1 |
| 282 | UNRESOLVED in checked Arch v7.2.11 source snapshot | `` | 0 | 13.0.2 | 1 |
| 286 | UNRESOLVED in checked Arch v7.2.11 source snapshot | `` | 0 | 4.0.2 | 1 |

## Source references

- [`soc15_hw_ip.h` — HWID numeric constants](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/master/drivers/gpu/drm/amd/include/soc15_hw_ip.h)
- [`amdgpu_discovery.c` — HWID names and discovery parser](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/master/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c)
- [`discovery.h` — binary/table structures](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/master/drivers/gpu/drm/amd/include/discovery.h)

## Interpretation limits

- Numeric HWID is not the same thing as the driver's ordered `HWIP` block index. A name match alone does not establish that every discovered block is initialized or supported by a particular OS.
- Version strings are read from the captured binary, not inferred from names.
- No register addresses are reproduced here.
- This evidence demonstrates Linux-side discovery on this RX 7600. It does **not** demonstrate macOS driver support, acceleration, or a working Cobalt64 kext.

## Closure update — 2026-10-24

Mapping pass is complete as far as the checked source tables permit: **28/30 HWIDs have source-correlated names; 2/30 remain explicitly unresolved**. The complete Arch Linux `v7.2.11-arch1` source snapshot at peeled commit `64a90897d3c539033f24105968e3513957e70819` was inspected locally; it contains no named HWID constants or discovery name-table entries for 282 and 286. The available `PKGBUILD.trunk` is for 6.3.2, so the source snapshot is not proven to match the installed 7.2.11 package binary. See [`ARCH-V7.2.11-HWID-AUDIT.md`](source-investigation/ARCH-V7.2.11-HWID-AUDIT.md).

- HWID `282`, instance `0`, version `13.0.2`: directly cross-checked against live sysfs at `/sys/bus/pci/devices/0000:03:00.0/ip_discovery/die/0/282/0/` (`major=13`, `minor=0`, `revision=2`). No matching named entry was found in the fetched `soc15_hw_ip.h` / `amdgpu_discovery.c` mapping tables.
- HWID `286`, instance `0`, version `4.0.2`: directly cross-checked against live sysfs at `/sys/bus/pci/devices/0000:03:00.0/ip_discovery/die/0/286/0/` (`major=4`, `minor=0`, `revision=2`). No matching named entry was found in the fetched source mapping tables.

**Status: mapping checked against the locally available Arch source snapshot, with two documented unknowns—not 30/30 named.** The names remain unknown in this snapshot. Exact installed-binary provenance still requires the `7.2.11.arch1-1` build recipe/patch set or build records. No names are guessed.
