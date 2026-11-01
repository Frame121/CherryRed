#ifndef COBALT64_NAVI33_LINUX_RESOURCE_PARSER_HPP
#define COBALT64_NAVI33_LINUX_RESOURCE_PARSER_HPP

#include "Navi33ResourceSurvey.hpp"

#include <cstddef>

namespace cobalt64::research {

enum class LinuxResourceParseError : unsigned char {
    None = 0, NullInput, MalformedLine, InvalidHex, TooManyLines,
    ConflictingTypeFlags, UnsupportedResourceType, DisabledResource, ValidationFailed
};

struct LinuxResourceParseResult {
    bool valid = false;
    std::size_t resourceCount = 0;
    LinuxResourceParseError error = LinuxResourceParseError::None;
    std::size_t errorLine = 0;
    ResourceSurveyResult validation{};
    const char* message = "";
};

// Parses Linux sysfs "resource" text only. Input is a NUL-terminated buffer.
// Empty (all-zero) entries are ignored; non-empty lines are decoded using
// Linux IORESOURCE_IO (0x100), IORESOURCE_MEM (0x200), and PREFETCH (0x2000).
// It never opens files, maps BARs, reads registers, or authorizes writes.
LinuxResourceParseResult parseLinuxPciResourceText(
    const char* text, PciResource* output, std::size_t outputCapacity) noexcept;

const char* linuxResourceParseErrorName(LinuxResourceParseError error) noexcept;

}  // namespace cobalt64::research
#endif
