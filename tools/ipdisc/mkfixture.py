#!/usr/bin/env python3
"""Generate deterministic synthetic AMD IP Discovery fixtures."""

import hashlib
import json
import os
import struct
import sys
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Tuple


BINARY_SIGNATURE = 0x28211407
IPDS_SIGNATURE = 0x53445049
MIN_BINARY_SIZE = 512
IPDS_HEADER_SIZE = 80
TABLES_MAX = 6
V1_HEADER_LIST_OFFSET = 12
V1_TABLE_LIST_END = V1_HEADER_LIST_OFFSET + TABLES_MAX * 8
V2_HEADER_LIST_OFFSET = 16
V2_TABLE_LIST_END = V2_HEADER_LIST_OFFSET + 8


@dataclass(frozen=True)
class FixtureSpec:
    description: str
    should_parse_strict: bool
    reason: str


class FixtureGenerator:
    """Build fixtures in memory; no fixture represents real hardware data."""

    def _build(
        self,
        entries: Iterable[Tuple[int, int, Tuple[int, int, int], List[int]]],
        binary_version: int = 1,
        ipds_version: int = 3,
        num_dies: int = 1,
        ipd_offset_override: Optional[int] = None,
    ) -> bytes:
        entry_list = list(entries)
        header_list_offset = (
            V2_HEADER_LIST_OFFSET if binary_version >= 2 else V1_HEADER_LIST_OFFSET
        )
        table_list_end = (
            V2_TABLE_LIST_END if binary_version >= 2 else V1_TABLE_LIST_END
        )
        ipd_offset = (
            ipd_offset_override
            if ipd_offset_override is not None
            else ((table_list_end + 3) // 4) * 4
        )
        die_offset = ipd_offset + IPDS_HEADER_SIZE

        records = bytearray()
        for hw_id, instance, version, bases in entry_list:
            if len(bases) > 255:
                raise ValueError("fixture base count must fit in one byte")
            records.extend(
                struct.pack(
                    "<HBBBBBB",
                    hw_id,
                    instance,
                    len(bases),
                    version[0],
                    version[1],
                    version[2],
                    0,
                )
            )
            for base in bases:
                records.extend(struct.pack("<I", base))

        table_size = IPDS_HEADER_SIZE + 4 + len(records)
        ipds = bytearray(IPDS_HEADER_SIZE)
        struct.pack_into("<IHHIH", ipds, 0, IPDS_SIGNATURE, ipds_version, table_size, 0, num_dies)
        struct.pack_into("<HH", ipds, 14, 0, die_offset)
        if ipds_version == 4:
            ipds[78:80] = b"\x00\x00"
        ipds.extend(struct.pack("<HH", 0, len(entry_list)))
        ipds.extend(records)

        binary = bytearray(MIN_BINARY_SIZE)
        struct.pack_into("<IHHHH", binary, 0, BINARY_SIGNATURE, binary_version, 0, 0, 0)
        if binary_version >= 2:
            struct.pack_into("<HH", binary, 12, 1, 0)
        struct.pack_into(
            "<HHHH",
            binary,
            header_list_offset,
            ipd_offset,
            0,
            table_size,
            0,
        )
        binary[ipd_offset : ipd_offset + table_size] = ipds
        struct.pack_into("<H", binary, 10, len(binary))
        self._refresh_checksums(binary)
        return bytes(binary)

    @staticmethod
    def _refresh_checksums(binary: bytearray) -> None:
        major = struct.unpack_from("<H", binary, 4)[0]
        table_list = 16 if major >= 2 else 12
        ipd_offset, _, _, _ = struct.unpack_from(
            "<HHHH", binary, table_list
        )
        ipd_size = struct.unpack_from("<H", binary, ipd_offset + 6)[0]
        table_checksum = sum(binary[ipd_offset : ipd_offset + ipd_size]) & 0xFFFF
        struct.pack_into("<H", binary, table_list + 2, table_checksum)
        FixtureGenerator._refresh_binary_checksum(binary)

    @staticmethod
    def _refresh_binary_checksum(binary: bytearray) -> None:
        binary_size = struct.unpack_from("<H", binary, 10)[0]
        checksum = sum(binary[10:binary_size]) & 0xFFFF
        struct.pack_into("<H", binary, 8, checksum)

    @staticmethod
    def _table_list(binary: bytearray) -> int:
        major = struct.unpack_from("<H", binary, 4)[0]
        return V2_HEADER_LIST_OFFSET if major >= 2 else V1_HEADER_LIST_OFFSET

    @classmethod
    def _ipd_offset(cls, binary: bytearray) -> int:
        return struct.unpack_from("<H", binary, cls._table_list(binary))[0]

    @classmethod
    def _die_offset(cls, binary: bytearray) -> int:
        return struct.unpack_from("<H", binary, cls._ipd_offset(binary) + 16)[0]

    def generate_valid_minimal(self) -> bytes:
        return self._build([(11, 0, (12, 0, 1), [0x40000])])

    def generate_valid_multi_record(self) -> bytes:
        return self._build(
            [
                (11, 0, (12, 0, 1), [0x40000, 0x50000]),
                (42, 1, (6, 1, 0), [0x60000, 0x61000, 0x62000]),
            ]
        )

    def generate_valid_v2_header(self) -> bytes:
        return self._build(
            [(11, 0, (12, 0, 1), [0x40000])], binary_version=2
        )

    def generate_valid_v4_table(self) -> bytes:
        return self._build(
            [(11, 0, (12, 0, 1), [0x40000])], ipds_version=4
        )

    def generate_table_overlapping_binary_header(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        struct.pack_into("<H", binary, self._table_list(binary), 8)
        self._refresh_binary_checksum(binary)
        return bytes(binary)

    def generate_table_overlapping_table_list(self) -> bytes:
        return self._build(
            [(11, 0, (12, 0, 1), [0x40000])],
            ipd_offset_override=40,
        )

    def generate_die_header_overlapping_ipds_header(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        ipd_offset = self._ipd_offset(binary)
        struct.pack_into("<H", binary, ipd_offset + 16, ipd_offset + 79)
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_compatibility_truncated_record_at_input_end(self) -> bytes:
        binary = bytearray(MIN_BINARY_SIZE)
        ipd_offset = 64
        die_offset = MIN_BINARY_SIZE - 12
        table_size = MIN_BINARY_SIZE - ipd_offset
        struct.pack_into(
            "<IHHHH", binary, 0, BINARY_SIGNATURE, 1, 0, 0, MIN_BINARY_SIZE
        )
        struct.pack_into(
            "<HHHH", binary, V1_HEADER_LIST_OFFSET, ipd_offset, 0, table_size, 0
        )
        struct.pack_into(
            "<IHHIH", binary, ipd_offset, IPDS_SIGNATURE, 3, table_size, 0, 1
        )
        struct.pack_into("<HH", binary, ipd_offset + 14, 0, die_offset)
        struct.pack_into("<HH", binary, die_offset, 0, 1)
        struct.pack_into(
            "<HBBBBBB", binary, die_offset + 4, 11, 0, 1, 12, 0, 1, 0
        )
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_invalid_signature(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        struct.pack_into("<I", binary, 0, 0xDEADBEEF)
        return bytes(binary)

    def generate_invalid_checksum(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        binary[8] ^= 1
        return bytes(binary)

    def generate_invalid_table_checksum(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        table_list = self._table_list(binary)
        binary[table_list + 2] ^= 1
        self._refresh_binary_checksum(binary)
        return bytes(binary)

    def generate_invalid_ipds_signature(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        struct.pack_into("<I", binary, self._ipd_offset(binary), 0xDEADBEEF)
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_invalid_offset(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        struct.pack_into("<H", binary, self._table_list(binary), 0xFFFF)
        self._refresh_binary_checksum(binary)
        return bytes(binary)

    def generate_zero_ip_count(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        die_offset = self._die_offset(binary)
        struct.pack_into("<H", binary, die_offset + 2, 0)
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_excessive_ip_count(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        die_offset = self._die_offset(binary)
        struct.pack_into("<H", binary, die_offset + 2, 300)
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_excessive_base_count(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        die_offset = self._die_offset(binary)
        binary[die_offset + 4 + 3] = 17
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_truncated_record(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        die_offset = self._die_offset(binary)
        binary[die_offset + 4 + 3] = 2
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_unsupported_version(self) -> bytes:
        binary = bytearray(self.generate_valid_minimal())
        struct.pack_into("<H", binary, self._ipd_offset(binary) + 4, 99)
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_unsupported_binary_version(self) -> bytes:
        binary = bytearray(
            self._build(
                [(11, 0, (12, 0, 1), [0x40000])], binary_version=3
            )
        )
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_unsupported_64bit_bases(self) -> bytes:
        binary = bytearray(self.generate_valid_v4_table())
        binary[self._ipd_offset(binary) + 78] = 1
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_multiple_dies(self) -> bytes:
        binary = bytearray(
            self._build(
                [(11, 0, (12, 0, 1), [0x40000])],
                num_dies=2,
            )
        )
        self._refresh_checksums(binary)
        return bytes(binary)

    def generate_truncated_header(self) -> bytes:
        binary = bytearray(MIN_BINARY_SIZE)
        struct.pack_into("<IHHHH", binary, 0, BINARY_SIGNATURE, 1, 0, 0, 15)
        return bytes(binary)

    def generate_all(self) -> Dict[str, bytes]:
        return {
            "valid_minimal.bin": self.generate_valid_minimal(),
            "valid_multi_record.bin": self.generate_valid_multi_record(),
            "valid_v2_header.bin": self.generate_valid_v2_header(),
            "valid_v4_table.bin": self.generate_valid_v4_table(),
            "table_overlaps_binary_header.bin": self.generate_table_overlapping_binary_header(),
            "table_overlaps_table_list.bin": self.generate_table_overlapping_table_list(),
            "die_overlaps_ipds_header.bin": self.generate_die_header_overlapping_ipds_header(),
            "compatibility_truncated_record.bin": self.generate_compatibility_truncated_record_at_input_end(),
            "invalid_signature.bin": self.generate_invalid_signature(),
            "invalid_checksum.bin": self.generate_invalid_checksum(),
            "invalid_table_checksum.bin": self.generate_invalid_table_checksum(),
            "invalid_ipds_signature.bin": self.generate_invalid_ipds_signature(),
            "invalid_offset.bin": self.generate_invalid_offset(),
            "zero_ip_count.bin": self.generate_zero_ip_count(),
            "excessive_ip_count.bin": self.generate_excessive_ip_count(),
            "excessive_base_count.bin": self.generate_excessive_base_count(),
            "truncated_record.bin": self.generate_truncated_record(),
            "unsupported_version.bin": self.generate_unsupported_version(),
            "unsupported_binary_version.bin": self.generate_unsupported_binary_version(),
            "unsupported_64bit_bases.bin": self.generate_unsupported_64bit_bases(),
            "multiple_dies.bin": self.generate_multiple_dies(),
            "truncated_header.bin": self.generate_truncated_header(),
        }

    @staticmethod
    def _spec(name: str) -> FixtureSpec:
        specs = {
            "valid_minimal.bin": FixtureSpec("One valid GC record.", True, "Valid synthetic binary."),
            "valid_multi_record.bin": FixtureSpec("Two records with multiple bases.", True, "Valid synthetic binary."),
            "valid_v2_header.bin": FixtureSpec("Valid version-2 binary header.", True, "Valid synthetic binary."),
            "valid_v4_table.bin": FixtureSpec("IP Discovery version 4 with 32-bit bases.", True, "Valid synthetic binary."),
            "table_overlaps_binary_header.bin": FixtureSpec("IP Discovery starts inside the binary header.", False, "Strict mode rejects table/header overlap."),
            "table_overlaps_table_list.bin": FixtureSpec("IP Discovery starts inside the complete v1 table list.", False, "Strict mode rejects table/table-list overlap."),
            "die_overlaps_ipds_header.bin": FixtureSpec("Die header starts inside the fixed IP Discovery header.", False, "Strict mode rejects die/header overlap."),
            "compatibility_truncated_record.bin": FixtureSpec("Base address starts at the supplied input boundary.", False, "Strict mode rejects a base array beyond the declared table and input."),
            "invalid_signature.bin": FixtureSpec("Incorrect binary signature.", False, "Binary signature mismatch."),
            "invalid_checksum.bin": FixtureSpec("Incorrect binary checksum.", False, "Binary checksum mismatch."),
            "invalid_table_checksum.bin": FixtureSpec("Incorrect table checksum.", False, "Strict mode checks table checksum."),
            "invalid_ipds_signature.bin": FixtureSpec("Incorrect IPDS signature.", False, "IPDS signature mismatch."),
            "invalid_offset.bin": FixtureSpec("Out-of-range table offset.", False, "Table points beyond binary."),
            "zero_ip_count.bin": FixtureSpec("Die declares zero IP records.", False, "IP count must be nonzero."),
            "excessive_ip_count.bin": FixtureSpec("Die declares more than 256 records.", False, "IP count exceeds parser limit."),
            "excessive_base_count.bin": FixtureSpec("Record declares 17 bases.", False, "Strict mode caps records at 16 bases."),
            "truncated_record.bin": FixtureSpec("Record base array exceeds table extent.", False, "Strict mode rejects truncated records."),
            "unsupported_version.bin": FixtureSpec("IPDS version 99.", False, "Strict mode supports IPDS versions 1 through 4."),
            "unsupported_binary_version.bin": FixtureSpec("Binary header version 3.", False, "Strict mode supports binary header versions 0 through 2."),
            "unsupported_64bit_bases.bin": FixtureSpec("IPDS v4 sets the 64-bit-base flag.", False, "Strict mode does not parse 64-bit base arrays."),
            "multiple_dies.bin": FixtureSpec("Binary declares two dies.", False, "Strict mode is intentionally single-die."),
            "truncated_header.bin": FixtureSpec("Binary header declares a size below its minimum.", False, "Binary size is smaller than 16 bytes."),
        }
        return specs[name]

    def generate_manifest(self, fixtures: Dict[str, bytes]) -> Dict[str, object]:
        return {
            "generator": "mkfixture.py",
            "version": "2.0",
            "fixtures": [
                {
                    "name": name,
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "size": len(data),
                    "description": self._spec(name).description,
                    "should_parse_strict": self._spec(name).should_parse_strict,
                    "reason": self._spec(name).reason,
                }
                for name, data in fixtures.items()
            ],
        }


def main() -> int:
    output_dir = sys.argv[1] if len(sys.argv) > 1 else "fixtures"
    os.makedirs(output_dir, exist_ok=True)
    generator = FixtureGenerator()
    fixtures = generator.generate_all()

    for name, data in fixtures.items():
        path = os.path.join(output_dir, name)
        with open(path, "wb") as fixture_file:
            fixture_file.write(data)
        print(f"Wrote {path} ({len(data)} bytes)")

    manifest_path = os.path.join(output_dir, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8", newline="\n") as manifest_file:
        json.dump(generator.generate_manifest(fixtures), manifest_file, indent=2)
        manifest_file.write("\n")
    print(f"Wrote {manifest_path}")
    print(f"Generated {len(fixtures)} synthetic fixtures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
