#!/usr/bin/env python3
"""Offline parser for AMD IP Discovery binaries."""

import struct
from dataclasses import dataclass
from enum import Enum, IntEnum
from typing import Any, Dict, List, Optional, Tuple


BINARY_SIGNATURE = 0x28211407
IPDS_SIGNATURE = 0x53445049
MAX_DIES = 16
MAX_TABLES = 6


class ValidationMode(str, Enum):
    COMPATIBILITY = "compatibility"
    STRICT = "strict"


class HwId(IntEnum):
    # Numeric IDs cross-referenced against the preserved Linux
    # soc15_hw_ip.h / amdgpu_discovery.c source snapshot. The source
    # snapshot is not proven byte-identical to the capture kernel build.
    MP1 = 1
    THM = 3
    SMUIO = 4
    FUSE = 5
    CLKA = 6
    PWR = 10
    GC = 11
    UVD = 12
    MMHUB = 34
    ATHUB = 35
    DBGU_NBIO = 36
    DFX = 37
    OSSSYS = 40
    HDP = 41
    SDMA0 = 42
    SDMA1 = 43
    DF = 46
    CLKB = 47
    DFX_DAP = 49
    PCIE = 70
    LSDMA = 91
    NBIF = 108
    SYSTEMHUB = 128
    UMC = 150
    MP0 = 255
    DMU = 271
    DIO = 272
    DAZ = 274


@dataclass(frozen=True)
class IPVersion:
    major: int
    minor: int
    revision: int

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.revision}"


@dataclass
class IPEntry:
    hw_id: int
    instance: int
    version: IPVersion
    num_bases: int
    bases: List[int]


class ParseError(Exception):
    def __init__(self, message: str, offset: Optional[int] = None):
        self.message = message
        self.offset = offset
        suffix = f" (offset {offset})" if offset is not None else ""
        super().__init__(message + suffix)


class IPDiscovery:
    """Parse an in-memory binary without accessing hardware or device state."""

    MAX_BASES = 16
    MAX_IPS = 256

    def __init__(self, mode: str = ValidationMode.STRICT.value):
        self.data = b""
        self.size = 0
        self.valid = False
        self.bin_base = 0
        self._die_base = 0
        self.binary_size = 0
        self.num_ips = 0
        self.mode = self._normalize_mode(mode)
        self._ip_entries: Optional[List[IPEntry]] = None

    @staticmethod
    def _normalize_mode(mode: str) -> str:
        try:
            return ValidationMode(mode).value
        except ValueError as exc:
            allowed = ", ".join(item.value for item in ValidationMode)
            raise ValueError(f"mode must be one of: {allowed}") from exc

    def init(
        self, buffer: bytes, mode: Optional[str] = None
    ) -> Tuple[bool, Optional[str]]:
        """Initialize from bytes; strict validation is the default."""
        if mode is not None:
            self.mode = self._normalize_mode(mode)

        self.data = bytes(buffer)
        self.size = len(self.data)
        self.valid = False
        self.bin_base = 0
        self._die_base = 0
        self.binary_size = 0
        self.num_ips = 0
        self._ip_entries = None

        if self.size < 512:
            return False, "Buffer too small (< 512 bytes)"

        candidate_error: Optional[ParseError] = None
        for offset in range(0, self.size - 511, 4):
            if self._read_u32(offset) != BINARY_SIGNATURE:
                continue
            try:
                self._validate_binary(offset)
                return True, None
            except ParseError as exc:
                candidate_error = exc

        if candidate_error is not None:
            return False, str(candidate_error)
        return False, "Binary signature not found"

    def is_valid(self) -> bool:
        return self.valid

    def binary_offset(self) -> int:
        return self.bin_base

    def die_base(self) -> int:
        return self._die_base

    def ip_count(self) -> int:
        return self.num_ips

    def ip_at(self, index: int) -> Optional[IPEntry]:
        if not self.valid or index < 0 or index >= self.num_ips:
            return None
        if self.mode == ValidationMode.STRICT.value:
            if self._ip_entries is None:
                return None
            return self._ip_entries[index]
        try:
            return self._compat_ip_at(index)
        except ParseError:
            return None

    def find_ip(self, hw_id: int, instance: int = 0) -> Optional[IPEntry]:
        for index in range(self.num_ips):
            entry = self.ip_at(index)
            if entry is None:
                return None
            if entry.hw_id == hw_id and entry.instance == instance:
                return entry
        return None

    def reg_byte_offset(
        self, hw_id: int, instance: int, segment: int, reg_dword_offset: int
    ) -> Optional[int]:
        entry = self.find_ip(hw_id, instance)
        if entry is None or segment < 0 or segment >= entry.num_bases:
            return None
        if self.mode == ValidationMode.STRICT.value and not (
            0 <= reg_dword_offset <= 0xFFFFFFFF
        ):
            return None
        if self.mode == ValidationMode.COMPATIBILITY.value:
            reg_dword_offset &= 0xFFFFFFFF
        byte_offset = (entry.bases[segment] + reg_dword_offset) * 4
        if self.mode == ValidationMode.COMPATIBILITY.value:
            return byte_offset & 0xFFFFFFFF
        if byte_offset > 0xFFFFFFFF:
            return None
        return byte_offset

    def to_dict(self) -> Dict[str, Any]:
        if not self.valid:
            return {"valid": False}

        ips = []
        for index in range(self.num_ips):
            entry = self.ip_at(index)
            if entry is None:
                continue
            try:
                hw_name = HwId(entry.hw_id).name
            except ValueError:
                hw_name = f"UNKNOWN_{entry.hw_id}"
            ips.append(
                {
                    "index": index,
                    "hw_id": entry.hw_id,
                    "hw_id_name": hw_name,
                    "instance": entry.instance,
                    "version": str(entry.version),
                    "num_bases": entry.num_bases,
                    "bases": entry.bases,
                }
            )
        return {
            "valid": True,
            "mode": self.mode,
            "binary_offset": self.bin_base,
            "die_offset": self._die_base,
            "ip_count": self.num_ips,
            "ips": ips,
        }

    def _read_u8(self, offset: int) -> int:
        if offset < 0 or offset + 1 > self.size:
            raise ParseError("Read beyond buffer", offset)
        return self.data[offset]

    def _read_u16(self, offset: int) -> int:
        if offset < 0 or offset + 2 > self.size:
            raise ParseError("Read beyond buffer", offset)
        return struct.unpack_from("<H", self.data, offset)[0]

    def _read_u32(self, offset: int) -> int:
        if offset < 0 or offset + 4 > self.size:
            raise ParseError("Read beyond buffer", offset)
        return struct.unpack_from("<I", self.data, offset)[0]

    def _validate_binary(self, offset: int) -> None:
        major = self._read_u16(offset + 4)
        checksum = self._read_u16(offset + 8)
        binary_size = self._read_u16(offset + 10)
        if binary_size < 16 or offset + binary_size > self.size:
            raise ParseError("Invalid binary size", offset + 10)

        strict = self.mode == ValidationMode.STRICT.value
        if strict and major not in (0, 1, 2):
            raise ParseError(f"Unsupported binary header version {major}", offset + 4)

        table_list_relative = 16 if major >= 2 else 12
        table_list = offset + table_list_relative
        table_list_end = 0
        if strict:
            if major >= 2:
                if binary_size < 16:
                    raise ParseError("Truncated version-2 binary header", offset + 10)
                table_count = self._read_u16(offset + 12)
                if table_count < 1 or table_count > MAX_TABLES:
                    raise ParseError("Invalid table count", offset + 12)
                table_list_end = 16 + table_count * 8
            else:
                table_list_end = 12 + MAX_TABLES * 8
            if binary_size < table_list_end:
                raise ParseError("Truncated binary table list", offset + 10)
        else:
            calculated = sum(self.data[offset + 10 : offset + binary_size]) & 0xFFFF
            if calculated != checksum:
                raise ParseError("Binary checksum mismatch", offset + 8)

        ipd_offset = self._read_u16(table_list)
        table_checksum = self._read_u16(table_list + 2)
        table_size = self._read_u16(table_list + 4)
        if ipd_offset == 0 or table_size < 80:
            raise ParseError("Invalid IP_DISCOVERY table offset or size", table_list)

        if strict and ipd_offset < table_list_end:
            raise ParseError(
                "IP_DISCOVERY table overlaps binary header or table list",
                table_list,
            )

        ipd = offset + ipd_offset
        if strict and (ipd_offset + table_size > binary_size):
            raise ParseError("IP_DISCOVERY table extends beyond binary", ipd)
        if strict:
            calculated = sum(self.data[offset + 10 : offset + binary_size]) & 0xFFFF
            if calculated != checksum:
                raise ParseError("Binary checksum mismatch", offset + 8)

        signature = self._read_u32(ipd)
        num_dies = self._read_u16(ipd + 12)
        die_offset = self._read_u16(ipd + 16)
        if signature != IPDS_SIGNATURE:
            raise ParseError("Invalid IP Discovery signature", ipd)
        if num_dies == 0 or (strict and num_dies != 1):
            raise ParseError("Unsupported IP Discovery die count", ipd + 12)
        if die_offset == 0:
            raise ParseError("Invalid first die offset", ipd + 16)

        die = offset + die_offset

        entries: Optional[List[IPEntry]] = None
        if strict:
            ipd_version = self._read_u16(ipd + 4)
            ipd_size = self._read_u16(ipd + 6)
            if ipd_version not in (1, 2, 3, 4):
                raise ParseError(f"Unsupported IP Discovery version {ipd_version}", ipd + 4)
            if ipd_size != table_size:
                raise ParseError("IP Discovery header/table sizes disagree", ipd + 6)
            if ipd_size < 80 or ipd + ipd_size > offset + binary_size:
                raise ParseError("IP Discovery table extends beyond binary", ipd + 6)
            if ipd_version == 4 and (self._read_u8(ipd + 78) & 1):
                raise ParseError("64-bit IP base addresses are unsupported", ipd + 78)

            table_end = ipd + table_size
            die_relative_end = ipd_offset + ipd_size
            if die_offset < ipd_offset + 80:
                raise ParseError(
                    "Die header overlaps fixed IP_DISCOVERY header", die
                )
            if die_offset + 4 > die_relative_end:
                raise ParseError("Die header lies outside IP_DISCOVERY table", die)
            calculated_table_checksum = (
                sum(self.data[ipd : ipd + ipd_size]) & 0xFFFF
            )
            if calculated_table_checksum != table_checksum:
                raise ParseError("IP_DISCOVERY table checksum mismatch", table_list + 2)

        num_ips = self._read_u16(die + 2)
        if num_ips == 0 or num_ips > self.MAX_IPS:
            raise ParseError("Invalid IP count", die + 2)

        if strict:
            entries = self._parse_strict_entries(
                die, num_ips, min(table_end, offset + binary_size, self.size)
            )

        self.bin_base = offset
        self._die_base = die
        self.binary_size = binary_size
        self.num_ips = num_ips
        self._ip_entries = entries
        self.valid = True

    def _parse_strict_entries(
        self, die: int, count: int, table_end: int
    ) -> List[IPEntry]:
        entries = []
        position = die + 4
        for index in range(count):
            if position + 8 > table_end:
                raise ParseError(f"Truncated IP record {index}", position)
            hw_id = self._read_u16(position)
            instance = self._read_u8(position + 2)
            num_bases = self._read_u8(position + 3)
            if num_bases > self.MAX_BASES:
                raise ParseError(
                    f"IP record {index} has too many base addresses", position + 3
                )
            record_end = position + 8 + 4 * num_bases
            if record_end > table_end:
                raise ParseError(f"Truncated base array in IP record {index}", position)
            version = IPVersion(
                self._read_u8(position + 4),
                self._read_u8(position + 5),
                self._read_u8(position + 6),
            )
            bases = [
                self._read_u32(position + 8 + 4 * base_index)
                for base_index in range(num_bases)
            ]
            entries.append(IPEntry(hw_id, instance, version, num_bases, bases))
            position = record_end
        return entries

    def _compat_ip_at(self, index: int) -> IPEntry:
        position = self._die_base + 4
        for current in range(index + 1):
            hw_id = self._read_u16(position)
            instance = self._read_u8(position + 2)
            num_bases = self._read_u8(position + 3)
            version = IPVersion(
                self._read_u8(position + 4),
                self._read_u8(position + 5),
                self._read_u8(position + 6),
            )
            exposed_bases = min(num_bases, self.MAX_BASES)
            if current == index:
                bases = [
                    self._read_u32(position + 8 + 4 * base_index)
                    for base_index in range(exposed_bases)
                ]
                return IPEntry(
                    hw_id, instance, version, exposed_bases, bases
                )
            position += 8 + 4 * num_bases
        raise ParseError("IP index not found")
