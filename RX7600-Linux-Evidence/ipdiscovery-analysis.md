# RX 7600 IP Discovery — preliminary parse (table-directory section superseded)

- Captured file: `/home/framelpo/rx7600-evidence/amdgpu_discovery.bin`
- File size: **10240 bytes**
- SHA-256: `4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`
- GPU identity recorded separately in manifest: PCI `1002:7480` (Navi 33)

## Validation
- Binary signature: `0x28211407` — valid
- Binary version: `1.3`
- Header-declared binary size: `1280` bytes (capture itself is 10240 bytes)
- Binary checksum: expected `0x5784`, calculated `0x5784` — **PASS**
- IP Discovery table: `IPDS`, version `3`, offset `60`, size `808`
- IP Discovery checksum: expected `0x49a2`, calculated `0x49a2` — **PASS**
- Dies: **1**; IP records parsed: **39**

## Table directory — superseded

**Important:** This preliminary table directory was decoded with the wrong stride and must not be used. See `parser-validation.md` for the corrected six-entry v1 directory and checksum audit.


| Table | Offset | Stored checksum |
|---|---:|---:|
| IP_DISCOVERY | 60 | `0x49a2` |
| GC | 808 | `0x0000` |
| HARVEST_INFO | 868 | `0x02d7` |
| VCN_INFO | 132 | `0x0000` |
| MALL_INFO | 1000 | `0x0131` |
| NPS_INFO | 136 | `0x0000` |

## IP records

| Die | HW ID | Instance | Version | Base-address count | Base addresses (raw) |
|---:|---:|---:|---|---:|---|
| 0 | 35 | 0 | 3.0.2 | 2 | `0x00000c00, 0x02408c00` |
| 0 | 6 | 0 | 13.0.7 | 2 | `0x00016c00, 0x02401800` |
| 0 | 6 | 1 | 13.0.7 | 2 | `0x00016e00, 0x02401c00` |
| 0 | 6 | 2 | 13.0.7 | 2 | `0x00017000, 0x02402000` |
| 0 | 6 | 3 | 13.0.7 | 2 | `0x00017200, 0x02402400` |
| 0 | 6 | 4 | 13.0.7 | 2 | `0x0001b000, 0x0242d800` |
| 0 | 6 | 5 | 13.0.7 | 2 | `0x0001b200, 0x0242dc00` |
| 0 | 6 | 6 | 13.0.7 | 2 | `0x0001b400, 0x0242e000` |
| 0 | 47 | 0 | 13.0.7 | 2 | `0x00017e00, 0x0240bc00` |
| 0 | 36 | 0 | 3.0.0 | 2 | `0x000001c0, 0x02409000` |
| 0 | 271 | 0 | 3.2.1 | 5 | `0x00000012, 0x000000c0, 0x000034c0, 0x00009000, 0x02403c00` |
| 0 | 46 | 0 | 4.4.0 | 5 | `0x00007000, 0x00c00000, 0x0240b800, 0x07c00000, 0x12400000` |
| 0 | 37 | 0 | 5.0.0 | 2 | `0x00000580, 0x02409400` |
| 0 | 49 | 0 | 2.0.0 | 3 | `0x000005a0, 0x00b80000, 0x0240c400` |
| 0 | 272 | 0 | 127.127.63 | 1 | `0x02404000` |
| 0 | 5 | 0 | 13.0.7 | 2 | `0x00017400, 0x02401400` |
| 0 | 11 | 0 | 11.0.2 | 4 | `0x00001260, 0x0000a000, 0x0001c000, 0x02402c00` |
| 0 | 274 | 0 | 127.127.63 | 2 | `0x004c0000, 0x02404800` |
| 0 | 41 | 0 | 6.0.1 | 2 | `0x00000f20, 0x0240a400` |
| 0 | 91 | 0 | 6.0.2 | 2 | `0x00011400, 0x02456800` |
| 0 | 34 | 0 | 3.0.2 | 2 | `0x0001a000, 0x02408800` |
| 0 | 255 | 0 | 13.0.7 | 5 | `0x00016000, 0x00dc0000, 0x00e00000, 0x00e40000, 0x0243fc00` |
| 0 | 1 | 0 | 13.0.7 | 5 | `0x00016000, 0x00dc0000, 0x00e00000, 0x00e40000, 0x0243fc00` |
| 0 | 108 | 0 | 4.3.1 | 6 | `0x00000000, 0x00000014, 0x00000d20, 0x00010400, 0x0241b000, 0x04040000` |
| 0 | 40 | 0 | 6.0.2 | 2 | `0x000010a0, 0x0240a000` |
| 0 | 70 | 0 | 7.6.0 | 6 | `0x00000000, 0x00000014, 0x00000d20, 0x00010400, 0x0241b000, 0x04040000` |
| 0 | 10 | 0 | 9.0.0 | 0 | `none` |
| 0 | 42 | 0 | 6.0.2 | 4 | `0x00001260, 0x0000a000, 0x0001c000, 0x02402c00` |
| 0 | 43 | 0 | 6.0.2 | 4 | `0x00001260, 0x0000a000, 0x0001c000, 0x02402c00` |
| 0 | 282 | 0 | 13.0.2 | 1 | `0x00bc0000` |
| 0 | 4 | 0 | 13.0.8 | 4 | `0x00016800, 0x00016a00, 0x02401000, 0x03440000` |
| 0 | 128 | 0 | 2.3.0 | 3 | `0x00000ea0, 0x00500000, 0x02420000` |
| 0 | 3 | 0 | 13.0.7 | 2 | `0x00016600, 0x02400c00` |
| 0 | 150 | 0 | 8.11.0 | 2 | `0x00014000, 0x02425800` |
| 0 | 150 | 1 | 8.11.0 | 2 | `0x00054000, 0x02425c00` |
| 0 | 150 | 2 | 8.11.0 | 2 | `0x00094000, 0x02426000` |
| 0 | 150 | 3 | 8.11.0 | 2 | `0x000d4000, 0x02426400` |
| 0 | 12 | 0 | 4.0.4 | 2 | `0x00007800, 0x00007e00` |
| 0 | 286 | 0 | 4.0.2 | 1 | `0x02403000` |

## Interpretation limits

- The capture file is 10 KiB, but its embedded binary header declares a 1280-byte binary. The checksum checks above validate the declared binary region and IP Discovery table only.
- Hardware IDs are kept numeric rather than assigned guessed names.
- Base addresses are metadata only. Do not use these values for direct register access.
- This confirms readable RX 7600 IP Discovery metadata, not macOS driver compatibility or graphics acceleration.
