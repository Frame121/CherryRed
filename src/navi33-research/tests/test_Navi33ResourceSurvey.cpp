#include "Navi33ResourceSurvey.hpp"
#include <cassert>
#include <cstdint>
#include <limits>

using namespace cobalt64::research;

static PciResource mem(std::uint64_t s, std::uint64_t e, bool enabled = true) {
    return {s, e, ResourceKind::Memory, enabled, false};
}
int main() {
    auto r = validatePciResources(nullptr, 0);
    assert(r.valid && r.resourceCount == 0);
    assert(!r.mappingPermitted && !r.registerWritesPermitted);

    PciResource good[] = {mem(0x1000, 0x1fff), {0x2000, 0x20ff, ResourceKind::IoPort, true, false}};
    r = validatePciResources(good, 2);
    assert(r.valid && r.memoryCount == 1 && r.ioPortCount == 1);

    r = validatePciResources(nullptr, 1);
    assert(!r.valid && r.error == ResourceSurveyError::NullInput);

    PciResource seven[7] = {};
    r = validatePciResources(seven, 7);
    assert(!r.valid && r.error == ResourceSurveyError::TooManyResources);

    PciResource disabled[] = {mem(1, 2, false)};
    r = validatePciResources(disabled, 1);
    assert(!r.valid && r.error == ResourceSurveyError::DisabledResource);

    PciResource unknown[] = {{1, 2, ResourceKind::Unknown, true, false}};
    r = validatePciResources(unknown, 1);
    assert(!r.valid && r.error == ResourceSurveyError::UnknownKind);

    PciResource reversed[] = {mem(3, 2)};
    r = validatePciResources(reversed, 1);
    assert(!r.valid && r.error == ResourceSurveyError::EmptyRange);

    PciResource full[] = {mem(0, std::numeric_limits<std::uint64_t>::max())};
    r = validatePciResources(full, 1);
    assert(!r.valid && r.error == ResourceSurveyError::AddressOverflow);

    PciResource adjacent[] = {mem(0x1000, 0x1fff), mem(0x2000, 0x2fff)};
    assert(validatePciResources(adjacent, 2).valid);

    PciResource overlap[] = {mem(0x1000, 0x2000), mem(0x2000, 0x2fff)};
    r = validatePciResources(overlap, 2);
    assert(!r.valid && r.error == ResourceSurveyError::OverlappingRanges);

    assert(!r.mappingPermitted && !r.registerWritesPermitted);
    assert(resourceSurveyErrorName(ResourceSurveyError::None)[0] == 'N');
    return 0;
}
