// Synthetic host-side bounds regressions for IpDiscovery.
// Build manually with: c++ -std=c++17 -Wall -Wextra -Werror -Isrc src/ipdiscovery.cpp tests/ipdiscovery_bounds_test.cpp -o ipdiscovery_bounds_test
// No hardware, ROM, MMIO, or firmware access is performed.
#include "ipdiscovery.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
constexpr size_t kBufferSize = 512;
constexpr size_t kBinarySize = 256;
constexpr uint32_t kBinarySignature = 0x28211407u;
constexpr uint32_t kIpdsSignature = 0x53445049u;

void put16(std::vector<uint8_t>& b, size_t p, uint16_t v) {
    b.at(p) = static_cast<uint8_t>(v);
    b.at(p + 1) = static_cast<uint8_t>(v >> 8);
}
void put32(std::vector<uint8_t>& b, size_t p, uint32_t v) {
    put16(b, p, static_cast<uint16_t>(v));
    put16(b, p + 2, static_cast<uint16_t>(v >> 16));
}
void checksum(std::vector<uint8_t>& b, size_t base, size_t length) {
    uint16_t sum = 0;
    for (size_t i = base + 10; i < base + length; ++i)
        sum = static_cast<uint16_t>(sum + b[i]);
    put16(b, base + 8, sum);
}

std::vector<uint8_t> fixture(bool v2 = false) {
    std::vector<uint8_t> b(kBufferSize, 0);
    const size_t base = 0;
    const size_t listRel = v2 ? 16u : 12u;
    const size_t tableRel = v2 ? 32u : 64u;
    const size_t tableSize = v2 ? 128u : 128u;
    const size_t dieRel = tableRel + 80u;

    put32(b, base, kBinarySignature);
    put16(b, base + 4, v2 ? 2u : 1u);
    put16(b, base + 6, 0u);
    put16(b, base + 10, static_cast<uint16_t>(kBinarySize));
    if (v2) {
        put16(b, base + 12, 1u); // one table_info entry
        put16(b, base + 14, 0u);
    }
    put16(b, base + listRel, static_cast<uint16_t>(tableRel));
    put16(b, base + listRel + 2, 0u); // table checksum intentionally not tested
    put16(b, base + listRel + 4, static_cast<uint16_t>(tableSize));
    put16(b, base + listRel + 6, 0u);

    put32(b, tableRel, kIpdsSignature);
    put16(b, tableRel + 4, 3u); // IPDS version
    put16(b, tableRel + 6, static_cast<uint16_t>(tableSize));
    put32(b, tableRel + 8, 0u);
    put16(b, tableRel + 12, 1u); // one die
    put16(b, tableRel + 16, static_cast<uint16_t>(dieRel));

    put16(b, dieRel, 0u); // die id
    put16(b, dieRel + 2, 1u); // one IP
    put16(b, dieRel + 4, 11u); // GC
    b[dieRel + 6] = 0u; // instance
    b[dieRel + 7] = 1u; // one base
    b[dieRel + 8] = 12u; // major
    b[dieRel + 9] = 0u;
    b[dieRel + 10] = 1u;
    b[dieRel + 11] = 0u;
    put32(b, dieRel + 12, 0x40000u);

    checksum(b, base, kBinarySize);
    return b;
}

bool rejected(std::vector<uint8_t> b) {
    IpDiscovery parser;
    return !parser.init(b.data(), b.size()) && !parser.isValid();
}

void testValidV1AndV2() {
    for (bool v2 : {false, true}) {
        auto b = fixture(v2);
        IpDiscovery parser;
        assert(parser.init(b.data(), b.size()));
        assert(parser.isValid());
        assert(parser.ipCount() == 1);
        IpDiscovery::IpEntry entry{};
        assert(parser.ipAt(0, entry));
        assert(entry.hwId == IpDiscovery::HwGc);
        assert(entry.numBases == 1);
        assert(entry.bases[0] == 0x40000u);
    }
}

void testTablePastDeclaredBinary() {
    auto b = fixture();
    put16(b, 12, 200u); // table starts inside caller buffer but only 56 bytes remain in binary
    checksum(b, 0, kBinarySize);
    assert(rejected(b));
}

void testDieHeaderOutsideDeclaredTable() {
    auto b = fixture();
    put16(b, 64 + 16, 200u); // inside binary, beyond IPDS table end (192)
    checksum(b, 0, kBinarySize);
    assert(rejected(b));
}

void testTruncatedV1TableList() {
    auto b = fixture();
    put16(b, 10, 59u); // v1 requires 12 + 6*8 = 60 bytes before any table entry
    checksum(b, 0, 59u);
    assert(rejected(b));
}

void testTruncatedV2CountedTableList() {
    auto b = fixture(true);
    put16(b, 10, 23u); // count=1 needs bytes [16,24), but declared binary ends at 23
    checksum(b, 0, 23u);
    assert(rejected(b));
}
void testNullAndShortInputs() {
    IpDiscovery parser;
    std::array<uint8_t, kBufferSize> bytes{};
    assert(!parser.init(nullptr, bytes.size()));
    assert(!parser.isValid());
    assert(!parser.init(bytes.data(), kBufferSize - 1));
    assert(!parser.isValid());
}

void testFailedInitClearsState() {
    auto good = fixture();
    IpDiscovery parser;
    assert(parser.init(good.data(), good.size()));
    assert(parser.ipCount() == 1);
    std::array<uint8_t, kBufferSize> bad{};
    assert(!parser.init(bad.data(), bad.size()));
    assert(!parser.isValid());
    assert(parser.ipCount() == 0);
    IpDiscovery::IpEntry entry{};
    assert(!parser.ipAt(0, entry));
}

void testMalformedRecordBounds() {
    auto b = fixture();
    constexpr size_t die = 64u + 80u;
    b.at(die + 7u) = 15u; // Declares 15 bases; record cannot fit in the IPDS table.
    checksum(b, 0, kBinarySize);
    IpDiscovery parser;
    assert(parser.init(b.data(), b.size())); // Record decoding is deliberately lazy.
    IpDiscovery::IpEntry entry{};
    assert(!parser.ipAt(0, entry));
}

void testRegisterOffsetOverflowPreservesOutput() {
    auto b = fixture();
    constexpr size_t die = 64u + 80u;
    put32(b, die + 12u, UINT32_MAX);
    checksum(b, 0, kBinarySize);
    IpDiscovery parser;
    assert(parser.init(b.data(), b.size()));
    uint32_t output = 0x12345678u;
    assert(!parser.regByteOffset(IpDiscovery::HwGc, 0, 0, 1u, output));
    assert(output == 0x12345678u);
}

void testBadChecksumRejected() {
    auto b = fixture();
    b.at(200u) ^= 0x5Au;
    assert(rejected(b));
}
} // namespace

int main() {
    testValidV1AndV2();
    testTablePastDeclaredBinary();
    testDieHeaderOutsideDeclaredTable();
    testTruncatedV1TableList();
    testTruncatedV2CountedTableList();
    testNullAndShortInputs();
    testFailedInitClearsState();
    testMalformedRecordBounds();
    testRegisterOffsetOverflowPreservesOutput();
    testBadChecksumRejected();
    std::puts("ipdiscovery_bounds_test: PASS (10 groups, synthetic data only)");
    return 0;
}
