# Source references

The discovery wire layout was checked against
[`discovery.h`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/include/discovery.h)
at the same pinned Linux revision. Relevant structures are `binary_header`,
`binary_header_v2`, `table_info`, `ip_discovery_header`, `die_header`, `ip`,
`ip_v3` and `ip_v4`. This file carries an AMD MIT permission notice. The new
C++ implementation uses explicit byte decoding; no packed Linux structures
or hardware implementation are copied. Existing local strict Python parser
fixtures serve as a compatibility check, with additional rejection of
duplicate keys and mismatched die IDs. No Navi48 initialization code is linked.

Binary/table/die offsets are bytes from the binary start; record base values
are dword offsets. Record byte 7 has harvest/reserved nibbles for versions
1–2 and subrevision/variant nibbles for versions 3–4. The reader preserves
the raw byte rather than applying a board-specific interpretation. It does
not decode the separate harvest table, topology, or firmware selection.

The common firmware envelope layout was checked against
[`common_firmware_header` in Linux amdgpu_ucode.h](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_ucode.h#L30)
at commit `d0c647a6aae2ce577372594a176c82733e1ed267`. The file carries an AMD
MIT permission notice. This implementation decodes the documented fields
independently; no Linux loader, dispatcher, or hardware implementation was
copied. No firmware binaries are bundled.

| Byte offset | Width | Field |
|---|---|---|
| 0 | 4 | Entire container size |
| 4 | 4 | Header size |
| 8 / 10 | 2 each | Header major / minor |
| 12 / 14 | 2 each | Declared IP major / minor |
| 16 | 4 | Microcode version |
| 20 | 4 | Payload size |
| 24 | 4 | Payload offset from start of container |
| 28 | 4 | Declared payload CRC32 |

Only this common prefix is inspected. No CRC algorithm, supported per-IP
format, or firmware-to-board assignment is inferred. Future per-IP decoders
must account for the different versioned extensions and multiple subimages.
The existing Navi48 extractor explicitly targets RDNA4 and is not reused.

Kernel compile checks use
[`acidanthera/MacKernelSDK`](https://github.com/acidanthera/MacKernelSDK/tree/3f750085caa17ec3a7880f11c11bf4f48cd6a164),
pinned to `3f750085caa17ec3a7880f11c11bf4f48cd6a164`. This revision provides
`Library/x86_64/libkmod.a`; it does not provide `libkmodc++.a`. The probe build
therefore links `libkmod` as documented by this SDK. SDK files remain external
and retain their original licenses.
