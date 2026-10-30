#include "Navi33Gfx11Manifest.hpp"

namespace cobalt64::research {
namespace {

bool validVersion(const Gfx11IpVersion& v) noexcept {
    return v.major != 0;
}

bool hexString(std::string_view value, std::size_t expected) noexcept {
    if (value.size() != expected) return false;
    for (char c : value) {
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        const bool upper = c >= 'A' && c <= 'F';
        if (!digit && !lower && !upper) return false;
    }
    return true;
}

bool startsWith(std::string_view value, std::string_view prefix) noexcept {
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

bool makePrefix(const char* family, const Gfx11IpVersion& version,
                bool trailingUnderscore, char* output,
                std::size_t capacity) noexcept {
    if (capacity < 16 || !validVersion(version)) return false;
    const unsigned a = version.major;
    const unsigned b = version.minor;
    const unsigned c = version.revision;
    // Avoid snprintf/CRT dependencies in the portable kernel-facing core.
    char digits[3][4]{};
    const unsigned values[3] = {a, b, c};
    for (unsigned i = 0; i < 3; ++i) {
        if (values[i] > 255) return false;
        unsigned n = values[i];
        unsigned pos = 3;
        do {
            digits[i][--pos] = static_cast<char>('0' + (n % 10));
            n /= 10;
        } while (n != 0 && pos > 0);
        digits[i][3] = '\0';
        if (pos > 0) {
            for (unsigned j = 0; j <= 3 - pos; ++j)
                digits[i][j] = digits[i][pos + j];
        }
    }
    std::size_t out = 0;
    const char* parts[] = {family, "_", digits[0], "_", digits[1], "_",
                           digits[2], trailingUnderscore ? "_" : ""};
    for (const char* part : parts) {
        for (std::size_t i = 0; part[i] != '\0'; ++i) {
            if (out + 1 >= capacity) return false;
            output[out++] = part[i];
        }
    }
    output[out] = '\0';
    return true;
}

const Gfx11IpVersion* roleVersion(Gfx11FirmwareRole role,
                                  const Gfx11Manifest& m) noexcept {
    switch (role) {
        case Gfx11FirmwareRole::PspSos:
        case Gfx11FirmwareRole::PspTa: return &m.psp;
        case Gfx11FirmwareRole::Smu: return &m.smu;
        case Gfx11FirmwareRole::GcPfp:
        case Gfx11FirmwareRole::GcMe:
        case Gfx11FirmwareRole::GcMec:
        case Gfx11FirmwareRole::GcRlc: return &m.gc;
        case Gfx11FirmwareRole::Sdma: return &m.sdma;
    }
    return nullptr;
}

const char* roleFamily(Gfx11FirmwareRole role) noexcept {
    switch (role) {
        case Gfx11FirmwareRole::PspSos:
        case Gfx11FirmwareRole::PspTa: return "psp";
        case Gfx11FirmwareRole::Smu: return "smu";
        case Gfx11FirmwareRole::GcPfp:
        case Gfx11FirmwareRole::GcMe:
        case Gfx11FirmwareRole::GcMec:
        case Gfx11FirmwareRole::GcRlc: return "gc";
        case Gfx11FirmwareRole::Sdma: return "sdma";
    }
    return "";
}

bool required(Gfx11FirmwareRole role) noexcept {
    return role != Gfx11FirmwareRole::PspTa;
}

} // namespace

Gfx11ManifestResult validateGfx11Manifest(
    const Gfx11Manifest& m) noexcept {
    if (m.target.vendorId != 0x1002 || m.target.deviceId != 0x7480 ||
        m.target.subsystemVendorId != 0x148c || m.target.subsystemId != 0x2421)
        return {false, Gfx11ManifestError::WrongTarget};
    if (m.evidence != Gfx11Evidence::BoardAttributed &&
        m.evidence != Gfx11Evidence::HardwareVerified)
        return {false, Gfx11ManifestError::InsufficientEvidence};
    if (!m.vbiosIdentityMatched)
        return {false, Gfx11ManifestError::MissingVbiosMatch};
    if (!validVersion(m.gc) || !validVersion(m.psp) ||
        !validVersion(m.smu) || !validVersion(m.sdma))
        return {false, Gfx11ManifestError::InvalidIpVersion};
    if (m.gc.major != 11 || m.psp.major != 13 ||
        m.smu.major != 13 || m.sdma.major != 6)
        return {false, Gfx11ManifestError::UnsupportedIpFamily};
    if (m.firmware == nullptr || m.firmwareCount == 0)
        return {false, Gfx11ManifestError::MissingFirmware};
    if (m.firmwareCount > 16)
        return {false, Gfx11ManifestError::TooManyFirmwareRecords};

    bool seen[8]{};
    for (std::size_t i = 0; i < m.firmwareCount; ++i) {
        const auto& f = m.firmware[i];
        const auto role = static_cast<unsigned>(f.role);
        if (role >= 8 || f.filename.empty() || f.byteLength == 0 ||
            f.byteLength > 64U * 1024U * 1024U ||
            !hexString(f.sha256, 64) || !hexString(f.upstreamCommit, 40) ||
            !f.boardAttributed || !f.licenseApproved)
            return {false, Gfx11ManifestError::InvalidFirmwareRecord};
        if (seen[role])
            return {false, Gfx11ManifestError::DuplicateFirmwareRole};
        seen[role] = true;

        char prefix[32]{};
        const auto* version = roleVersion(f.role, m);
        const bool prefixBuilt = version != nullptr &&
            makePrefix(roleFamily(f.role), *version,
                       f.role != Gfx11FirmwareRole::Smu &&
                           f.role != Gfx11FirmwareRole::Sdma,
                       prefix, sizeof(prefix));
        if (!prefixBuilt || !startsWith(f.filename, prefix))
            return {false, Gfx11ManifestError::WrongFirmwarePrefix};
    }

    for (unsigned role = 0; role < 8; ++role) {
        const auto typedRole = static_cast<Gfx11FirmwareRole>(role);
        if (required(typedRole) && !seen[role])
            return {false, Gfx11ManifestError::MissingRequiredRole};
    }
    return {true, Gfx11ManifestError::None};
}

} // namespace cobalt64::research
