#include "ipdiscovery.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace {

constexpr std::size_t kInputSize = 512;
constexpr std::size_t kIpDiscoveryOffset = 64;
constexpr std::size_t kDieOffset = kIpDiscoveryOffset + 80;
constexpr std::size_t kRecordOffset = kDieOffset + 4;

void writeU16(std::vector<std::uint8_t>& data, std::size_t offset,
              std::uint16_t value) {
    data[offset] = static_cast<std::uint8_t>(value);
    data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void writeU32(std::vector<std::uint8_t>& data, std::size_t offset,
              std::uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte) {
        data[offset + byte] =
            static_cast<std::uint8_t>(value >> (byte * 8));
    }
}

void refreshChecksum(std::vector<std::uint8_t>& data) {
    std::uint16_t checksum = 0;
    for (std::size_t index = 10; index < data.size(); ++index) {
        checksum = static_cast<std::uint16_t>(checksum + data[index]);
    }
    writeU16(data, 8, checksum);
}

std::vector<std::uint8_t> makeFixture(std::uint16_t tableSize,
                                      std::uint16_t ipCount,
                                      std::uint8_t baseCount,
                                      std::uint16_t binaryVersion = 1) {
    std::vector<std::uint8_t> data(kInputSize, 0);
    writeU32(data, 0, 0x28211407);
    writeU16(data, 4, binaryVersion);
    writeU16(data, 6, 3);
    writeU16(data, 10, static_cast<std::uint16_t>(kInputSize));
    const std::size_t tableListOffset = binaryVersion >= 2 ? 16 : 12;
    if (binaryVersion >= 2) {
        writeU16(data, 12, 1);
    }
    writeU16(data, tableListOffset,
             static_cast<std::uint16_t>(kIpDiscoveryOffset));
    writeU16(data, tableListOffset + 4, tableSize);

    writeU32(data, kIpDiscoveryOffset, 0x53445049);
    writeU16(data, kIpDiscoveryOffset + 4, 3);
    writeU16(data, kIpDiscoveryOffset + 6, tableSize);
    writeU16(data, kIpDiscoveryOffset + 12, 1);
    writeU16(data, kIpDiscoveryOffset + 16,
             static_cast<std::uint16_t>(kDieOffset));

    writeU16(data, kDieOffset + 2, ipCount);
    writeU16(data, kRecordOffset, 11);
    data[kRecordOffset + 2] = 0;
    data[kRecordOffset + 3] = baseCount;
    data[kRecordOffset + 4] = 12;
    data[kRecordOffset + 5] = 0;
    data[kRecordOffset + 6] = 1;
    for (std::uint8_t index = 0; index < baseCount; ++index) {
        writeU32(data, kRecordOffset + 8 + 4u * index, 0x40000u + index);
    }

    refreshChecksum(data);
    return data;
}

bool check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
    return condition;
}

}  // namespace

int main() {
    int failures = 0;
    {
        for (const std::uint16_t binaryVersion :
             {std::uint16_t{1}, std::uint16_t{2}}) {
            auto bytes = makeFixture(96, 1, 1, binaryVersion);
            IpDiscovery parser;
            IpDiscovery::IpEntry entry{};
            failures += !check(parser.init(bytes.data(), bytes.size()),
                               "valid v1/v2 declared-table layout initializes");
            failures += !check(parser.ipAt(0, entry) && entry.hwId == 11 &&
                                   entry.numBases == 1 &&
                                   entry.bases[0] == 0x40000,
                               "valid v1/v2 record inside declared table is readable");
        }
    }

    {
        auto bytes = makeFixture(96, 1, 1);
        IpDiscovery parser;
        std::uint32_t byteOffset = 0;
        failures += !check(parser.init(bytes.data(), bytes.size()) &&
                               parser.regByteOffset(11, 0, 0, 0x123,
                                                   byteOffset) &&
                               byteOffset == (0x40000u + 0x123u) * 4u,
                           "register dword offsets convert without changing valid addresses");

        byteOffset = 0xA5A5A5A5;
        failures += !check(!parser.regByteOffset(11, 0, 0, UINT32_MAX,
                                                 byteOffset) &&
                               byteOffset == 0xA5A5A5A5,
                           "overflowing base addition is rejected without wrapping output");
    }

    {
        auto bytes = makeFixture(96, 1, 1);
        writeU32(bytes, kRecordOffset + 8, 0x3FFFFFFF);
        refreshChecksum(bytes);
        IpDiscovery parser;
        std::uint32_t byteOffset = 0;
        failures += !check(parser.init(bytes.data(), bytes.size()) &&
                               parser.regByteOffset(11, 0, 0, 0, byteOffset) &&
                               byteOffset == UINT32_MAX - 3,
                           "largest representable aligned register byte offset is accepted");

        writeU32(bytes, kRecordOffset + 8, 0x40000000);
        refreshChecksum(bytes);
        IpDiscovery overflowParser;
        byteOffset = 0x5A5A5A5A;
        failures += !check(overflowParser.init(bytes.data(), bytes.size()) &&
                               !overflowParser.regByteOffset(11, 0, 0, 0,
                                                             byteOffset) &&
                               byteOffset == 0x5A5A5A5A,
                           "byte-offset multiplication overflow is rejected without changing output");
    }

    {
        auto bytes = makeFixture(92, 1, 1);
        IpDiscovery parser;
        IpDiscovery::IpEntry entry{};
        failures += !check(parser.init(bytes.data(), bytes.size()),
                           "truncated record table remains lazily parsed");
        failures += !check(!parser.ipAt(0, entry),
                           "base array beyond declared table is rejected");
    }

    {
        auto bytes = makeFixture(96, 2, 1);
        IpDiscovery parser;
        IpDiscovery::IpEntry entry{};
        failures += !check(parser.init(bytes.data(), bytes.size()),
                           "record-count fixture initializes");
        failures += !check(!parser.ipAt(1, entry),
                           "record header beyond declared table is rejected");
    }

    {
        auto bytes = makeFixture(96, 1, 1);
        writeU16(bytes, 12, 56);
        std::uint16_t checksum = 0;
        for (std::size_t index = 10; index < bytes.size(); ++index) {
            checksum = static_cast<std::uint16_t>(checksum + bytes[index]);
        }
        writeU16(bytes, 8, checksum);
        IpDiscovery parser;
        failures += !check(!parser.init(bytes.data(), bytes.size()),
                           "v1 table overlapping the six-entry list is rejected");
    }

    {
        auto bytes = makeFixture(96, 1, 1);
        writeU16(bytes, 12, 480);
        std::uint16_t checksum = 0;
        for (std::size_t index = 10; index < bytes.size(); ++index) {
            checksum = static_cast<std::uint16_t>(checksum + bytes[index]);
        }
        writeU16(bytes, 8, checksum);
        IpDiscovery parser;
        failures += !check(!parser.init(bytes.data(), bytes.size()),
                           "table extending beyond declared binary is rejected");
    }

    if (failures != 0) {
        std::fprintf(stderr, "IP Discovery bounds tests: %d failed\n", failures);
        return 1;
    }
    std::puts("IP Discovery bounds checks: passed (7 fixture cases)");
    return 0;
}
