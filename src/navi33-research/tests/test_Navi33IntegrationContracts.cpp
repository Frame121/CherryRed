#include "Navi33BuildConfiguration.hpp"
#include "Navi33IntegrationContracts.hpp"

#include <cstdint>

#include <cstdio>
using namespace cobalt64::research;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
void readinessTests() {
    IntegrationReadiness r{};
    check(validateFullGraphicsReadiness(r).error ==
          ReadinessError::MissingPciTransport, "empty readiness blocks at PCI");
    r.pciTransport = IntegrationState::HardwareVerified;
    r.firmware = IntegrationState::HardwareVerified;
    r.memory = IntegrationState::HardwareVerified;
    r.queueSubmission = IntegrationState::HardwareVerified;
    r.ioAccelerator = IntegrationState::HardwareVerified;
    r.metal = IntegrationState::HardwareVerified;
    r.display = IntegrationState::HostModelOnly;
    check(validateFullGraphicsReadiness(r).error ==
          ReadinessError::MissingDisplay,
          "host display model cannot satisfy hardware readiness");
    r.display = IntegrationState::HardwareVerified;
    check(validateFullGraphicsReadiness(r).ready,
          "all hardware-verified layers pass strict gate");
    r.metal = IntegrationState::ImplementedUnverified;
    check(validateFullGraphicsReadiness(r).error ==
          ReadinessError::MissingMetal,
          "implemented but unverified Metal is rejected");
}
void displayTests() {
    auto layout = calculateLinearDisplayLayout({1920,1080,60000,32}, 256);
    check(layout.valid && layout.layout.rowBytes == 7680 &&
          layout.layout.frameBytes == 8294400,
          "1920x1080x32 aligned framebuffer layout");
    layout = calculateLinearDisplayLayout({1920,1080,60000,24}, 256);
    check(layout.valid && layout.layout.rowBytes == 5888,
          "24-bit row pitch is aligned");
    check(calculateLinearDisplayLayout({0,1080,60000,32},256).error ==
          DisplayContractError::ZeroDimension, "zero width rejected");
    check(calculateLinearDisplayLayout({20000,1080,60000,32},256).error ==
          DisplayContractError::DimensionTooLarge, "oversized mode rejected");
    check(calculateLinearDisplayLayout({1920,1080,0,32},256).error ==
          DisplayContractError::InvalidRefresh, "zero refresh rejected");
    check(calculateLinearDisplayLayout({1920,1080,60000,16},256).error ==
          DisplayContractError::UnsupportedPixelFormat,
          "unsupported pixel format rejected");
    check(calculateLinearDisplayLayout({1920,1080,60000,32},24).error ==
          DisplayContractError::InvalidPitch, "invalid pitch alignment rejected");
}
void fenceTests() {
    FenceTimelineModel timeline;
    check(timeline.completedValue() == 0 && timeline.isComplete(0),
          "new timeline begins at zero");
    check(timeline.signal(5), "timeline accepts increasing signal");
    check(timeline.isComplete(4) && timeline.isComplete(5) &&
          !timeline.isComplete(6), "timeline completion comparison");
    check(!timeline.signal(5) && !timeline.signal(3),
          "timeline rejects duplicate and decreasing signals");
    check(timeline.signal(9) && timeline.completedValue() == 9,
          "timeline advances monotonically");
}
}
int main() {
    readinessTests(); displayTests(); fenceTests();
    if (failures) {
        std::fprintf(stderr, "%d/%d integration contract checks failed\n",
                     failures, checks);
        return 1;
    }
    std::printf("Navi33 integration contract checks: %d passed, 0 failed\n", checks);
    return 0;
}
