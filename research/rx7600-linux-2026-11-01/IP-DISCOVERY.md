# AMD IP Discovery — RX 7600 / Navi 33

**Capture date:** 2026-11-01
**PCI identity:** `1002:7480` (subsystem `148c:2421`, revision `cf`)

## Export validation

- Exported binary: 10,240 bytes; header-declared binary size: 1,280 bytes. The 8,960-byte trailing padding was excluded from parsing.
- SHA-256: `e6381d289c19cb097a8366216bcad579623457cce5adbafae9b81b355164a2a8`.
- Header v1.3; signature `0x28211407`; header checksum `0x5784` — valid.
- IP Discovery v3; 808-byte structure; one die; 39 IP records.
- IP_DISCOVERY, GC, HARVEST_INFO, VCN_INFO and MALL_INFO checksums passed. NPS_INFO was absent.
- 37 sysfs IP aliases were cross-checked; all 37 matched decoded IP versions and base-address lists.

## Selected IP blocks

| Block | Reported version |
|---|---:|
| GC | 11.0.2 |
| SDMA0 / SDMA1 | 6.0.2 |
| UMC | 8.11.0 |
| DMU | 3.2.1 |
| MP0 / MP1 | 13.0.7 |

Selected GC topology fields: 2 shader engines, 2 shader arrays per shader engine, 4 WGP per shader array, wave size 32; kernel-reported active CU count was 32.

## Interpretation

The export and sysfs cross-check provide a useful hardware/IP inventory for a Navi 33-specific research profile. These values can guide investigation of IP block identification, version-dependent initialization and resource discovery. They do not provide a macOS driver implementation, command submission ABI, firmware interface compatibility, or Metal support.

The raw binary is retained in the local evidence archive; this document publishes only its checksum and decoded summary.
