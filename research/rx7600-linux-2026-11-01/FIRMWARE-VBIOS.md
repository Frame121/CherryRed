# VBIOS and firmware observations

**Capture date:** 2026-11-01
**Device:** AMD Navi 33 / RX 7600, PCI `1002:7480`

## VBIOS identification

- Kernel-reported VBIOS string: `113-EXT86626-001`.
- ROM image extracted locally from ACPI VFCT: 57,856 bytes; ROM signature `55 AA`; PCIR structure present.
- PCIR vendor/device: `1002:7480`; ROM checksum modulo 256: zero.
- Extracted ROM SHA-256: `7e57f8bd4f64eb33eedb7c1d95d32df10ee1680488ec4b3194a28d4741bb2c1f`.
- Decoded board: NAVI33, GDDR6, Fighter D74510 XL GDDR6 18Gbps 8GB.
- BIOS version: `022.003.001.027.000001`; build `00062039`; date 2023-04-25.

## Runtime firmware versions reported by Linux

| Component | Version |
|---|---:|
| ME | `0x9ec` |
| PFP | `0xa96` |
| MEC | `0xae6` |
| RLC | `0x61` |
| RLCP | `0x16` |
| RLCV | `0x13` |
| SDMA0 / SDMA1 | `0x18` |
| SMU | `82.97.0` |
| VCN | `0x09118037` |
| DMCUB | `0x07003600` |
| MES_KIQ | `0x113` |
| MES | `0x96` |

Candidate firmware-header version fields were cross-checked for GC ME/PFP/MEC/IMU/RLC, SDMA, VCN and DMCUB. This is a header/version consistency check; it is not proof of every firmware file selected, loaded or executed.

## Relevance and limits

These values help identify the board VBIOS and firmware generation present in Linux. They may support compatibility research and comparison with public AMD driver sources. They do not establish that these firmware images are usable by macOS or that Apple's driver stack supports Navi 33. The extracted ROM, firmware binaries, ACPI VFCT payload and raw ACPI tables are intentionally excluded.
