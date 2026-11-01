#include "Navi33TargetProfile.hpp"

#include <cstdio>
#include <cstring>

using namespace cobalt64::research;

namespace {
int failures = 0;
void check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}
}

int main() {
    Navi33IpObservation good[] = {
        {11, 0, 11, 0, 2},
        {42, 0, 6, 0, 2},
        {43, 0, 6, 0, 2},
        {12, 0, 4, 0, 4},
        {255, 0, 13, 0, 7},
        {1, 0, 13, 0, 7},
        {271, 0, 3, 2, 1},
        {282, 0, 13, 0, 2},
        {286, 0, 4, 0, 2},
    };
    auto ok = validateNavi33TargetProfile(good, sizeof(good) / sizeof(good[0]));
    check(ok.valid && ok.error == TargetProfileError::None,
          "captured critical numeric HWIDs and versions accepted");
    check(std::strstr(ok.message, "initialization remains disabled") != nullptr,
          "passing profile does not authorize hardware initialization");

    auto nullInput = validateNavi33TargetProfile(nullptr, 0);
    check(!nullInput.valid && nullInput.error == TargetProfileError::NullInput,
          "null input rejected");

    Navi33IpObservation missing[] = {
        {11, 0, 11, 0, 2}, {42, 0, 6, 0, 2}, {43, 0, 6, 0, 2},
        {12, 0, 4, 0, 4}, {255, 0, 13, 0, 7}, {1, 0, 13, 0, 7},
    };
    auto missingResult = validateNavi33TargetProfile(
        missing, sizeof(missing) / sizeof(missing[0]));
    check(!missingResult.valid &&
          missingResult.error == TargetProfileError::MissingRequiredBlock &&
          std::strcmp(missingResult.blockName, "DMU") == 0,
          "missing required numeric HWID rejected");

    Navi33IpObservation mismatch[sizeof(good) / sizeof(good[0])];
    std::memcpy(mismatch, good, sizeof(good));
    mismatch[0].revision = 3;
    auto mismatchResult = validateNavi33TargetProfile(
        mismatch, sizeof(mismatch) / sizeof(mismatch[0]));
    check(!mismatchResult.valid &&
          mismatchResult.error == TargetProfileError::VersionMismatch &&
          std::strcmp(mismatchResult.blockName, "GC") == 0,
          "mismatched GC version rejected");

    Navi33IpObservation duplicate[sizeof(good) / sizeof(good[0]) + 1];
    std::memcpy(duplicate, good, sizeof(good));
    duplicate[sizeof(good) / sizeof(good[0])] = {1, 0, 13, 0, 7};
    auto duplicateResult = validateNavi33TargetProfile(
        duplicate, sizeof(duplicate) / sizeof(duplicate[0]));
    check(!duplicateResult.valid &&
          duplicateResult.error == TargetProfileError::DuplicateRequiredBlock &&
          std::strcmp(duplicateResult.blockName, "MP1") == 0,
          "duplicate required numeric HWID + instance rejected");

    if (failures != 0) {
        std::fprintf(stderr, "Navi33 target profile tests: %d failure(s)\n", failures);
        return 1;
    }
    std::puts("Navi33 target profile tests: PASS (5 scenarios)");
    return 0;
}
