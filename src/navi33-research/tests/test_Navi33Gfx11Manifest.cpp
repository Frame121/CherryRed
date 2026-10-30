#include "Navi33Gfx11Manifest.hpp"

#include <cstdio>

using namespace cobalt64::research;

namespace {
int failures = 0;
int checks = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}

const char* digest = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
const char* commit = "0123456789abcdef0123456789abcdef01234567";

Gfx11FirmwareRecord firmware[] = {
    {Gfx11FirmwareRole::PspSos, "psp_13_0_7_sos.bin", 4096, digest, commit, true, true},
    {Gfx11FirmwareRole::Smu, "smu_13_0_7.bin", 4096, digest, commit, true, true},
    {Gfx11FirmwareRole::GcPfp, "gc_11_0_2_pfp.bin", 4096, digest, commit, true, true},
    {Gfx11FirmwareRole::GcMe, "gc_11_0_2_me.bin", 4096, digest, commit, true, true},
    {Gfx11FirmwareRole::GcMec, "gc_11_0_2_mec.bin", 4096, digest, commit, true, true},
    {Gfx11FirmwareRole::GcRlc, "gc_11_0_2_rlc.bin", 4096, digest, commit, true, true},
    {Gfx11FirmwareRole::Sdma, "sdma_6_0_2.bin", 4096, digest, commit, true, true}
};

Gfx11Manifest validManifest() {
    Gfx11Manifest m{};
    m.target = {0x1002, 0x7480, 0x148c, 0x2421, 0xcf};
    m.evidence = Gfx11Evidence::BoardAttributed;
    m.gc = {11, 0, 2};
    m.psp = {13, 0, 7};
    m.smu = {13, 0, 7};
    m.sdma = {6, 0, 2};
    m.vbiosIdentityMatched = true;
    m.firmware = firmware;
    m.firmwareCount = sizeof(firmware) / sizeof(firmware[0]);
    return m;
}

void testManifestGate() {
    auto m = validManifest();
    const auto initialResult = validateGfx11Manifest(m);
    check(initialResult.valid,
          "complete board-attributed manifest passes review gate");

    m.evidence = Gfx11Evidence::GenericUpstream;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::InsufficientEvidence,
          "generic upstream evidence cannot authorize target manifest");

    m = validManifest();
    m.target.subsystemId ^= 1;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::WrongTarget,
          "wrong subsystem identity is rejected");

    m = validManifest();
    m.gc.major = 12;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::UnsupportedIpFamily,
          "GFX12 IP family is rejected by the GFX11 backend gate");

    m = validManifest();
    m.vbiosIdentityMatched = false;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::MissingVbiosMatch,
          "unmatched VBIOS identity is rejected");

    m = validManifest();
    Gfx11FirmwareRecord wrongFamily[7];
    for (unsigned i = 0; i < 7; ++i) wrongFamily[i] = firmware[i];
    wrongFamily[2].filename = "gc_12_0_0_pfp.bin";
    m.firmware = wrongFamily;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::WrongFirmwarePrefix,
          "Navi48/GFX12 firmware cannot pass GFX11 version gate");

    m = validManifest();
    Gfx11FirmwareRecord unlicensed[7];
    for (unsigned i = 0; i < 7; ++i) unlicensed[i] = firmware[i];
    unlicensed[2].licenseApproved = false;
    m.firmware = unlicensed;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::InvalidFirmwareRecord,
          "unapproved firmware license is rejected");

    m = validManifest();
    Gfx11FirmwareRecord generic[7];
    for (unsigned i = 0; i < 7; ++i) generic[i] = firmware[i];
    generic[2].boardAttributed = false;
    m.firmware = generic;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::InvalidFirmwareRecord,
          "generic firmware provenance is rejected");

    m = validManifest();
    m.firmwareCount = 6;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::MissingRequiredRole,
          "missing mandatory firmware role is rejected");

    m = validManifest();
    Gfx11FirmwareRecord duplicate[7];
    for (unsigned i = 0; i < 7; ++i) duplicate[i] = firmware[i];
    duplicate[6].role = Gfx11FirmwareRole::GcRlc;
    m.firmware = duplicate;
    check(validateGfx11Manifest(m).error == Gfx11ManifestError::DuplicateFirmwareRole,
          "duplicate firmware role is rejected");
}
}

int main() {
    testManifestGate();
    if (failures) {
        std::fprintf(stderr, "%d of %d GFX11 manifest checks failed\n", failures, checks);
        return 1;
    }
    std::printf("Navi33 GFX11 manifest checks: %d passed, 0 failed\n", checks);
    return 0;
}
