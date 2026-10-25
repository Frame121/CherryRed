#include "Discovery.hpp"

namespace cherryred::navi33 {
namespace {
uint16_t u16(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
uint32_t u32(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t checksum(const uint8_t* bytes, size_t size) noexcept {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; ++i) sum += bytes[i];
    return static_cast<uint16_t>(sum);
}
}  // namespace

DiscoveryError Discovery::initialize(const uint8_t* bytes, size_t size) noexcept {
    bytes_ = nullptr;
    count_ = tableEnd_ = 0;
    version_ = dieId_ = 0;
    if (!bytes) return DiscoveryError::NullInput;
    if (size < 512) return DiscoveryError::Truncated;
    if (u32(bytes) != 0x28211407) return DiscoveryError::Signature;
    const auto major = u16(bytes + 4);
    if (major > 2) return DiscoveryError::BinaryVersion;
    const size_t binarySize = u16(bytes + 10);
    const size_t list = major == 2 ? 16 : 12;
    const size_t tables = major == 2 ? u16(bytes + 12) : 6;
    if (tables == 0 || tables > 6 || binarySize > size || binarySize < list + tables * 8)
        return DiscoveryError::BinaryBounds;
    const size_t table = u16(bytes + list);
    const size_t tableSize = u16(bytes + list + 4);
    if (table < list + tables * 8 || table > binarySize ||
        tableSize < 80 || tableSize > binarySize - table)
        return DiscoveryError::TableBounds;
    if (checksum(bytes + 10, binarySize - 10) != u16(bytes + 8))
        return DiscoveryError::BinaryChecksum;
    const auto* header = bytes + table;
    if (u32(header) != 0x53445049) return DiscoveryError::Signature;
    if (u16(header + 6) != tableSize) return DiscoveryError::TableBounds;
    const auto version = u16(header + 4);
    if (version < 1 || version > 4) return DiscoveryError::TableVersion;
    if (version == 4 && (header[78] & 1)) return DiscoveryError::UnsupportedAddressWidth;
    if (u16(header + 12) != 1) return DiscoveryError::UnsupportedDies;
    if (checksum(header, tableSize) != u16(bytes + list + 2))
        return DiscoveryError::TableChecksum;
    const size_t end = table + tableSize;
    const size_t die = u16(header + 16);
    if (die < table + 80 || die > end || end - die < 4)
        return DiscoveryError::TableBounds;
    if (u16(bytes + die) != u16(header + 14)) return DiscoveryError::DieMismatch;
    const size_t count = u16(bytes + die + 2);
    if (!count || count > kMaxRecords) return DiscoveryError::RecordBounds;
    size_t cursor = die + 4;
    for (size_t i = 0; i < count; ++i) {
        if (cursor > end || end - cursor < 8) return DiscoveryError::RecordBounds;
        const size_t bases = bytes[cursor + 3];
        if (bases > 16 || bases * 4 > end - cursor - 8)
            return DiscoveryError::RecordBounds;
        // Unique keys are required before lookup can select a single record.
        for (size_t j = 0; j < i; ++j) {
            const auto previous = offsets_[j];
            if (u16(bytes + previous) == u16(bytes + cursor) &&
                bytes[previous + 2] == bytes[cursor + 2]) return DiscoveryError::DuplicateIp;
        }
        offsets_[i] = static_cast<uint16_t>(cursor);
        cursor += 8 + bases * 4;
    }
    bytes_ = bytes;
    tableEnd_ = end;
    count_ = count;
    version_ = version;
    dieId_ = u16(bytes + die);
    return DiscoveryError::None;
}

bool Discovery::at(size_t index, DiscoveryRecord& out) const noexcept {
    out = {};
    if (!bytes_ || index >= count_) return false;
    const size_t offset = offsets_[index];
    if (offset > tableEnd_ || tableEnd_ - offset < 8) return false;
    const auto* record = bytes_ + offset;
    const size_t bases = record[3];
    if (bases > 16 || bases * 4 > tableEnd_ - offset - 8) return false;
    out.hwId = u16(record);
    out.instance = record[2];
    out.baseCount = record[3];
    out.major = record[4];
    out.minor = record[5];
    out.revision = record[6];
    out.attributes = record[7];
    for (size_t i = 0; i < bases; ++i) out.bases[i] = u32(record + 8 + i * 4);
    return true;
}

bool Discovery::find(uint16_t hwId, uint8_t instance, DiscoveryRecord& out) const noexcept {
    out = {};
    for (size_t i = 0; i < count_; ++i) {
        DiscoveryRecord record;
        if (!at(i, record)) return false;
        if (record.hwId == hwId && record.instance == instance) {
            out = record;
            return true;
        }
    }
    return false;
}

bool Discovery::registerByteOffset(uint16_t hwId, uint8_t instance, uint8_t segment,
                                   uint32_t regDword, uint64_t apertureBytes,
                                   uint32_t& out) const noexcept {
    out = 0;
    DiscoveryRecord record;
    if (!find(hwId, instance, record) || segment >= record.baseCount) return false;
    const uint64_t offset = (static_cast<uint64_t>(record.bases[segment]) + regDword) * 4;
    if (offset > UINT32_MAX || offset > apertureBytes || apertureBytes - offset < 4) return false;
    out = static_cast<uint32_t>(offset);
    return true;
}

const char* discoveryErrorName(DiscoveryError error) noexcept {
    switch (error) {
        case DiscoveryError::None: return "bounded IP Discovery table";
        case DiscoveryError::NullInput: return "null input";
        case DiscoveryError::Truncated: return "input shorter than 512 bytes";
        case DiscoveryError::Signature: return "invalid signature";
        case DiscoveryError::BinaryVersion: return "unsupported binary version";
        case DiscoveryError::BinaryBounds: return "invalid binary/table-list bounds";
        case DiscoveryError::BinaryChecksum: return "binary checksum mismatch";
        case DiscoveryError::TableBounds: return "invalid table/die bounds";
        case DiscoveryError::TableChecksum: return "table checksum mismatch";
        case DiscoveryError::TableVersion: return "unsupported table version";
        case DiscoveryError::UnsupportedDies: return "only single-die tables supported";
        case DiscoveryError::UnsupportedAddressWidth: return "64-bit bases unsupported";
        case DiscoveryError::DieMismatch: return "die IDs disagree";
        case DiscoveryError::RecordBounds: return "invalid record/base-array bounds";
        case DiscoveryError::DuplicateIp: return "duplicate hardware ID/instance";
    }
    return "unknown discovery error";
}
}  // namespace cherryred::navi33
