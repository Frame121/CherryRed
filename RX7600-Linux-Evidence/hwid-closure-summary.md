# HWID mapping closure summary

Date: 2026-10-23
GPU: AMD RX 7600 / `1002:7480` / subsystem `148c:2421`
Raw discovery binary SHA-256: `4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`

## Final inventory result
- 39 discovery IP records
- 30 unique numeric HWIDs
- 28 IDs mapped to named entries in the fetched upstream source mapping
- 2 IDs explicitly catalogued as `UNKNOWN_HWID_282` and `UNKNOWN_HWID_286` for inventory completeness; these are neutral placeholders, not claimed hardware block names

| Numeric HWID | Instance | Reported version | Final inventory label | Evidence status |
|---:|---:|---|---|---|
| 282 | 0 | 13.0.2 | `UNKNOWN_HWID_282` | Live sysfs version corroborated; semantic name unresolved |
| 286 | 0 | 4.0.2 | `UNKNOWN_HWID_286` | Live sysfs version corroborated; semantic name unresolved |

The direct sysfs paths were read from `/sys/bus/pci/devices/0000:03:00.0/ip_discovery/die/0/282/0/` and `/sys/bus/pci/devices/0000:03:00.0/ip_discovery/die/0/286/0/`. Results matched 13.0.2 and 4.0.2 respectively.

## Closure decision
The inventory is complete for all 30 numeric IDs, but semantic source-name coverage is 28/30. Do not report 30/30 names resolved. Naming the remaining two requires a matching Arch kernel source/patch or authoritative AMD HWID documentation; guesses are not acceptable.

## Addendum — 2026-10-25 (definitive v7.2.11 source check)

Checked the locally cloned `v7.2.11-arch1` source (`RX7600-Linux-Evidence/source-investigation/
arch-linux-v7.2.11`, commit `64a90897d`) `drivers/gpu/drm/amd/include/soc15_hw_ip.h` directly. The
highest HWID constants defined are `ATU_HWID 294` and `AIGC_HWID 295`; the numeric range 275–293
is empty (gap between `DAZ_HWID 274` and `ATU_HWID 294`). **Neither 282 nor 286 is defined** in this
snapshot, confirming the unresolved status with a citable source line rather than a guess.

Known limitation: this clone is a `blob:none` partial checkout and does not contain the RDNA 3
dGPU asic file (`navi33.c`) or a `0x7480` entry in `ObjectID.h`, so the installed `7.2.11.arch1-1`
package's exact build recipe/patch set remains unproven against this source (the captured
`PKGBUILD.trunk` is for 6.3.2). Naming 282/286 therefore still requires the exact package source
or authoritative AMD documentation. No names are guessed.
