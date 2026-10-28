#ifndef COBALT64_NAVI33_RESOURCE_SURVEY_HPP
#define COBALT64_NAVI33_RESOURCE_SURVEY_HPP

#include <cstddef>
#include <cstdint>

namespace cobalt64::research {

enum class ResourceKind : std::uint8_t { Unknown = 0, Memory, IoPort };

struct PciResource {
    std::uint64_t start = 0;
    std::uint64_t endInclusive = 0;
    ResourceKind kind = ResourceKind::Unknown;
    bool enabled = false;
    bool prefetchable = false;
};

enum class ResourceSurveyError : std::uint8_t {
    None = 0, NullInput, TooManyResources, EmptyRange, UnknownKind,
    DisabledResource, AddressOverflow, OverlappingRanges
};

struct ResourceSurveyResult {
    bool valid = false;
    std::size_t resourceCount = 0;
    std::size_t memoryCount = 0;
    std::size_t ioPortCount = 0;
    ResourceSurveyError error = ResourceSurveyError::None;
    std::size_t errorIndex = 0;
    const char* message = "";
    // This offline model never maps BARs, reads registers, or permits writes.
    bool mappingPermitted = false;
    bool registerWritesPermitted = false;
};

// Validates caller-supplied PCI resource metadata only; it performs no I/O.
ResourceSurveyResult validatePciResources(const PciResource* resources,
                                          std::size_t count) noexcept;
const char* resourceSurveyErrorName(ResourceSurveyError error) noexcept;

}  // namespace cobalt64::research

#endif
