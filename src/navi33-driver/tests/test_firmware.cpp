#include "Firmware.hpp"
#include "Fixture.hpp"
#include <algorithm>
#include <random>
#include <vector>
using namespace cherryred::navi33;

int main() {
    auto data = fixture();
    FirmwareInfo info;
    CHECK(inspectFirmware(data.data(), data.size(), info) == FirmwareError::None);
    CHECK(info.fileSize == 128 && info.headerSize == 48);
    CHECK(info.headerMajor == 2 && info.headerMinor == 7 && info.ipMajor == 99 && info.ipMinor == 3);
    CHECK(info.ucodeVersion == 0x12345678 && info.declaredCrc32 == 0xdeadbeef);
    CHECK(info.payloadOffset == 64 && info.payloadSize == 64);

    // No alignment or buffer-lifetime dependency.
    std::vector<uint8_t> unaligned(129);
    std::copy(data.begin(), data.end(), unaligned.begin() + 1);
    CHECK(inspectFirmware(unaligned.data() + 1, 128, info) == FirmwareError::None);
    unaligned.clear();
    CHECK(info.ucodeVersion == 0x12345678);
    for (size_t length = 0; length < data.size(); ++length) {
        info.fileSize = 123;
        CHECK(inspectFirmware(data.data(), length, info) != FirmwareError::None);
        CHECK(info.fileSize == 0 && info.payloadSize == 0);
    }
    CHECK(inspectFirmware(nullptr, 128, info) == FirmwareError::NullInput);
    for (uint32_t header : {0u, 31u, 129u, UINT32_MAX}) {
        auto bad = data;
        put32(bad.data() + 4, header);
        CHECK(inspectFirmware(bad.data(), bad.size(), info) == FirmwareError::InvalidHeader);
    }
    for (uint32_t offset : {0u, 47u, 65u, 129u, UINT32_MAX}) {
        auto bad = data;
        put32(bad.data() + 24, offset);
        CHECK(inspectFirmware(bad.data(), bad.size(), info) == FirmwareError::InvalidPayload);
    }
    for (uint32_t size : {0u, 65u, UINT32_MAX}) {
        auto bad = data;
        put32(bad.data() + 20, size);
        CHECK(inspectFirmware(bad.data(), bad.size(), info) == FirmwareError::InvalidPayload);
    }
    auto extra = std::vector<uint8_t>(data.begin(), data.end());
    extra.push_back(0);
    CHECK(inspectFirmware(extra.data(), extra.size(), info) == FirmwareError::SizeMismatch);

    // Deterministic malformed inputs with a truthful file-size field. If
    // accepted, every exported span must remain inside the supplied bytes.
    std::mt19937 random(0x33);
    for (unsigned trial = 0; trial < 2000; ++trial) {
        std::vector<uint8_t> bytes(32 + random() % 512);
        for (auto& byte : bytes) byte = static_cast<uint8_t>(random());
        put32(bytes.data(), static_cast<uint32_t>(bytes.size()));
        const auto result = inspectFirmware(bytes.data(), bytes.size(), info);
        if (result == FirmwareError::None) {
            CHECK(info.headerSize >= 32 && info.headerSize <= bytes.size());
            CHECK(info.payloadOffset >= info.headerSize && info.payloadOffset <= bytes.size());
            CHECK(info.payloadSize != 0 && info.payloadSize <= bytes.size() - info.payloadOffset);
        } else {
            CHECK(info.fileSize == 0 && info.payloadOffset == 0 && info.payloadSize == 0);
        }
    }
}
