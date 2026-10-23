# RX 7600 IP Discovery parser validation

- Artifact SHA-256: `4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`
- Captured file: 10240 bytes; header-declared binary: 1280 bytes
- Binary signature `0x28211407`; version 1.3
- Binary checksum: stored `0x5784`, calculated `0x5784` — **PASS**
- Project parser strict mode: **PASS**; IP records parsed: **39**
- IPDS table: signature `0x53445049`, version 3, size 808, stored checksum `0x49a2`, calculated `0x49a2`

## Correctly decoded v1 table directory

| Table | Offset | Stored checksum | Declared size | Checksum check | Range within declared binary |
|---|---:|---:|---:|---|---|
| IP_DISCOVERY | 60 | `0x49a2` | 808 | 0x49a2 PASS | yes |
| GC | 868 | `0x2d7` | 132 | 0x2d7 PASS | yes |
| HARVEST_INFO | 1000 | `0x131` | 136 | 0x131 PASS | yes |
| VCN_INFO | 1136 | `0x149` | 96 | 0x149 PASS | yes |
| MALL_INFO | 1232 | `0x157` | 48 | 0x157 PASS | yes |
| NPS_INFO | 0 | `0x0` | 0 | N/A (empty/zero-size) | yes |

## Notes

- Header v1 uses six 8-byte `table_info` entries. Each entry is offset, checksum, size, padding; do not decode entries as 4-byte pairs.
- The project parser validates the binary checksum, IPDS signature/size/checksum, die bounds and all 39 variable-length IP records in strict mode.
- Non-IPDS auxiliary table checksums are audited above, but do not count as parser-validated unless the parser explicitly checks their structure and checksum.
- The 10 KiB sysfs capture contains a 1280-byte declared binary region plus trailing bytes. Validation of the declared binary does not imply the trailing bytes are part of that binary.
- This is read-only offline analysis; base addresses are metadata only, not instructions for register access. This evidence does not establish macOS compatibility or acceleration.

Reference layout: Linux AMD `discovery.h` defines `table_info` as four uint16 fields and v1 `binary_header` as six table entries; `amdgpu_discovery.c` checks IPDS using its table header size and directory checksum. See https://codebrowser.dev/linux/linux/drivers/gpu/drm/amd/include/discovery.h.html and https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c.
