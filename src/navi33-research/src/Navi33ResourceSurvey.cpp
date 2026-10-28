#include "Navi33ResourceSurvey.hpp"

namespace cobalt64::research {
namespace {
ResourceSurveyResult failure(ResourceSurveyError error, std::size_t index,
                             const char* message, std::size_t count) noexcept {
    ResourceSurveyResult result{};
    result.resourceCount = count;
    result.error = error;
    result.errorIndex = index;
    result.message = message;
    result.mappingPermitted = false;
    result.registerWritesPermitted = false;
    return result;
}
}  // namespace

const char* resourceSurveyErrorName(ResourceSurveyError error) noexcept {
    switch (error) {
        case ResourceSurveyError::None: return "None";
        case ResourceSurveyError::NullInput: return "NullInput";
        case ResourceSurveyError::TooManyResources: return "TooManyResources";
        case ResourceSurveyError::EmptyRange: return "EmptyRange";
        case ResourceSurveyError::UnknownKind: return "UnknownKind";
        case ResourceSurveyError::DisabledResource: return "DisabledResource";
        case ResourceSurveyError::AddressOverflow: return "AddressOverflow";
        case ResourceSurveyError::OverlappingRanges: return "OverlappingRanges";
    }
    return "UnknownError";
}

ResourceSurveyResult validatePciResources(const PciResource* resources,
                                          std::size_t count) noexcept {
    constexpr std::size_t kMaxResources = 6;
    if (count != 0 && resources == nullptr)
        return failure(ResourceSurveyError::NullInput, 0, "null input with nonzero count", count);
    if (count > kMaxResources)
        return failure(ResourceSurveyError::TooManyResources, kMaxResources, "resource limit exceeded", count);

    ResourceSurveyResult result{};
    result.valid = true;
    result.resourceCount = count;
    result.mappingPermitted = false;
    result.registerWritesPermitted = false;

    for (std::size_t i = 0; i < count; ++i) {
        const auto& r = resources[i];
        if (r.kind != ResourceKind::Memory && r.kind != ResourceKind::IoPort)
            return failure(ResourceSurveyError::UnknownKind, i, "unknown resource kind", count);
        if (!r.enabled)
            return failure(ResourceSurveyError::DisabledResource, i, "resource is disabled", count);
        if (r.endInclusive < r.start)
            return failure(ResourceSurveyError::EmptyRange, i, "end address precedes start address", count);
        const std::uint64_t distance = r.endInclusive - r.start;
        if (distance == UINT64_MAX)
            return failure(ResourceSurveyError::AddressOverflow, i, "inclusive range length overflows uint64", count);
        if (r.kind == ResourceKind::Memory) ++result.memoryCount;
        else ++result.ioPortCount;
    }

    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            const auto& a = resources[i];
            const auto& b = resources[j];
            if (a.start <= b.endInclusive && b.start <= a.endInclusive)
                return failure(ResourceSurveyError::OverlappingRanges, j, "resource ranges overlap", count);
        }
    }

    result.message = "resource metadata valid; no mapping or register access permitted";
    return result;
}
}  // namespace cobalt64::research
