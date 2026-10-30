# Navi 33 IP Discovery parser

## Scope

`Navi33IpDiscovery` is an offline, bounded parser for AMD Discovery binary v1/v2 with IP Discovery table v3. It is currently scoped to the format in the RX 7600 capture at `RX7600-Linux-Evidence/amdgpu_discovery.bin`.

It validates:
- the binary signature, supported binary major version, declared extent, and binary checksum;
- the bounded table directory and the IP_DISCOVERY table extent;
- the IPDS signature, declared size, IPDS table checksum, and supported IPDS version;
- die offsets, die identities, per-die record counts, variable record extents, and base-address count bounds.

The parser preserves numeric HWIDs, instance IDs, versions, variant/sub-revision nibbles, die index, and raw IP Discovery base-address metadata. It deliberately does not assign symbolic names to unknown IDs 282 and 286.

## Connection to target profile

The parser output is converted directly into numeric `Navi33IpObservation` records. The profile validator checks the required RX 7600 records by numeric HWID and instance:

| IP block label (source-correlated) | HWID | Instance | Version |
|---|---:|---:|---:|
| GC | 11 | 0 | 11.0.2 |
| SDMA0 | 42 | 0 | 6.0.2 |
| SDMA1 | 43 | 0 | 6.0.2 |
| UVD | 12 | 0 | 4.0.4 |
| MP0 | 255 | 0 | 13.0.7 |
| MP1 | 1 | 0 | 13.0.7 |
| DMU | 271 | 0 | 3.2.1 |

A successful profile means only that the captured metadata matches these expected values. It does not authorize firmware loading, BAR mapping, MMIO, register writes, GPU initialization, or acceleration.

## Validation

The CTest target `navi33_ipdiscovery_capture_tests` reads the real capture and verifies:
- binary v1.3, declared size 1,280 bytes, IPDS v3, one die, 39 records;
- 30 unique numeric HWIDs, including the two unresolved numeric IDs;
- all seven critical version tuples and the direct profile-validator connection;
- fail-closed behavior for bad signatures, truncated input, insufficient output capacity, binary checksum mismatch, IPDS checksum mismatch, and unsupported IPDS version.

The existing 512-input malformed-buffer test for the separate Navi 48 ROM parser remains independent; this new parser does not reuse Navi 48 register maps or hardware initialization logic.

## Limitations

- Only IPDS table version 3 is currently accepted. A different table version fails closed.
- The parser is host-testable research code and is not wired into the IOKit bring-up kext.
- Raw base addresses are metadata from the capture, not host physical addresses or safe MMIO offsets.
- No firmware, VRAM/GART, PSP/SMU, command submission, display, or Metal path is enabled by this code.


## Cross-check against upstream Discovery structures (2026-10-30)

The parser layout was re-checked against the current Linux AMD Discovery definitions and consumer code:

- `die_info.die_offset` points to a `die_header` from the beginning of the discovery binary, not from the IP Discovery table. The Linux consumer forms `discovery_bin + die_offset`; this parser forms `binaryOffset + dieOffset` to also support a binary embedded at a non-zero aligned prefix in an input buffer.
- IPDS v3 records use the 8-byte fixed prefix: HWID, instance, base-address count, major, minor, revision and the variant/sub-revision byte. In the little-endian layout, sub-revision occupies the low nibble and variant the high nibble. The parser stores both fields separately.
- IPDS v3 base addresses are 32-bit. IPDS v4 introduces an explicit 32-/64-bit base-address selection; this parser intentionally rejects versions other than v3 rather than silently parsing v4 with 32-bit assumptions.
- Linux validates the IP Discovery table signature and checksum from the binary table directory. The parser does the same and additionally enforces its own hard bounds on every offset/count before reading.

Sources:
- Linux AMD Discovery structure definitions: https://gbmc.googlesource.com/linux/+/3ab394b363c5fd14b231e335fb6746ddfb93aaaa/drivers/gpu/drm/amd/include/discovery.h
- Linux AMD Discovery consumer (die offsets and table validation): https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c

The source describes the binary layout. It does not make the raw base addresses safe MMIO offsets and does not supply a macOS runtime transport for obtaining the discovery binary.
