#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cherryred::navi33 {

enum class FirmwareError {
    None, NullInput, Truncated, SizeMismatch, InvalidHeader, InvalidPayload
};

// Common AMD firmware envelope only. No IP-specific subimages are decoded.
// Numeric metadata is copied; the caller's byte buffer is never retained.
struct FirmwareInfo {
    uint32_t fileSize = 0;
    uint32_t headerSize = 0;
    uint16_t headerMajor = 0;
    uint16_t headerMinor = 0;
    uint16_t ipMajor = 0;
    uint16_t ipMinor = 0;
    uint32_t ucodeVersion = 0;
    uint32_t payloadSize = 0;
    uint32_t payloadOffset = 0;
    uint32_t declaredCrc32 = 0;
};

// On error, clears out. Success establishes bounded common-header fields,
// not checksum/signature validity, subimage layout, or hardware compatibility.
FirmwareError inspectFirmware(const uint8_t* bytes, size_t size,
                              FirmwareInfo& out) noexcept;
const char* firmwareErrorName(FirmwareError error) noexcept;

}  // namespace cherryred::navi33
