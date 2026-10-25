#include "Firmware.hpp"

namespace cherryred::navi33 {
namespace {
uint16_t le16(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
uint32_t le32(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

FirmwareError inspectFirmware(const uint8_t* bytes, size_t size,
                              FirmwareInfo& out) noexcept {
    out = {};
    if (!bytes) return FirmwareError::NullInput;
    if (size < 32) return FirmwareError::Truncated;
    // Wire layout: AMD common_firmware_header, documented in SOURCES.md.
    // Decode bytes individually: firmware can be unaligned and host endian
    // must not affect results. No packed-struct casts or offset additions.
    FirmwareInfo info;
    info.fileSize = le32(bytes);
    info.headerSize = le32(bytes + 4);
    info.headerMajor = le16(bytes + 8);
    info.headerMinor = le16(bytes + 10);
    info.ipMajor = le16(bytes + 12);
    info.ipMinor = le16(bytes + 14);
    info.ucodeVersion = le32(bytes + 16);
    info.payloadSize = le32(bytes + 20);
    info.payloadOffset = le32(bytes + 24);
    info.declaredCrc32 = le32(bytes + 28);
    if (size != info.fileSize) return FirmwareError::SizeMismatch;
    if (info.headerSize < 32 || info.headerSize > size)
        return FirmwareError::InvalidHeader;
    if (info.payloadSize == 0 || info.payloadOffset < info.headerSize ||
        info.payloadOffset > size || info.payloadSize > size - info.payloadOffset)
        return FirmwareError::InvalidPayload;
    out = info;
    return FirmwareError::None;
}

const char* firmwareErrorName(FirmwareError error) noexcept {
    switch (error) {
        case FirmwareError::None: return "bounded common envelope";
        case FirmwareError::NullInput: return "null input";
        case FirmwareError::Truncated: return "truncated common header";
        case FirmwareError::SizeMismatch: return "declared/file size mismatch";
        case FirmwareError::InvalidHeader: return "invalid header extent";
        case FirmwareError::InvalidPayload: return "invalid payload extent";
    }
    return "unknown error";
}
}  // namespace cherryred::navi33
