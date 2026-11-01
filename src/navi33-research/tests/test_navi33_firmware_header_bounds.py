#!/usr/bin/env python3
"""Bounds-check the RX7600 AMD firmware headers and selected family descriptors.

Host-only, read-only metadata validation. It does not verify firmware signature,
cryptographic authenticity, CRC semantics, execute a firmware image, or prove
macOS compatibility. Every family parser is restricted to the versions and
formats represented by this evidence set.
"""
import json
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "RX7600-Linux-Evidence" / "navi33-firmware-manifest.json"
FIRMWARE_DIR = ROOT / "src" / "navi33-bringup" / "firmware"

EXPECTED = {
    "gc_11_0_2_imu.bin": (1, 0, 48, 11, 0),
    "gc_11_0_2_me.bin": (2, 0, 60, 11, 0),
    "gc_11_0_2_mec.bin": (2, 0, 60, 11, 0),
    "gc_11_0_2_mes.bin": (1, 0, 72, 11, 0),
    "gc_11_0_2_mes1.bin": (1, 0, 72, 11, 0),
    "gc_11_0_2_mes_2.bin": (1, 0, 72, 11, 0),
    "gc_11_0_2_pfp.bin": (2, 0, 60, 11, 0),
    "gc_11_0_2_rlc.bin": (2, 3, 204, 11, 0),
    "sdma_6_0_2.bin": (2, 0, 64, 6, 0),
    "vcn_4_0_4.bin": (1, 0, 32, 4, 0),
    "dcn_3_2_1_dmcub.bin": (1, 0, 40, 3, 0),
    "psp_13_0_7_sos.bin": (2, 0, 36, 13, 0),
    "psp_13_0_7_ta.bin": (2, 0, 36, 13, 0),
    "smu_13_0_7.bin": (2, 1, 44, 13, 0),
}


class HeaderError(ValueError):
    pass


def u32(data, offset):
    if offset < 0 or offset > len(data) - 4:
        raise HeaderError(f"u32 read at {offset} exceeds file")
    return struct.unpack_from("<I", data, offset)[0]


def check_range(data, offset, size, label, minimum=0):
    if offset < minimum or size < 0 or offset > len(data) or size > len(data) - offset:
        raise HeaderError(
            f"{label} range [{offset}, {offset + size}) exceeds file "
            f"or begins before {minimum}"
        )


def parse_common_header(data, expected):
    if len(data) < 32:
        raise HeaderError("file is shorter than common_firmware_header (32 bytes)")
    total_size, header_size, header_major, header_minor, ip_major, ip_minor, \
        ucode_version, ucode_size, ucode_offset, crc32 = struct.unpack_from(
            "<IIHHHHIIII", data, 0
        )
    if total_size != len(data):
        raise HeaderError(
            f"declared total size {total_size} does not match file size {len(data)}"
        )
    if header_size < 32 or header_size > total_size:
        raise HeaderError("header_size_bytes is outside the file")
    exp_major, exp_minor, exp_header_size, exp_ip_major, exp_ip_minor = expected
    if (header_major, header_minor, header_size) != (
        exp_major, exp_minor, exp_header_size
    ):
        raise HeaderError(
            f"header format v{header_major}.{header_minor}/{header_size} bytes "
            f"does not match expected v{exp_major}.{exp_minor}/{exp_header_size}"
        )
    if (ip_major, ip_minor) != (exp_ip_major, exp_ip_minor):
        raise HeaderError(
            f"IP major/minor {ip_major}.{ip_minor} does not match "
            f"{exp_ip_major}.{exp_ip_minor}"
        )
    if ucode_offset < header_size or (ucode_offset & 3):
        raise HeaderError("ucode_array_offset_bytes overlaps header or is unaligned")
    check_range(data, ucode_offset, ucode_size, "common ucode payload", header_size)
    if ucode_size == 0:
        raise HeaderError("common ucode payload is empty")
    return {
        "size": total_size,
        "header_size": header_size,
        "header_version": (header_major, header_minor),
        "ip_version": (ip_major, ip_minor),
        "ucode_version": ucode_version,
        "ucode_size": ucode_size,
        "ucode_offset": ucode_offset,
        "crc32_field": crc32,
    }


def validate_gfx_v2(data, common):
    # gfx_firmware_header_v2_0: ucode size/absolute file offset and data
    # size/absolute file offset. The Linux loader copies each range directly.
    code_size, code_offset = u32(data, 36), u32(data, 40)
    data_size, data_offset = u32(data, 44), u32(data, 48)
    check_range(data, code_offset, code_size, "GFX code", common["header_size"])
    check_range(data, data_offset, data_size, "GFX data", common["header_size"])
    if code_size == 0 or data_size == 0:
        raise HeaderError("GFX v2.0 code/data subrange is empty")
    if code_offset + code_size != data_offset:
        raise HeaderError("GFX v2.0 code and data are not contiguous in this evidence set")
    if data_offset + data_size != len(data):
        raise HeaderError("GFX v2.0 data range does not end at the file boundary")


def validate_imu(data, common):
    iram_size, iram_rel = u32(data, 32), u32(data, 36)
    dram_size, dram_rel = u32(data, 40), u32(data, 44)
    base = common["ucode_offset"]
    if iram_size == 0 or dram_size == 0 or iram_rel != 0:
        raise HeaderError("IMU IRAM/DRAM sizes or IRAM relative offset invalid")
    if dram_rel != iram_size:
        raise HeaderError("IMU DRAM relative offset does not follow IRAM payload")
    check_range(data, base + iram_rel, iram_size, "IMU IRAM", common["header_size"])
    check_range(data, base + dram_rel, dram_size, "IMU DRAM", common["header_size"])
    if base + dram_rel + dram_size != len(data):
        raise HeaderError("IMU DRAM range does not end at the file boundary")


def validate_mes(data, common):
    code_size, code_offset = u32(data, 36), u32(data, 40)
    data_size, data_offset = u32(data, 48), u32(data, 52)
    check_range(data, code_offset, code_size, "MES code", common["header_size"])
    check_range(data, data_offset, data_size, "MES data", common["header_size"])
    if code_size == 0 or data_size == 0 or code_offset + code_size != data_offset:
        raise HeaderError("MES code/data subranges are empty or not contiguous")
    if data_offset + data_size != len(data):
        raise HeaderError("MES data range does not end at the file boundary")


def validate_sdma_v2(data, common):
    ctx_size = u32(data, 36)
    ctx_jt_offset = u32(data, 40)
    ctx_jt_size = u32(data, 44)
    ctl_offset = u32(data, 48)
    ctl_size = u32(data, 52)
    ctl_jt_offset = u32(data, 56)
    ctl_jt_size = u32(data, 60)
    ctx_offset = common["ucode_offset"]
    check_range(data, ctx_offset, ctx_size, "SDMA context code", common["header_size"])
    check_range(data, ctl_offset, ctl_size, "SDMA control code", common["header_size"])
    if ctx_size == 0 or ctl_size == 0 or ctx_offset + ctx_size != ctl_offset:
        raise HeaderError("SDMA context/control code ranges are empty or not contiguous")
    if ctl_offset + ctl_size != len(data):
        raise HeaderError("SDMA control code range does not end at file boundary")
    # Jump-table values are offsets/sizes in the SDMA header. This test only
    # bounds them against the corresponding code region; it does not execute
    # or patch the jump table.
    if ctx_jt_offset > ctx_size or ctx_jt_size > ctx_size - ctx_jt_offset:
        raise HeaderError("SDMA context jump-table metadata exceeds context code")
    if ctl_jt_offset > ctl_size or ctl_jt_size > ctl_size - ctl_jt_offset:
        raise HeaderError("SDMA control jump-table metadata exceeds control code")


def validate_psp_packaging(data, common):
    count = u32(data, 32)
    descriptors_offset = 36
    if count == 0 or count > 64:
        raise HeaderError("PSP package descriptor count is outside 1..64")
    descriptor_bytes = count * 16
    check_range(
        data, descriptors_offset, descriptor_bytes,
        "PSP descriptor array", common["header_size"]
    )
    if descriptors_offset + descriptor_bytes > common["ucode_offset"]:
        raise HeaderError("PSP descriptor array overlaps the payload area")
    seen_types = set()
    for index in range(count):
        fw_type, fw_version, offset, size = struct.unpack_from(
            "<IIII", data, descriptors_offset + index * 16
        )
        if fw_type == 0 or fw_type in seen_types:
            raise HeaderError("PSP package contains unknown-zero or duplicate firmware type")
        seen_types.add(fw_type)
        check_range(
            data, common["ucode_offset"] + offset, size,
            f"PSP package descriptor {index}", common["ucode_offset"]
        )
        if size == 0:
            raise HeaderError(f"PSP package descriptor {index} is empty")


def validate_smu_v21(data, common):
    count = u32(data, 36)
    entries_offset = u32(data, 40)
    if count == 0 or count > 64:
        raise HeaderError("SMU PPTABLE entry count is outside 1..64")
    check_range(data, entries_offset, count * 12, "SMU PPTABLE entries",
                common["header_size"])
    previous_end = entries_offset + count * 12
    for index in range(count):
        table_id, offset, size = struct.unpack_from(
            "<III", data, entries_offset + index * 12
        )
        if table_id == 0 or size == 0:
            raise HeaderError(f"SMU PPTABLE entry {index} has zero id or size")
        check_range(data, offset, size, f"SMU PPTABLE payload {index}",
                    common["header_size"])
        if offset < previous_end:
            raise HeaderError(f"SMU PPTABLE payload {index} overlaps entry array or prior payload")
        previous_end = offset + size


def validate_rlc_v23(data, common):
    # RLC v2.3 extends v2.0 -> v2.1 -> v2.2 -> v2.3. The offsets below
    # are byte offsets from the start of the firmware file in the Linux
    # structure/loader path. Validate every size/offset pair present in v2.3.
    pairs = [
        (72, 76, "RLC register-list format"),
        (80, 84, "RLC register list"),
        (88, 92, "RLC separate register-list format"),
        (96, 100, "RLC separate register list"),
        (116, 120, "RLC control save/restore list"),
        (132, 136, "RLC GPM save/restore list"),
        (148, 152, "RLC SRM save/restore list"),
        (156, 160, "RLC IRAM"),
        (164, 168, "RLC DRAM"),
        (180, 184, "RLC RLP"),
        (196, 200, "RLC RLCV"),
    ]
    nonempty = 0
    for size_offset, data_offset, label in pairs:
        size, offset = u32(data, size_offset), u32(data, data_offset)
        if size == 0:
            if offset > len(data):
                raise HeaderError(f"{label} empty-range offset exceeds file")
            continue
        check_range(data, offset, size, label, common["header_size"])
        nonempty += 1
    if nonempty < 5:
        raise HeaderError("RLC v2.3 expected section descriptors are unexpectedly empty")


def validate_family(name, data, common):
    if name in {"gc_11_0_2_me.bin", "gc_11_0_2_mec.bin", "gc_11_0_2_pfp.bin"}:
        validate_gfx_v2(data, common)
    elif name == "gc_11_0_2_imu.bin":
        validate_imu(data, common)
    elif name in {"gc_11_0_2_mes.bin", "gc_11_0_2_mes1.bin", "gc_11_0_2_mes_2.bin"}:
        validate_mes(data, common)
    elif name == "sdma_6_0_2.bin":
        validate_sdma_v2(data, common)
    elif name == "gc_11_0_2_rlc.bin":
        validate_rlc_v23(data, common)
    elif name in {"psp_13_0_7_sos.bin", "psp_13_0_7_ta.bin"}:
        validate_psp_packaging(data, common)
    elif name == "smu_13_0_7.bin":
        validate_smu_v21(data, common)
    elif name == "dcn_3_2_1_dmcub.bin":
        inst_const_bytes, bss_data_bytes = u32(data, 32), u32(data, 36)
        if inst_const_bytes != common["ucode_size"]:
            raise HeaderError("DMCUB instruction/constant length differs from common payload length")
        if bss_data_bytes > len(data):
            raise HeaderError("DMCUB BSS/data length exceeds file")
    # VCN 4.0.4 is common-header-only in the retained source format.
    return common


class Navi33FirmwareHeaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        cls.required = {
            Path(row["path"]).name
            for row in cls.manifest["firmware"]
            if row.get("required")
        }

    def test_expected_firmware_set_matches_header_matrix(self):
        self.assertEqual(self.required, set(EXPECTED))

    def test_all_14_common_headers_and_family_descriptors_are_bounded(self):
        results = {}
        for name, expected in EXPECTED.items():
            path = FIRMWARE_DIR / name
            self.assertTrue(path.is_file(), f"missing firmware: {name}")
            data = path.read_bytes()
            common = parse_common_header(data, expected)
            results[name] = validate_family(name, data, common)
        self.assertEqual(len(results), 14)
        print("Validated common and family descriptor bounds for 14 Navi33 firmware files")

    def _mutate(self, name, offset, value):
        data = bytearray((FIRMWARE_DIR / name).read_bytes())
        struct.pack_into("<I", data, offset, value)
        common = parse_common_header(bytes(data), EXPECTED[name])
        validate_family(name, bytes(data), common)

    def test_truncated_common_header_rejected(self):
        with self.assertRaises(HeaderError):
            parse_common_header(b"\x00" * 31, EXPECTED["vcn_4_0_4.bin"])

    def test_declared_file_size_mismatch_rejected(self):
        data = bytearray((FIRMWARE_DIR / "vcn_4_0_4.bin").read_bytes())
        struct.pack_into("<I", data, 0, len(data) + 1)
        with self.assertRaises(HeaderError):
            parse_common_header(bytes(data), EXPECTED["vcn_4_0_4.bin"])

    def test_header_size_overrun_rejected(self):
        data = bytearray((FIRMWARE_DIR / "vcn_4_0_4.bin").read_bytes())
        struct.pack_into("<I", data, 4, len(data) + 1)
        with self.assertRaises(HeaderError):
            parse_common_header(bytes(data), EXPECTED["vcn_4_0_4.bin"])

    def test_payload_offset_overrun_rejected(self):
        data = bytearray((FIRMWARE_DIR / "vcn_4_0_4.bin").read_bytes())
        struct.pack_into("<I", data, 24, len(data) - 4)
        struct.pack_into("<I", data, 20, 16)
        with self.assertRaises(HeaderError):
            parse_common_header(bytes(data), EXPECTED["vcn_4_0_4.bin"])

    def test_wrong_ip_family_rejected(self):
        data = bytearray((FIRMWARE_DIR / "vcn_4_0_4.bin").read_bytes())
        struct.pack_into("<H", data, 12, 11)
        with self.assertRaises(HeaderError):
            parse_common_header(bytes(data), EXPECTED["vcn_4_0_4.bin"])

    def test_psp_descriptor_count_overflow_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("psp_13_0_7_sos.bin", 32, 0xFFFFFFFF)

    def test_psp_payload_descriptor_overflow_rejected(self):
        data = bytearray((FIRMWARE_DIR / "psp_13_0_7_sos.bin").read_bytes())
        struct.pack_into("<I", data, 44, 0xFFFFFFFF)  # first descriptor payload offset
        common = parse_common_header(bytes(data), EXPECTED["psp_13_0_7_sos.bin"])
        with self.assertRaises(HeaderError):
            validate_family("psp_13_0_7_sos.bin", bytes(data), common)

    def test_smu_pptable_count_overflow_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("smu_13_0_7.bin", 36, 0xFFFFFFFF)

    def test_smu_pptable_payload_range_overflow_rejected(self):
        data = bytearray((FIRMWARE_DIR / "smu_13_0_7.bin").read_bytes())
        entries = u32(data, 40)
        struct.pack_into("<I", data, entries + 4, 0xFFFFFFFF)
        common = parse_common_header(bytes(data), EXPECTED["smu_13_0_7.bin"])
        with self.assertRaises(HeaderError):
            validate_family("smu_13_0_7.bin", bytes(data), common)

    def test_imu_dram_offset_overflow_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("gc_11_0_2_imu.bin", 44, 0xFFFFFFFF)

    def test_gfx_data_offset_overflow_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("gc_11_0_2_me.bin", 48, 0xFFFFFFFF)

    def test_dmcub_length_mismatch_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("dcn_3_2_1_dmcub.bin", 32, 1)

    def test_mes_data_range_overflow_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("gc_11_0_2_mes.bin", 48, 0xFFFFFFFF)

    def test_rlc_section_range_overflow_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("gc_11_0_2_rlc.bin", 156, 0xFFFFFFFF)

    def test_sdma_control_range_overflow_rejected(self):
        with self.assertRaises(HeaderError):
            self._mutate("sdma_6_0_2.bin", 52, 0xFFFFFFFF)


if __name__ == "__main__":
    unittest.main(verbosity=2)
