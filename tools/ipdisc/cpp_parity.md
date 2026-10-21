# C++ Compatibility and Strict Validation

The tool exposes two modes because the earlier Cobalt64 parser behavior and
offline validation have different goals:

- `compatibility` reproduces the behavior recorded before the C++ parser's
  2026-10-21 bounds hardening, as closely as its public API allows.
- `strict` is the default and rejects malformed or unsupported layouts before
  exposing any IP records.

Neither mode accesses hardware. The compatibility mode is not a claim that the
C++ implementation is safe for arbitrary untrusted files.

## Verified C++ behavior

The C++ parser:

- Reads little-endian values and scans for `0x28211407` on four-byte steps,
  considering only candidates with at least 512 bytes remaining.
- Checks a 16-bit wrapping byte sum from the binary-size field (byte 10) through
  the declared binary end.
- Uses the first table entry at binary-header offset 12 for major versions
  below 2, and offset 16 for versions 2 and above.
- Requires the IP Discovery table to start after the complete six-entry v1
  list or the counted v2 list, and bounds its extent by the declared binary
  size. It does not validate the table checksum.
- Requires the die header to start after the fixed 80-byte IP Discovery header
  and fit inside the declared table.
- Requires a nonzero IP Discovery table offset, table size of at least 80,
  `IPDS` signature, nonzero die count, nonzero first-die offset, and 1–256 IPs.
- Treats table and die offsets as relative to the binary start, and reads only
  the first die.
- Does not validate the table checksum or IP Discovery header version. IP
  records are read lazily but each requested record and its full declared base
  array must fit in the declared table. Their base count is clamped to 16 for
  returned values, while the next record position advances using the original
  count.
- Does not validate every record during `init`; a malformed later record is
  rejected only when that record is requested.

These facts are visible in `src/navi48-bringup/src/ipdiscovery.cpp`, especially
`validateBinary`, `init`, and `ipAt`. The C++ parser was tightened on
2026-10-21 to enforce declared binary/table extents. Python compatibility mode
continues to reproduce the previously verified C++ behavior, including its
limited bounds checks, and therefore intentionally differs from the current
bounded C++ parser in these cases. Failed record reads return `None`, matching
the C++ API's `false`.

## Intentional strict-mode differences

| Case | Compatibility mode | Strict mode |
|---|---|---|
| Binary header version | Uses the C++ `major >= 2` layout switch. | Accepts versions 0, 1, and 2; checks the v2 table count. |
| Table checksum | Not checked by C++. | Checks the IP Discovery table byte sum. |
| Table overlap | Earlier behavior did not reject overlap; current C++ requires the table to begin after the complete v1/v2 table list. | Requires the IP Discovery table to begin after the complete binary header and table list. |
| Table and record bounds | Current C++ bounds the table to the binary and each requested record to the table; `init` remains lazy. | Requires declared table and all record data to fit inside the binary/table extents during initialization. |
| Die-header overlap | Earlier behavior did not reject overlap; current C++ requires the die header after the fixed IP Discovery header. | Requires the full die header to start after the fixed 80-byte IP Discovery header and fit inside the table. |
| Die count | Requires nonzero, reads die 0. | Requires exactly one die. |
| IP Discovery version | Ignored by C++. | Supports versions 1–4 with 32-bit base addresses; rejects unknown versions and v4's 64-bit-base flag. |
| Base count | Clamps exposed bases to 16. | Rejects records declaring more than 16 bases. |
| Record validation | Happens when `ip_at` is called, with current C++ checking requested record bounds. | Validates every record during `init`. |
| Register byte offset overflow | Mirrors 32-bit C++ arithmetic. | Returns `None` if the result does not fit in 32 bits. |

Strict-mode format choices use AMD's Linux definitions in
`drivers/gpu/drm/amd/include/discovery.h`: packed `binary_header`,
`binary_header_v2`, `table_info`, `ip_discovery_header`, `die_info`, and the
`ip`, `ip_v3`, and `ip_v4` record layouts. The exact reference inspected during
M1.1 verification is upstream `torvalds/linux` commit
`d0c647a6aae2ce577372594a176c82733e1ed267`. Recording this pin does not prove
which revision the earlier moving-`master` lookup used. The pinned Linux
definition includes a six-entry v1 table list, a counted v2 table list, and a
v4 option for 64-bit base addresses. This parser explicitly rejects that
64-bit-base option rather than interpreting it as 32-bit data.

## Parity case inventory

`parity_cases.json` records compatibility cases and strict-mode differences.
Python compatibility tests retain the earlier behavior; separate C++ tests
exercise the newer declared-bound checks against synthetic data. These tests
do not establish any real GPU's IP Discovery table.
