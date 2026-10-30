#include "Navi33IpDiscovery.hpp"

#include <cstddef>
#include <cstdint>

namespace cobalt64::research {
namespace {

constexpr std::uint32_t kBinarySignature = 0x28211407U;
constexpr std::uint32_t kIpDiscoverySignature = 0x53445049U;
constexpr std::size_t kBinaryHeaderSize = 12U;
constexpr std::size_t kTableInfoSize = 8U;
constexpr std::size_t kV1TableCount = 6U;
constexpr std::size_t kV1TableListOffset = 12U;
constexpr std::size_t kV2TableListOffset = 16U;
constexpr std::size_t kIpDiscoveryHeaderSize = 80U;
constexpr std::size_t kDieHeaderSize = 4U;
constexpr std::size_t kMaxDies = 16U;
constexpr std::size_t kMaxRecords = 256U;
constexpr std::size_t kMaxBaseAddresses = 16U;

Navi33IpDiscoveryResult failure(Navi33IpDiscoveryError error,
                                const char* message) noexcept {
    Navi33IpDiscoveryResult result{};
    result.error = error;
    result.message = message;
    return result;
}

bool rangeFits(std::size_t offset, std::size_t length,
               std::size_t limit) noexcept {
    return offset <= limit && length <= limit - offset;
}

bool readU16(const std::uint8_t* data, std::size_t size,
             std::size_t offset, std::uint16_t& value) noexcept {
    if (!rangeFits(offset, 2U, size)) return false;
    value = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[offset]) |
        (static_cast<std::uint16_t>(data[offset + 1U]) << 8U));
    return true;
}

bool readU32(const std::uint8_t* data, std::size_t size,
             std::size_t offset, std::uint32_t& value) noexcept {
    if (!rangeFits(offset, 4U, size)) return false;
    value = static_cast<std::uint32_t>(data[offset]) |
            (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
            (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
            (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
    return true;
}

std::uint16_t byteSum(const std::uint8_t* data, std::size_t offset,
                      std::size_t length) noexcept {
    std::uint16_t sum = 0;
    for (std::size_t i = 0; i < length; ++i) {
        sum = static_cast<std::uint16_t>(sum + data[offset + i]);
    }
    return sum;
}

struct DieRange {
    std::size_t start = 0;
    std::size_t end = 0;
    std::uint16_t id = 0;
    std::uint16_t count = 0;
};

bool walkRecords(const std::uint8_t* data, std::size_t tableEnd,
                 const DieRange* dies, std::size_t dieCount,
                 Navi33DiscoveredIp* output, std::size_t& outputCount,
                 Navi33IpDiscoveryError& error) noexcept {
    outputCount = 0;
    for (std::size_t dieIndex = 0; dieIndex < dieCount; ++dieIndex) {
        std::size_t cursor = dies[dieIndex].start + kDieHeaderSize;
        for (std::size_t recordIndex = 0;
             recordIndex < dies[dieIndex].count; ++recordIndex) {
            if (!rangeFits(cursor, 8U, dies[dieIndex].end)) {
                error = Navi33IpDiscoveryError::TruncatedRecord;
                return false;
            }

            std::uint16_t hwId = 0;
            if (!readU16(data, tableEnd, cursor, hwId)) {
                error = Navi33IpDiscoveryError::TruncatedRecord;
                return false;
            }
            const std::uint8_t instance = data[cursor + 2U];
            const std::uint8_t baseCount = data[cursor + 3U];
            const std::uint8_t major = data[cursor + 4U];
            const std::uint8_t minor = data[cursor + 5U];
            const std::uint8_t revision = data[cursor + 6U];
            const std::uint8_t subRevisionAndVariant = data[cursor + 7U];

            if (baseCount > kMaxBaseAddresses) {
                error = Navi33IpDiscoveryError::TooManyBaseAddresses;
                return false;
            }
            const std::size_t baseBytes =
                static_cast<std::size_t>(baseCount) * sizeof(std::uint32_t);
            if (!rangeFits(cursor + 8U, baseBytes, dies[dieIndex].end)) {
                error = Navi33IpDiscoveryError::TruncatedRecord;
                return false;
            }

            if (output != nullptr) {
                Navi33DiscoveredIp& record = output[outputCount];
                record.hwId = hwId;
                record.instance = instance;
                record.major = major;
                record.minor = minor;
                record.revision = revision;
                record.subRevision =
                    static_cast<std::uint8_t>(subRevisionAndVariant & 0x0FU);
                record.variant =
                    static_cast<std::uint8_t>((subRevisionAndVariant >> 4U) & 0x0FU);
                record.numBaseAddresses = baseCount;
                record.dieIndex = static_cast<std::uint8_t>(dieIndex);
                for (std::size_t baseIndex = 0; baseIndex < baseCount;
                     ++baseIndex) {
                    std::uint32_t base = 0;
                    if (!readU32(data, tableEnd, cursor + 8U +
                                 baseIndex * sizeof(std::uint32_t), base)) {
                        error = Navi33IpDiscoveryError::TruncatedRecord;
                        return false;
                    }
                    record.baseAddresses[baseIndex] = base;
                }
            }

            cursor += 8U + baseBytes;
            ++outputCount;
            if (outputCount > kMaxRecords) {
                error = Navi33IpDiscoveryError::TooManyRecords;
                return false;
            }
        }
    }
    return true;
}

}  // namespace

const char* navi33IpDiscoveryErrorName(Navi33IpDiscoveryError error) noexcept {
    switch (error) {
        case Navi33IpDiscoveryError::None: return "None";
        case Navi33IpDiscoveryError::NullInput: return "NullInput";
        case Navi33IpDiscoveryError::NullOutput: return "NullOutput";
        case Navi33IpDiscoveryError::InputTooSmall: return "InputTooSmall";
        case Navi33IpDiscoveryError::BinaryNotFound: return "BinaryNotFound";
        case Navi33IpDiscoveryError::UnsupportedBinaryVersion: return "UnsupportedBinaryVersion";
        case Navi33IpDiscoveryError::InvalidBinarySize: return "InvalidBinarySize";
        case Navi33IpDiscoveryError::BinaryChecksumMismatch: return "BinaryChecksumMismatch";
        case Navi33IpDiscoveryError::TableDirectoryInvalid: return "TableDirectoryInvalid";
        case Navi33IpDiscoveryError::IpDiscoveryMissing: return "IpDiscoveryMissing";
        case Navi33IpDiscoveryError::IpDiscoveryHeaderInvalid: return "IpDiscoveryHeaderInvalid";
        case Navi33IpDiscoveryError::IpDiscoveryChecksumMismatch: return "IpDiscoveryChecksumMismatch";
        case Navi33IpDiscoveryError::UnsupportedIpDiscoveryVersion: return "UnsupportedIpDiscoveryVersion";
        case Navi33IpDiscoveryError::InvalidDieLayout: return "InvalidDieLayout";
        case Navi33IpDiscoveryError::TooManyRecords: return "TooManyRecords";
        case Navi33IpDiscoveryError::TooManyBaseAddresses: return "TooManyBaseAddresses";
        case Navi33IpDiscoveryError::TruncatedRecord: return "TruncatedRecord";
        case Navi33IpDiscoveryError::OutputCapacityTooSmall: return "OutputCapacityTooSmall";
    }
    return "UnknownError";
}

Navi33IpDiscoveryResult parseNavi33IpDiscovery(
    const std::uint8_t* data, std::size_t size,
    Navi33DiscoveredIp* output, std::size_t outputCapacity) noexcept {
    if (data == nullptr) {
        return failure(Navi33IpDiscoveryError::NullInput, "discovery input is null");
    }
    if (output == nullptr || outputCapacity == 0U) {
        return failure(Navi33IpDiscoveryError::NullOutput, "non-empty output buffer is required");
    }
    if (size < kBinaryHeaderSize) {
        return failure(Navi33IpDiscoveryError::InputTooSmall, "input is smaller than the binary header");
    }

    std::size_t binaryOffset = size;
    for (std::size_t offset = 0; rangeFits(offset, kBinaryHeaderSize, size);) {
        std::uint32_t signature = 0;
        if (readU32(data, size, offset, signature) &&
            signature == kBinarySignature) {
            binaryOffset = offset;
            break;
        }
        if (size - offset <= kBinaryHeaderSize + 3U) break;
        offset += 4U;
    }
    if (binaryOffset == size) {
        return failure(Navi33IpDiscoveryError::BinaryNotFound,
                       "AMD Discovery binary signature was not found");
    }

    std::uint16_t binaryMajor = 0, binaryMinor = 0;
    std::uint16_t expectedBinaryChecksum = 0, binarySize = 0;
    if (!readU16(data, size, binaryOffset + 4U, binaryMajor) ||
        !readU16(data, size, binaryOffset + 6U, binaryMinor) ||
        !readU16(data, size, binaryOffset + 8U, expectedBinaryChecksum) ||
        !readU16(data, size, binaryOffset + 10U, binarySize)) {
        return failure(Navi33IpDiscoveryError::InvalidBinarySize,
                       "binary header fields are truncated");
    }
    if (binaryMajor != 1U && binaryMajor != 2U) {
        return failure(Navi33IpDiscoveryError::UnsupportedBinaryVersion,
                       "only Discovery binary major versions 1 and 2 are supported");
    }
    if (binarySize < kBinaryHeaderSize ||
        !rangeFits(binaryOffset, binarySize, size)) {
        return failure(Navi33IpDiscoveryError::InvalidBinarySize,
                       "declared binary range exceeds the supplied input");
    }
    const std::size_t binaryEnd = binaryOffset + binarySize;
    if (byteSum(data, binaryOffset + 10U, binarySize - 10U) !=
        expectedBinaryChecksum) {
        return failure(Navi33IpDiscoveryError::BinaryChecksumMismatch,
                       "Discovery binary checksum does not match");
    }

    std::size_t tableListOffset = kV1TableListOffset;
    std::size_t tableCount = kV1TableCount;
    if (binaryMajor == 2U) {
        std::uint16_t count = 0;
        if (binarySize < kV2TableListOffset ||
            !readU16(data, binaryEnd, binaryOffset + 12U, count) ||
            count == 0U || count > 64U) {
            return failure(Navi33IpDiscoveryError::TableDirectoryInvalid,
                           "v2 table count is invalid");
        }
        tableListOffset = kV2TableListOffset;
        tableCount = count;
    }
    const std::size_t tableListBytes = tableCount * kTableInfoSize;
    if (!rangeFits(tableListOffset, tableListBytes, binarySize)) {
        return failure(Navi33IpDiscoveryError::TableDirectoryInvalid,
                       "table directory exceeds the declared binary");
    }
    const std::size_t tableListEnd = tableListOffset + tableListBytes;

    std::uint16_t ipDiscoveryOffset16 = 0;
    std::uint16_t ipDiscoveryChecksum = 0;
    std::uint16_t ipDiscoverySize16 = 0;
    const std::size_t ipTableInfo = binaryOffset + tableListOffset;
    if (!readU16(data, binaryEnd, ipTableInfo, ipDiscoveryOffset16) ||
        !readU16(data, binaryEnd, ipTableInfo + 2U, ipDiscoveryChecksum) ||
        !readU16(data, binaryEnd, ipTableInfo + 4U, ipDiscoverySize16)) {
        return failure(Navi33IpDiscoveryError::TableDirectoryInvalid,
                       "IP Discovery table entry is truncated");
    }
    const std::size_t ipDiscoveryOffset = ipDiscoveryOffset16;
    const std::size_t ipDiscoverySize = ipDiscoverySize16;
    if (ipDiscoveryOffset == 0U || ipDiscoverySize < kIpDiscoveryHeaderSize) {
        return failure(Navi33IpDiscoveryError::IpDiscoveryMissing,
                       "IP Discovery table is absent or too small");
    }
    if (ipDiscoveryOffset < tableListEnd ||
        !rangeFits(ipDiscoveryOffset, ipDiscoverySize, binarySize)) {
        return failure(Navi33IpDiscoveryError::TableDirectoryInvalid,
                       "IP Discovery table overlaps the directory or exceeds the binary");
    }
    const std::size_t ipDiscoveryEnd = ipDiscoveryOffset + ipDiscoverySize;
    const std::size_t ipDiscoveryAbsolute = binaryOffset + ipDiscoveryOffset;

    std::uint32_t ipSignature = 0;
    std::uint16_t ipVersion = 0, declaredIpSize = 0, dieCount16 = 0;
    if (!readU32(data, binaryEnd, ipDiscoveryAbsolute, ipSignature) ||
        !readU16(data, binaryEnd, ipDiscoveryAbsolute + 4U, ipVersion) ||
        !readU16(data, binaryEnd, ipDiscoveryAbsolute + 6U, declaredIpSize) ||
        !readU16(data, binaryEnd, ipDiscoveryAbsolute + 12U, dieCount16) ||
        ipSignature != kIpDiscoverySignature ||
        declaredIpSize != ipDiscoverySize ||
        !rangeFits(ipDiscoveryOffset, kIpDiscoveryHeaderSize, binarySize)) {
        return failure(Navi33IpDiscoveryError::IpDiscoveryHeaderInvalid,
                       "IP Discovery header signature, size, or fixed header is invalid");
    }
    if (byteSum(data, ipDiscoveryAbsolute, ipDiscoverySize) !=
        ipDiscoveryChecksum) {
        return failure(Navi33IpDiscoveryError::IpDiscoveryChecksumMismatch,
                       "IP Discovery table checksum does not match");
    }
    if (ipVersion != 3U) {
        return failure(Navi33IpDiscoveryError::UnsupportedIpDiscoveryVersion,
                       "this RX 7600 parser accepts IP Discovery table version 3 only");
    }
    if (dieCount16 == 0U || dieCount16 > kMaxDies) {
        return failure(Navi33IpDiscoveryError::InvalidDieLayout,
                       "die count is outside the supported range");
    }

    DieRange dies[kMaxDies]{};
    std::size_t totalRecords = 0;
    for (std::size_t dieIndex = 0; dieIndex < dieCount16; ++dieIndex) {
        std::uint16_t dieId = 0, dieOffset16 = 0, ipCount16 = 0;
        const std::size_t dieInfoOffset =
            ipDiscoveryAbsolute + 14U + dieIndex * 4U;
        if (!readU16(data, binaryEnd, dieInfoOffset, dieId) ||
            !readU16(data, binaryEnd, dieInfoOffset + 2U, dieOffset16)) {
            return failure(Navi33IpDiscoveryError::InvalidDieLayout,
                           "die information entry is truncated");
        }
        const std::size_t dieOffset = dieOffset16;
        if (dieOffset < ipDiscoveryOffset + kIpDiscoveryHeaderSize ||
            !rangeFits(dieOffset, kDieHeaderSize, ipDiscoveryEnd)) {
            return failure(Navi33IpDiscoveryError::InvalidDieLayout,
                           "die header is outside the IP Discovery table");
        }
        const std::size_t dieAbsolute = binaryOffset + dieOffset;
        std::uint16_t storedDieId = 0;
        if (!readU16(data, binaryEnd, dieAbsolute, storedDieId) ||
            !readU16(data, binaryEnd, dieAbsolute + 2U, ipCount16) ||
            storedDieId != dieId || ipCount16 == 0U ||
            ipCount16 > kMaxRecords) {
            return failure(Navi33IpDiscoveryError::InvalidDieLayout,
                           "die header identity or record count is invalid");
        }
        dies[dieIndex].start = dieAbsolute;
        dies[dieIndex].end = binaryOffset + ipDiscoveryEnd;
        dies[dieIndex].id = dieId;
        dies[dieIndex].count = ipCount16;
        totalRecords += ipCount16;
        if (totalRecords > kMaxRecords) {
            return failure(Navi33IpDiscoveryError::TooManyRecords,
                           "total IP record count exceeds the parser limit");
        }
    }

    Navi33IpDiscoveryError walkError = Navi33IpDiscoveryError::None;
    std::size_t validatedCount = 0;
    if (!walkRecords(data, binaryEnd, dies, dieCount16, nullptr,
                     validatedCount, walkError) ||
        validatedCount != totalRecords) {
        return failure(walkError == Navi33IpDiscoveryError::None
                           ? Navi33IpDiscoveryError::TruncatedRecord : walkError,
                       "one or more variable-size IP records are malformed");
    }

    // Calculate each die's exact record end and reject overlapping die regions.
    for (std::size_t dieIndex = 0; dieIndex < dieCount16; ++dieIndex) {
        std::size_t cursor = dies[dieIndex].start + kDieHeaderSize;
        for (std::size_t recordIndex = 0;
             recordIndex < dies[dieIndex].count; ++recordIndex) {
            const std::uint8_t baseCount = data[cursor + 3U];
            cursor += 8U + static_cast<std::size_t>(baseCount) * 4U;
        }
        dies[dieIndex].end = cursor;
        for (std::size_t other = 0; other < dieIndex; ++other) {
            if (dies[dieIndex].start < dies[other].end &&
                dies[other].start < dies[dieIndex].end) {
                return failure(Navi33IpDiscoveryError::InvalidDieLayout,
                               "die record regions overlap");
            }
        }
    }

    if (outputCapacity < totalRecords) {
        return failure(Navi33IpDiscoveryError::OutputCapacityTooSmall,
                       "output buffer cannot hold every discovered IP record");
    }

    std::size_t writtenCount = 0;
    if (!walkRecords(data, binaryEnd, dies, dieCount16, output,
                     writtenCount, walkError) ||
        writtenCount != totalRecords) {
        return failure(walkError == Navi33IpDiscoveryError::None
                           ? Navi33IpDiscoveryError::TruncatedRecord : walkError,
                       "validated record copy unexpectedly failed");
    }

    Navi33IpDiscoveryResult result{};
    result.valid = true;
    result.error = Navi33IpDiscoveryError::None;
    result.binaryOffset = binaryOffset;
    result.binaryMajor = binaryMajor;
    result.binaryMinor = binaryMinor;
    result.declaredBinarySize = binarySize;
    result.ipDiscoveryVersion = ipVersion;
    result.dieCount = dieCount16;
    result.recordCount = writtenCount;
    result.message =
        "IP Discovery parsed offline; base addresses are metadata only and no hardware access is permitted.";
    return result;
}

}  // namespace cobalt64::research
