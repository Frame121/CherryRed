#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cherryred::navi33 {

enum class DiscoveryError {
    None, NullInput, Truncated, Signature, BinaryVersion, BinaryBounds,
    BinaryChecksum, TableBounds, TableChecksum, TableVersion, UnsupportedDies,
    UnsupportedAddressWidth, DieMismatch, RecordBounds, DuplicateIp
};

struct DiscoveryRecord {
    uint16_t hwId = 0;
    uint8_t instance = 0;
    uint8_t major = 0, minor = 0, revision = 0;
    // Byte 7: v1/v2 harvest/reserved; v3/v4 subrevision/variant.
    // Preserve raw bits; separate HARVEST_INFO is not decoded here.
    uint8_t attributes = 0;
    uint8_t baseCount = 0;
    uint32_t bases[16]{}; // dword addresses, not CPU or GPU virtual addresses
};

// A bounded, read-only view of a binary starting at its signature (no scan).
// Caller owns bytes and must keep them alive and immutable while using this
// object, and serialize calls. No hardware/provenance claim is made.
class Discovery {
public:
    static constexpr size_t kMaxRecords = 256;
    DiscoveryError initialize(const uint8_t* bytes, size_t size) noexcept;
    size_t count() const noexcept { return count_; }
    uint16_t tableVersion() const noexcept { return version_; }
    uint16_t dieId() const noexcept { return dieId_; }
    bool at(size_t index, DiscoveryRecord& out) const noexcept;
    bool find(uint16_t hwId, uint8_t instance, DiscoveryRecord& out) const noexcept;
    // Resolve a 32-bit register offset, including the entire four-byte access
    // in the caller-supplied aperture. No register read or mapping occurs.
    bool registerByteOffset(uint16_t hwId, uint8_t instance, uint8_t segment,
                            uint32_t regDword, uint64_t apertureBytes,
                            uint32_t& out) const noexcept;

private:
    const uint8_t* bytes_ = nullptr;
    size_t tableEnd_ = 0;
    size_t count_ = 0;
    uint16_t version_ = 0, dieId_ = 0;
    uint16_t offsets_[kMaxRecords]{};
};

const char* discoveryErrorName(DiscoveryError error) noexcept;
}  // namespace cherryred::navi33
