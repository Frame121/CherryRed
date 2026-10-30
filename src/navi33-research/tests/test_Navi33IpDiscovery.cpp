#include "Navi33IpDiscovery.hpp"
#include "Navi33TargetProfile.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace cobalt64::research;

namespace {
int failures = 0;

void check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}

std::uint16_t readU16(const std::vector<std::uint8_t>& data,
                      std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[offset]) |
        (static_cast<std::uint16_t>(data[offset + 1U]) << 8U));
}

void writeU16(std::vector<std::uint8_t>& data, std::size_t offset,
              std::uint16_t value) {
    data[offset] = static_cast<std::uint8_t>(value);
    data[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void refreshBinaryChecksum(std::vector<std::uint8_t>& data) {
    const std::size_t binarySize = readU16(data, 10U);
    std::uint16_t sum = 0;
    for (std::size_t i = 10U; i < binarySize; ++i) {
        sum = static_cast<std::uint16_t>(sum + data[i]);
    }
    writeU16(data, 8U, sum);
}

void refreshIpDiscoveryChecksum(std::vector<std::uint8_t>& data) {
    const std::size_t tableOffset = readU16(data, 12U);
    const std::size_t tableSize = readU16(data, 16U);
    std::uint16_t sum = 0;
    for (std::size_t i = 0; i < tableSize; ++i) {
        sum = static_cast<std::uint16_t>(sum + data[tableOffset + i]);
    }
    writeU16(data, 14U, sum);
    refreshBinaryChecksum(data);
}

const Navi33DiscoveredIp* findRecord(const Navi33DiscoveredIp* records,
                                     std::size_t count, std::uint16_t hwId,
                                     std::uint8_t instance = 0) {
    for (std::size_t i = 0; i < count; ++i) {
        if (records[i].hwId == hwId && records[i].instance == instance) {
            return &records[i];
        }
    }
    return nullptr;
}
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: navi33_ipdiscovery_tests <capture-path>\n");
        return 2;
    }

    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::fprintf(stderr, "cannot open RX 7600 IP Discovery capture: %s\n", argv[1]);
        return 2;
    }
    std::vector<std::uint8_t> capture(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    check(capture.size() == 10240U, "captured evidence file has expected 10 KiB length");

    Navi33DiscoveredIp records[256]{};
    const auto parsed = parseNavi33IpDiscovery(
        capture.data(), capture.size(), records, 256U);
    check(parsed.valid, "captured RX 7600 IP Discovery binary parses");
    if (parsed.valid) {
        check(parsed.binaryOffset == 0U && parsed.binaryMajor == 1U &&
              parsed.binaryMinor == 3U && parsed.declaredBinarySize == 1280U,
              "binary v1.3 header and declared 1280-byte extent match evidence");
        check(parsed.ipDiscoveryVersion == 3U && parsed.dieCount == 1U &&
              parsed.recordCount == 39U,
              "IPDS v3 single-die record count matches evidence");

        const std::uint16_t knownIds[] = {
            1, 3, 4, 5, 6, 10, 11, 12, 34, 35, 36, 37, 40, 41, 42,
            43, 46, 47, 49, 70, 91, 108, 128, 150, 255, 271, 272, 274,
            282, 286
        };
        std::size_t uniqueCount = 0;
        for (std::size_t i = 0; i < parsed.recordCount; ++i) {
            bool first = true;
            for (std::size_t j = 0; j < i; ++j) {
                if (records[j].hwId == records[i].hwId) {
                    first = false;
                    break;
                }
            }
            if (first) ++uniqueCount;
        }
        check(uniqueCount == 30U,
              "all 30 distinct numeric HWIDs are preserved from capture");
        for (const auto hwId : knownIds) {
            check(findRecord(records, parsed.recordCount, hwId) != nullptr,
                  "each documented unique numeric HWID is present");
        }

        const auto* gc = findRecord(records, parsed.recordCount, 11);
        const auto* sdma0 = findRecord(records, parsed.recordCount, 42);
        const auto* sdma1 = findRecord(records, parsed.recordCount, 43);
        const auto* uvd = findRecord(records, parsed.recordCount, 12);
        const auto* mp0 = findRecord(records, parsed.recordCount, 255);
        const auto* mp1 = findRecord(records, parsed.recordCount, 1);
        const auto* dmu = findRecord(records, parsed.recordCount, 271);
        const auto* unknown282 = findRecord(records, parsed.recordCount, 282);
        const auto* unknown286 = findRecord(records, parsed.recordCount, 286);
        check(gc && gc->major == 11 && gc->minor == 0 && gc->revision == 2,
              "GC HWID 11 version 11.0.2 matches capture");
        check(sdma0 && sdma0->major == 6 && sdma0->minor == 0 && sdma0->revision == 2,
              "SDMA0 HWID 42 version 6.0.2 matches capture");
        check(sdma1 && sdma1->major == 6 && sdma1->minor == 0 && sdma1->revision == 2,
              "SDMA1 HWID 43 version 6.0.2 matches capture");
        check(uvd && uvd->major == 4 && uvd->minor == 0 && uvd->revision == 4,
              "UVD HWID 12 version 4.0.4 matches capture");
        check(mp0 && mp0->major == 13 && mp0->minor == 0 && mp0->revision == 7,
              "MP0 HWID 255 version 13.0.7 matches capture");
        check(mp1 && mp1->major == 13 && mp1->minor == 0 && mp1->revision == 7,
              "MP1 HWID 1 version 13.0.7 matches capture");
        check(dmu && dmu->major == 3 && dmu->minor == 2 && dmu->revision == 1,
              "DMU HWID 271 version 3.2.1 matches capture");
        check(unknown282 && unknown282->major == 13 &&
              unknown282->minor == 0 && unknown282->revision == 2,
              "unresolved HWID 282 retained numerically without guessing its name");
        check(unknown286 && unknown286->major == 4 &&
              unknown286->minor == 0 && unknown286->revision == 2,
              "unresolved HWID 286 retained numerically without guessing its name");

        Navi33IpObservation observations[256]{};
        for (std::size_t i = 0; i < parsed.recordCount; ++i) {
            observations[i] = {records[i].hwId, records[i].instance,
                               records[i].major, records[i].minor,
                               records[i].revision};
        }
        const auto profile = validateNavi33TargetProfile(
            observations, parsed.recordCount);
        check(profile.valid,
              "parsed capture connects directly to numeric Navi33 target-profile validator");
        check(profile.valid &&
              std::string(profile.message).find("initialization remains disabled") != std::string::npos,
              "live parsed profile remains a read-only gate, not hardware authorization");
    }

    {
        const auto result = parseNavi33IpDiscovery(nullptr, 0U, records, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::NullInput,
              "null input rejected");
    }
    {
        const auto result = parseNavi33IpDiscovery(
            capture.data(), capture.size(), nullptr, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::NullOutput,
              "null output rejected");
    }
    {
        const auto result = parseNavi33IpDiscovery(
            capture.data(), 8U, records, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::InputTooSmall,
              "short input rejected");
    }
    {
        auto bad = capture;
        bad[0] ^= 0x01U;
        const auto result = parseNavi33IpDiscovery(
            bad.data(), bad.size(), records, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::BinaryNotFound,
              "bad signature rejected");
    }
    {
        const auto result = parseNavi33IpDiscovery(
            capture.data(), 100U, records, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::InvalidBinarySize,
              "truncated declared binary rejected");
    }
    {
        const auto result = parseNavi33IpDiscovery(
            capture.data(), capture.size(), records, 1U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::OutputCapacityTooSmall,
              "output capacity must hold all records");
    }
    {
        auto bad = capture;
        bad[100U] ^= 0x01U;
        const auto result = parseNavi33IpDiscovery(
            bad.data(), bad.size(), records, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::BinaryChecksumMismatch,
              "binary checksum mismatch rejected");
    }
    {
        auto bad = capture;
        const std::size_t ipOffset = readU16(bad, 12U);
        bad[ipOffset + 100U] ^= 0x01U;
        refreshBinaryChecksum(bad);
        const auto result = parseNavi33IpDiscovery(
            bad.data(), bad.size(), records, 256U);
        check(!result.valid &&
              result.error == Navi33IpDiscoveryError::IpDiscoveryChecksumMismatch,
              "IP Discovery checksum mismatch rejected independently");
    }
    {
        auto bad = capture;
        const std::size_t ipOffset = readU16(bad, 12U);
        writeU16(bad, ipOffset + 4U, 4U);
        refreshIpDiscoveryChecksum(bad);
        const auto result = parseNavi33IpDiscovery(
            bad.data(), bad.size(), records, 256U);
        check(!result.valid &&
              result.error == Navi33IpDiscoveryError::UnsupportedIpDiscoveryVersion,
              "unsupported IP Discovery version fails closed");
    }

    {
        std::vector<std::uint8_t> prefixed(16U + capture.size(), 0xA5U);
        std::copy(capture.begin(), capture.end(), prefixed.begin() + 16);
        const auto result = parseNavi33IpDiscovery(
            prefixed.data(), prefixed.size(), records, 256U);
        check(result.valid && result.binaryOffset == 16U &&
              result.recordCount == 39U,
              "aligned non-zero binary offset is discovered and parsed");
    }
    {
        auto bad = capture;
        const std::size_t ipOffset = readU16(bad, 12U);
        writeU16(bad, ipOffset + 16U, static_cast<std::uint16_t>(ipOffset));
        refreshIpDiscoveryChecksum(bad);
        const auto result = parseNavi33IpDiscovery(
            bad.data(), bad.size(), records, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::InvalidDieLayout,
              "die header pointing inside the IP Discovery header is rejected");
    }
    {
        auto bad = capture;
        const std::size_t ipOffset = readU16(bad, 12U);
        const std::size_t dieOffset = readU16(bad, ipOffset + 16U);
        const std::size_t firstRecordOffset = dieOffset + 4U;
        bad[firstRecordOffset + 3U] = 17U;
        refreshIpDiscoveryChecksum(bad);
        const auto result = parseNavi33IpDiscovery(
            bad.data(), bad.size(), records, 256U);
        check(!result.valid &&
              result.error == Navi33IpDiscoveryError::TooManyBaseAddresses,
              "record with more than 16 base addresses is rejected");
    }
    {
        auto bad = capture;
        const std::size_t ipOffset = readU16(bad, 12U);
        const std::size_t dieOffset = readU16(bad, ipOffset + 16U);
        writeU16(bad, dieOffset + 2U, 257U);
        refreshIpDiscoveryChecksum(bad);
        const auto result = parseNavi33IpDiscovery(
            bad.data(), bad.size(), records, 256U);
        check(!result.valid && result.error == Navi33IpDiscoveryError::InvalidDieLayout,
              "die record count above parser bound is rejected");
    }
    {
        bool allTruncationsRejected = true;
        for (std::size_t length = 0U; length < 1280U; ++length) {
            const auto result = parseNavi33IpDiscovery(
                capture.data(), length, records, 256U);
            if (result.valid) {
                allTruncationsRejected = false;
                break;
            }
        }
        check(allTruncationsRejected,
              "all strict truncations of the declared binary fail closed");
    }

    if (failures != 0) {
        std::fprintf(stderr, "Navi33 IP Discovery tests: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("Navi33 IP Discovery tests: PASS (captured RX7600 binary + malformed-input cases)");
    return 0;
}
