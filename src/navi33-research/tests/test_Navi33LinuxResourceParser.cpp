#include "Navi33LinuxResourceParser.hpp"
// Tests must keep assert() effective in NDEBUG (Release) builds: NDEBUG must
// be undefined BEFORE including cassert, which reads it while redefining
// assert.
#undef NDEBUG
#include <cassert>
#include <cstdint>
using namespace cobalt64::research;
int main() {
    constexpr const char* sample =
        "0x000000fa00000000 0x000000fbffffffff 0x000000000014220c\n"
        "0x0000000000000000 0x0000000000000000 0x0000000000000000\n"
        "0x000000fc00000000 0x000000fc0fffffff 0x000000000014220c\n"
        "0x0000000000000000 0x0000000000000000 0x0000000000000000\n"
        "0x000000000000f000 0x000000000000f0ff 0x0000000000040101\n"
        "0x00000000fcb00000 0x00000000fcbfffff 0x0000000000040200\n"
        "0x00000000fcc00000 0x00000000fcc1ffff 0x0000000000046200\n";
    PciResource resources[6]{};
    auto r = parseLinuxPciResourceText(sample, resources, 6);
    assert(r.valid && r.resourceCount == 5);
    assert(resources[0].start == UINT64_C(0xfa00000000));
    assert(resources[0].endInclusive == UINT64_C(0xfbffffffff));
    assert(resources[0].kind == ResourceKind::Memory && resources[0].prefetchable);
    assert(resources[2].kind == ResourceKind::IoPort);
    assert(resources[3].kind == ResourceKind::Memory && !resources[3].prefetchable);
    assert(!r.validation.mappingPermitted && !r.validation.registerWritesPermitted);
    assert(!parseLinuxPciResourceText(nullptr, resources, 6).valid);
    assert(!parseLinuxPciResourceText("garbage\n", resources, 6).valid);
    assert(!parseLinuxPciResourceText("0x1 0x2 0x0\n", resources, 6).valid);
    assert(!parseLinuxPciResourceText("0x1 0x2 0x300\n", resources, 6).valid);
    assert(!parseLinuxPciResourceText("0x1 0x2 0x1\n", resources, 6).valid);
    const auto disabled = parseLinuxPciResourceText(
        "0x1000 0x1fff 0x10000200\n", resources, 6);
    assert(!disabled.valid && disabled.error == LinuxResourceParseError::DisabledResource);
    assert(!disabled.validation.mappingPermitted && !disabled.validation.registerWritesPermitted);
    assert(linuxResourceParseErrorName(LinuxResourceParseError::DisabledResource)[0] == 'D');
    assert(linuxResourceParseErrorName(LinuxResourceParseError::None)[0] == 'N');

    // Reject overflow and malformed suffixes without publishing partial output.
    PciResource sentinel[2]{};
    sentinel[0].start = UINT64_C(0x1234);
    const auto overflow = parseLinuxPciResourceText(
        "0xfffffffffffffffff 0xffffffffffffffff 0x200\n", sentinel, 2);
    assert(!overflow.valid && overflow.error == LinuxResourceParseError::InvalidHex);
    assert(sentinel[0].start == UINT64_C(0x1234));

    const auto suffix = parseLinuxPciResourceText(
        "0x1000 0x1fff 0x200 trailing\n", sentinel, 2);
    assert(!suffix.valid && suffix.error == LinuxResourceParseError::MalformedLine);
    assert(sentinel[0].start == UINT64_C(0x1234));

    const auto tooSmall = parseLinuxPciResourceText(
        "0x1000 0x1fff 0x200\n0x3000 0x3fff 0x200\n", sentinel, 1);
    assert(!tooSmall.valid && tooSmall.error == LinuxResourceParseError::TooManyLines);
    assert(sentinel[0].start == UINT64_C(0x1234));

    const auto reversed = parseLinuxPciResourceText(
        "0x2000 0x1000 0x200\n", sentinel, 2);
    assert(!reversed.valid && reversed.error == LinuxResourceParseError::ValidationFailed);
    assert(!reversed.validation.mappingPermitted);
    assert(!reversed.validation.registerWritesPermitted);
    assert(sentinel[0].start == UINT64_C(0x1234));

    assert(!parseLinuxPciResourceText("0x1000 0x1fff 0x200\n", nullptr, 1).valid);
    assert(!parseLinuxPciResourceText("0x1000 0x1fff 0x200\n", nullptr, 0).valid);
    return 0;
}
