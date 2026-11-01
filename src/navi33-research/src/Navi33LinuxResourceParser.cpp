#include "Navi33LinuxResourceParser.hpp"

#include <cstdint>

namespace cobalt64::research {
namespace {
LinuxResourceParseResult fail(LinuxResourceParseError error, std::size_t line,
                              const char* message) noexcept {
    LinuxResourceParseResult result{};
    result.error = error;
    result.errorLine = line;
    result.message = message;
    result.validation.mappingPermitted = false;
    result.validation.registerWritesPermitted = false;
    return result;
}
bool hexDigit(char c, unsigned& value) noexcept {
    if (c >= '0' && c <= '9') { value = static_cast<unsigned>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { value = static_cast<unsigned>(c - 'a') + 10U; return true; }
    if (c >= 'A' && c <= 'F') { value = static_cast<unsigned>(c - 'A') + 10U; return true; }
    return false;
}
bool parseHex(const char*& p, std::uint64_t& value) noexcept {
    if (p[0] != '0' || (p[1] != 'x' && p[1] != 'X')) return false;
    p += 2;
    unsigned digit = 0;
    if (!hexDigit(*p, digit)) return false;
    value = 0;
    do {
        if (value > (UINT64_MAX - digit) / 16U) return false;
        value = value * 16U + digit;
        ++p;
    } while (hexDigit(*p, digit));
    return true;
}
bool space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r'; }
void skipSpace(const char*& p) noexcept { while (space(*p)) ++p; }
}
const char* linuxResourceParseErrorName(LinuxResourceParseError e) noexcept {
    switch (e) {
        case LinuxResourceParseError::None: return "None";
        case LinuxResourceParseError::NullInput: return "NullInput";
        case LinuxResourceParseError::MalformedLine: return "MalformedLine";
        case LinuxResourceParseError::InvalidHex: return "InvalidHex";
        case LinuxResourceParseError::TooManyLines: return "TooManyLines";
        case LinuxResourceParseError::ConflictingTypeFlags: return "ConflictingTypeFlags";
        case LinuxResourceParseError::UnsupportedResourceType: return "UnsupportedResourceType";
        case LinuxResourceParseError::DisabledResource: return "DisabledResource";
        case LinuxResourceParseError::ValidationFailed: return "ValidationFailed";
    }
    return "UnknownError";
}
LinuxResourceParseResult parseLinuxPciResourceText(
    const char* text, PciResource* output, std::size_t capacity) noexcept {
    if (text == nullptr) return fail(LinuxResourceParseError::NullInput, 0, "null resource text");
    if (capacity != 0 && output == nullptr)
        return fail(LinuxResourceParseError::NullInput, 0, "null output with nonzero capacity");
    PciResource parsed[6]{};
    std::size_t count = 0, line = 0;
    const char* p = text;
    while (*p != '\0') {
        ++line;
        skipSpace(p);
        if (*p == '\n') { ++p; continue; }
        if (*p == '\0') break;
        std::uint64_t start = 0, end = 0, flags = 0;
        if (!parseHex(p, start)) return fail(LinuxResourceParseError::InvalidHex, line, "invalid start address");
        skipSpace(p);
        if (!parseHex(p, end)) return fail(LinuxResourceParseError::InvalidHex, line, "invalid end address");
        skipSpace(p);
        if (!parseHex(p, flags)) return fail(LinuxResourceParseError::InvalidHex, line, "invalid flags");
        skipSpace(p);
        if (*p != '\n' && *p != '\0')
            return fail(LinuxResourceParseError::MalformedLine, line, "unexpected characters after flags");
        if (*p == '\n') ++p;
        if (start == 0 && end == 0 && flags == 0) continue;
        // Linux IORESOURCE_DISABLED (0x10000000) must fail closed even if
        // the resource still carries non-zero start/end values.
        if ((flags & 0x10000000ULL) != 0)
            return fail(LinuxResourceParseError::DisabledResource, line,
                        "resource is marked disabled by Linux sysfs flags");
        if (count >= 6 || count >= capacity)
            return fail(LinuxResourceParseError::TooManyLines, line, "non-empty resources exceed output capacity or validator limit");
        const bool isIo = (flags & 0x00000100ULL) != 0;
        const bool isMemory = (flags & 0x00000200ULL) != 0;
        if (isIo && isMemory)
            return fail(LinuxResourceParseError::ConflictingTypeFlags, line, "both IO and MEM flags are set");
        if (!isIo && !isMemory)
            return fail(LinuxResourceParseError::UnsupportedResourceType, line, "resource has neither IO nor MEM flag");
        parsed[count++] = {start, end, isIo ? ResourceKind::IoPort : ResourceKind::Memory,
                           true, (flags & 0x00002000ULL) != 0};
    }
    const auto validation = validatePciResources(parsed, count);
    if (!validation.valid) {
        auto result = fail(LinuxResourceParseError::ValidationFailed, validation.errorIndex + 1,
                           "parsed Linux resource metadata failed validation");
        result.resourceCount = count;
        result.validation = validation;
        result.validation.mappingPermitted = false;
        result.validation.registerWritesPermitted = false;
        return result;
    }
    for (std::size_t i = 0; i < count; ++i) output[i] = parsed[i];
    LinuxResourceParseResult result{};
    result.valid = true;
    result.resourceCount = count;
    result.validation = validation;
    result.message = "Linux resource metadata parsed and validated; no mapping or register access permitted";
    return result;
}
}  // namespace cobalt64::research
