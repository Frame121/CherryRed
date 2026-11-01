#include "Discovery.hpp"
#include "DiscoveryFixture.hpp"
#include <random>
#include <vector>
using namespace cherryred::navi33;

int main() {
    auto bytes = discoveryFixture();
    Discovery discovery;
    DiscoveryRecord record;
    CHECK(discovery.initialize(bytes.data(), bytes.size()) == DiscoveryError::None);
    CHECK(discovery.count() == 2 && discovery.tableVersion() == 3 && discovery.dieId() == 0);
    CHECK(discovery.find(11, 0, record) && record.major == 99 && record.revision == 7);
    CHECK(record.attributes == 0x42 && record.baseCount == 1 && record.bases[0] == 0x100);
    CHECK(discovery.find(42, 1, record) && record.major == 98 && record.bases[0] == 0x200);
    CHECK(!discovery.find(42, 0, record) && record.baseCount == 0);
    CHECK(!discovery.at(2, record) && record.hwId == 0);
    uint32_t offset = 123;
    CHECK(discovery.registerByteOffset(11, 0, 0, 3, 0x410, offset) && offset == 0x40c);
    CHECK(!discovery.registerByteOffset(11, 0, 0, 3, 0x40f, offset) && offset == 0);
    CHECK(!discovery.registerByteOffset(11, 0, 1, 0, 0x1000, offset));
    CHECK(!discovery.registerByteOffset(11, 0, 0, UINT32_MAX, UINT64_MAX, offset));

    // Reinitializing a successful parser with invalid input clears all state.
    for (size_t size = 0; size < 512; ++size) {
        CHECK(discovery.initialize(bytes.data(), size) == DiscoveryError::Truncated);
        CHECK(discovery.count() == 0 && discovery.tableVersion() == 0);
        CHECK(!discovery.at(0, record));
    }
    CHECK(discovery.initialize(nullptr, 512) == DiscoveryError::NullInput);
    for (const auto badOffset : {0u, 59u, 500u, 65535u}) {
        auto bad = bytes;
        put16(bad.data() + 12, static_cast<uint16_t>(badOffset));
        refreshDiscovery(bad);
        CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::TableBounds);
    }
    auto bad = bytes;
    bad[8] ^= 1;
    CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::BinaryChecksum);
    bad = bytes;
    bad[14] ^= 1;
    uint32_t sum = 0;
    for (size_t i = 10; i < bad.size(); ++i) sum += bad[i];
    put16(bad.data() + 8, static_cast<uint16_t>(sum));
    CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::TableChecksum);
    bad = bytes;
    put16(bad.data() + 144, 1);
    refreshDiscovery(bad);
    CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::DieMismatch);
    bad = bytes;
    put16(bad.data() + 160, 11); bad[162] = 0;
    refreshDiscovery(bad);
    CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::DuplicateIp);
    for (const auto bases : {2u, 17u, 255u}) {
        bad = bytes; bad[163] = static_cast<uint8_t>(bases);
        refreshDiscovery(bad);
        CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::RecordBounds);
    }
    bad = bytes;
    put16(bad.data() + 68, 4); bad[142] = 1;
    refreshDiscovery(bad);
    CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::UnsupportedAddressWidth);

    // Unaligned storage must decode identically.
    std::vector<uint8_t> unaligned(513);
    for (size_t i = 0; i < bytes.size(); ++i) unaligned[i + 1] = bytes[i];
    CHECK(discovery.initialize(unaligned.data() + 1, 512) == DiscoveryError::None);
    CHECK(discovery.find(11, 0, record) && record.attributes == 0x42);

    for (uint16_t version = 1; version <= 4; ++version) {
        bad = bytes;
        put16(bad.data() + 68, version);
        put16(bad.data() + 78, 7); // first die ID, not v4 flags at offset 142
        put16(bad.data() + 144, 7);
        refreshDiscovery(bad);
        CHECK(discovery.initialize(bad.data(), bad.size()) == DiscoveryError::None);
        CHECK(discovery.tableVersion() == version && discovery.dieId() == 7);
    }

    // Exercise both fixed capacities at their accepted boundary: 256 unique
    // records, each with 16 bases, including the last offset-table element.
    std::vector<uint8_t> maximum(64 + 84 + 256 * 72);
    for (size_t i = 0; i < 148; ++i) maximum[i] = bytes[i];
    put16(maximum.data() + 10, static_cast<uint16_t>(maximum.size()));
    put16(maximum.data() + 16, static_cast<uint16_t>(maximum.size() - 64));
    put16(maximum.data() + 70, static_cast<uint16_t>(maximum.size() - 64));
    put16(maximum.data() + 146, 256);
    for (size_t i = 0; i < 256; ++i) {
        auto* entry = maximum.data() + 148 + i * 72;
        put16(entry, static_cast<uint16_t>(1000 + i));
        entry[3] = 16;
        entry[4] = 99;
        for (uint32_t j = 0; j < 16; ++j) put32(entry + 8 + j * 4, j + 0x100);
    }
    sum = 0;
    for (size_t i = 64; i < maximum.size(); ++i) sum += maximum[i];
    put16(maximum.data() + 14, static_cast<uint16_t>(sum));
    sum = 0;
    for (size_t i = 10; i < maximum.size(); ++i) sum += maximum[i];
    put16(maximum.data() + 8, static_cast<uint16_t>(sum));
    CHECK(discovery.initialize(maximum.data(), maximum.size()) == DiscoveryError::None);
    CHECK(discovery.count() == 256 && discovery.at(255, record));
    CHECK(record.hwId == 1255 && record.baseCount == 16 && record.bases[15] == 0x10f);
    CHECK(!discovery.at(256, record));

    // Mutate table bytes, then repair both checksums so fuzzing exercises
    // structural validation rather than stopping at the outer checksum.
    std::mt19937 random(0x1d33);
    for (unsigned trial = 0; trial < 3000; ++trial) {
        bad = bytes;
        for (unsigned mutation = 0; mutation < 4; ++mutation)
            bad[64 + random() % 108] = static_cast<uint8_t>(random());
        refreshDiscovery(bad);
        const auto result = discovery.initialize(bad.data(), bad.size());
        if (result == DiscoveryError::None) {
            CHECK(discovery.count() > 0 && discovery.count() <= Discovery::kMaxRecords);
            for (size_t i = 0; i < discovery.count(); ++i) {
                CHECK(discovery.at(i, record) && record.baseCount <= 16);
                DiscoveryRecord found;
                CHECK(discovery.find(record.hwId, record.instance, found));
                CHECK(found.major == record.major && found.revision == record.revision);
            }
        } else {
            CHECK(discovery.count() == 0 && !discovery.at(0, record));
        }
    }
}
