#include "Navi33DisplayContracts.hpp"
#include <array>
#include <cstdio>
using namespace cobalt64::research;
namespace {
int checks = 0;
int failures = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
void edidTests() {
    std::array<std::uint8_t, kEdidBlockBytes> edid{};
    const std::uint8_t header[8] = {0,255,255,255,255,255,255,0};
    for (unsigned i=0; i<8; ++i) edid[i] = header[i];
    std::uint8_t sum = 0;
    for (std::size_t i=0; i<127; ++i) sum = static_cast<std::uint8_t>(sum + edid[i]);
    edid[127] = static_cast<std::uint8_t>(0u - sum);
    auto result = validateEdidBaseBlock(edid.data(), edid.size());
    check(result.valid && result.hasBaseHeader && result.checksumValid, "valid EDID base block");
    check(!validateEdidBaseBlock(edid.data(), 127).valid, "short EDID rejected");
    edid[10] ^= 1;
    check(!validateEdidBaseBlock(edid.data(), edid.size()).valid, "bad EDID checksum rejected");
    edid[10] ^= 1;
    edid[0] = 1;
    check(!validateEdidBaseBlock(edid.data(), edid.size()).valid, "bad EDID header rejected");
}
void scanoutTests() {
    const ScanoutAperture aperture{0x100000, 0x100000};
    const ScanoutBuffer buffer{0x110000, 0x8000, 256, 128};
    auto result = validateScanoutBuffer(aperture, buffer, DisplayEvidenceState::BoardAttributed);
    check(result.valid, "board-attributed scanout buffer accepted");
    check(validateScanoutBuffer(aperture, buffer, DisplayEvidenceState::GenericUpstream).error == ScanoutError::UnknownEvidence, "generic upstream evidence cannot authorize scanout");
    auto outside = buffer; outside.address = 0x1f0000; outside.length = 0x20000;
    check(validateScanoutBuffer(aperture, outside, DisplayEvidenceState::HardwareVerified).error == ScanoutError::OutsideAperture, "buffer outside aperture rejected");
    auto shortBuffer = buffer; shortBuffer.length = 10;
    check(validateScanoutBuffer(aperture, shortBuffer, DisplayEvidenceState::HardwareVerified).error == ScanoutError::EmptyBuffer, "undersized scanout buffer rejected");
    auto overflow = buffer; overflow.address = UINT64_MAX - 4; overflow.length = 0x8000;
    check(validateScanoutBuffer(aperture, overflow, DisplayEvidenceState::HardwareVerified).error == ScanoutError::ArithmeticOverflow, "address overflow rejected");
}
void flipTests() {
    DisplayFlipTimelineModel flips;
    const ScanoutAperture aperture{0x100000, 0x100000};
    const ScanoutBuffer first{0x110000, 0x8000, 256, 128};
    const ScanoutBuffer second{0x120000, 0x8000, 256, 128};
    check(flips.submit(1, first, aperture, DisplayEvidenceState::BoardAttributed).accepted, "first flip submitted");
    check(flips.hasPendingFlip(), "pending flip visible");
    check(flips.submit(2, second, aperture, DisplayEvidenceState::BoardAttributed).error == FlipError::AlreadyPending, "second pending flip rejected");
    check(flips.completeAtVblank(0).error == FlipError::StaleCompletion, "stale vblank rejected");
    check(flips.completeAtVblank(2).error == FlipError::OutOfOrderCompletion, "out-of-order vblank rejected");
    check(flips.completeAtVblank(1).accepted, "matching vblank completes flip");
    check(!flips.hasPendingFlip() && flips.completedSequence() == 1 && flips.activeAddress() == first.address, "completed flip becomes active");
    check(flips.submit(1, second, aperture, DisplayEvidenceState::BoardAttributed).error == FlipError::InvalidSequence, "replayed sequence rejected");
    check(flips.submit(2, second, aperture, DisplayEvidenceState::BoardAttributed).accepted, "next monotonic flip accepted");
}
}
int main() {
    edidTests(); scanoutTests(); flipTests();
    if (failures) { std::fprintf(stderr, "%d/%d display contract checks failed\n", failures, checks); return 1; }
    std::printf("Navi33 display contract checks: %d passed, 0 failed\n", checks);
    return 0;
}
