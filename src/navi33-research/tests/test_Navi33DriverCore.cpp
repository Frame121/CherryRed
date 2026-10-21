#include "Navi33BuildConfiguration.hpp"
#include "Navi33DriverCore.hpp"

#include <cstdio>
#include <string>
#include <string_view>

using namespace cobalt64::research;

namespace {

int failures = 0;
int testsRun = 0;

void check(bool condition, const char* name) {
    ++testsRun;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}

class MockNavi33Platform final : public Navi33Platform {
public:
    bool prepare() noexcept override {
        ++prepareCalls;
        return prepareSucceeds;
    }

    bool start() noexcept override {
        ++startCalls;
        return startSucceeds;
    }

    void stop() noexcept override {
        ++stopCalls;
    }

    bool prepareSucceeds = true;
    bool startSucceeds = true;
    int prepareCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
};

Navi33DeviceIdentity rx7600() {
    return rx7600ResearchIdentity();
}

void testSuccessfulLifecycleAndCleanup() {
    MockNavi33Platform platform;
    Navi33DriverCore core(platform);

    const Navi33LifecycleResult identified = core.identify(rx7600());
    check(identified.succeeded &&
              identified.state == Navi33DriverState::Identified &&
              core.selection().route == ArchitectureRoute::Navi33ResearchOnly,
          "RX 7600 identity enters only the research lifecycle");
    check(platform.prepareCalls == 0 && platform.startCalls == 0 &&
              platform.stopCalls == 0,
          "identity detection invokes no platform or hardware callback");

    const Navi33LifecycleResult prepared = core.prepare();
    check(prepared.succeeded && prepared.state == Navi33DriverState::Prepared &&
              platform.prepareCalls == 1,
          "identified device can be prepared through the platform boundary");

    const Navi33LifecycleResult started = core.start();
    check(started.succeeded && started.state == Navi33DriverState::Running &&
              platform.startCalls == 1,
          "prepared lifecycle can enter Running without claiming hardware support");

    const Navi33LifecycleResult stopped = core.stop();
    check(stopped.succeeded && stopped.state == Navi33DriverState::Stopped &&
              platform.stopCalls == 1,
          "stop releases platform resources exactly once");
    check(core.stop().error == Navi33LifecycleError::InvalidTransition &&
              platform.stopCalls == 1,
          "repeated stop is rejected without repeating cleanup");
}

void testIdentifyDoesNotRetainBorrowedIdentityText() {
    MockNavi33Platform platform;
    Navi33DriverCore core(platform);
    Navi33LifecycleResult identified;
    {
        const std::string label = "Navi 33";
        const Navi33DeviceIdentity identity{
            kAmdVendorId, kRx7600DeviceId, ArchitectureFamily::Navi33,
            ArchitectureVersionState::Unknown, label};
        identified = core.identify(identity);
    }

    check(identified.succeeded &&
              core.selection().route == ArchitectureRoute::Navi33ResearchOnly &&
              std::string_view(core.selection().message).find(
                  "offline Navi 33 research only") != std::string_view::npos,
          "identify retains only its static selection result after borrowed text expires");
    check(core.prepare().succeeded && core.stop().succeeded &&
              platform.prepareCalls == 1 && platform.stopCalls == 1,
          "lifecycle remains valid after caller-owned identity text expires");
}

void testRejectedIdentitiesNeverCallPlatform() {
    MockNavi33Platform wrongVendorPlatform;
    Navi33DriverCore wrongVendorCore(wrongVendorPlatform);
    Navi33DeviceIdentity wrongVendor = rx7600();
    wrongVendor.vendorId = 0x1234;
    const Navi33LifecycleResult vendorResult =
        wrongVendorCore.identify(wrongVendor);
    check(!vendorResult.succeeded &&
              vendorResult.state == Navi33DriverState::Failed &&
              vendorResult.error == Navi33LifecycleError::IdentityRejected,
          "wrong vendor fails identification deterministically");
    check(wrongVendorPlatform.prepareCalls == 0 &&
              wrongVendorPlatform.startCalls == 0 &&
              wrongVendorPlatform.stopCalls == 0,
          "wrong vendor invokes no platform callback");

    MockNavi33Platform wrongDevicePlatform;
    Navi33DriverCore wrongDeviceCore(wrongDevicePlatform);
    Navi33DeviceIdentity wrongDevice = rx7600();
    wrongDevice.deviceId = 0x7550;
    wrongDevice.claimedFamily = ArchitectureFamily::Navi48;
    wrongDevice.architectureLabel = "Navi 48";
    const Navi33LifecycleResult deviceResult =
        wrongDeviceCore.identify(wrongDevice);
    check(!deviceResult.succeeded &&
              deviceResult.state == Navi33DriverState::Failed &&
              deviceResult.error == Navi33LifecycleError::IdentityRejected,
          "Navi48 identity is rejected by the Navi33 core");
    check(wrongDevicePlatform.prepareCalls == 0 &&
              wrongDevicePlatform.startCalls == 0 &&
              wrongDevicePlatform.stopCalls == 0,
          "Navi48 identity cannot enter any Navi33 platform operation");
}

void testInvalidAndRepeatedTransitions() {
    MockNavi33Platform platform;
    Navi33DriverCore core(platform);
    const Navi33LifecycleResult invalidPrepare = core.prepare();
    check(invalidPrepare.error == Navi33LifecycleError::InvalidTransition &&
              std::string_view(invalidPrepare.message) ==
                  "Invalid lifecycle transition: prepare" &&
              core.start().error == Navi33LifecycleError::InvalidTransition,
          "prepare and start are rejected before identification");
    check(platform.prepareCalls == 0 && platform.startCalls == 0,
          "invalid transitions do not invoke platform callbacks");

    check(core.identify(rx7600()).succeeded, "initial identification succeeds");
    check(core.identify(rx7600()).error ==
              Navi33LifecycleError::InvalidTransition,
          "repeated identification is rejected");
    check(core.prepare().succeeded, "preparation succeeds once");
    check(core.prepare().error == Navi33LifecycleError::InvalidTransition &&
              platform.prepareCalls == 1,
          "repeated preparation is rejected without another callback");
    check(core.start().succeeded, "start succeeds once");
    check(core.start().error == Navi33LifecycleError::InvalidTransition &&
              platform.startCalls == 1,
          "repeated start is rejected without another callback");
}

void testPlatformFailuresCleanUpAndFailClosed() {
    MockNavi33Platform prepareFailurePlatform;
    prepareFailurePlatform.prepareSucceeds = false;
    Navi33DriverCore prepareFailureCore(prepareFailurePlatform);
    check(prepareFailureCore.identify(rx7600()).succeeded,
          "identity is accepted before prepare failure test");
    const Navi33LifecycleResult prepareFailure =
        prepareFailureCore.prepare();
    check(!prepareFailure.succeeded &&
              prepareFailure.state == Navi33DriverState::Failed &&
              prepareFailure.error == Navi33LifecycleError::PlatformFailure &&
              prepareFailurePlatform.stopCalls == 1,
          "prepare failure requests cleanup and enters Failed");
    check(prepareFailureCore.prepare().error ==
                  Navi33LifecycleError::InvalidTransition &&
              prepareFailurePlatform.prepareCalls == 1 &&
              prepareFailurePlatform.stopCalls == 1,
          "repeated prepare failure cannot rerun callbacks or cleanup");
    check(prepareFailureCore.stop().succeeded &&
              prepareFailurePlatform.stopCalls == 1,
          "stopping after prepare failure does not double-clean resources");

    MockNavi33Platform startFailurePlatform;
    startFailurePlatform.startSucceeds = false;
    Navi33DriverCore startFailureCore(startFailurePlatform);
    check(startFailureCore.identify(rx7600()).succeeded &&
              startFailureCore.prepare().succeeded,
          "lifecycle reaches Prepared before start failure test");
    const Navi33LifecycleResult startFailure = startFailureCore.start();
    check(!startFailure.succeeded &&
              startFailure.state == Navi33DriverState::Failed &&
              startFailure.error == Navi33LifecycleError::PlatformFailure &&
              startFailurePlatform.stopCalls == 1,
          "start failure requests cleanup and enters Failed");
    check(startFailureCore.start().error ==
                  Navi33LifecycleError::InvalidTransition &&
              startFailurePlatform.startCalls == 1 &&
              startFailurePlatform.stopCalls == 1,
          "repeated start failure cannot rerun callbacks or cleanup");
    check(startFailureCore.stop().succeeded &&
              startFailurePlatform.stopCalls == 1,
          "stopping after start failure does not double-clean resources");
}

void testStoppedAndFailedCoresCannotRestart() {
    MockNavi33Platform platform;
    Navi33DriverCore core(platform);
    core.identify(rx7600());
    core.stop();
    check(core.identify(rx7600()).error ==
              Navi33LifecycleError::InvalidTransition &&
              core.prepare().error == Navi33LifecycleError::InvalidTransition,
          "stopped lifecycle cannot be reinitialized");

    MockNavi33Platform failedPlatform;
    failedPlatform.prepareSucceeds = false;
    Navi33DriverCore failedCore(failedPlatform);
    failedCore.identify(rx7600());
    failedCore.prepare();
    check(failedCore.start().error == Navi33LifecycleError::InvalidTransition,
          "failed lifecycle cannot proceed to start");
}

void testDestructorCleansUpAnUnstoppedBackend() {
    MockNavi33Platform platform;
    {
        Navi33DriverCore core(platform);
        core.identify(rx7600());
        core.prepare();
        core.start();
        check(platform.stopCalls == 0,
              "running lifecycle has not cleaned up before destruction");
    }
    check(platform.stopCalls == 1,
          "destroying a running lifecycle releases platform resources");
}

}  // namespace

int main() {
    testSuccessfulLifecycleAndCleanup();
    testIdentifyDoesNotRetainBorrowedIdentityText();
    testRejectedIdentitiesNeverCallPlatform();
    testInvalidAndRepeatedTransitions();
    testPlatformFailuresCleanUpAndFailClosed();
    testStoppedAndFailedCoresCannotRestart();
    testDestructorCleansUpAnUnstoppedBackend();

    if (failures != 0) {
        std::fprintf(stderr, "%d of %d Navi 33 lifecycle tests failed\n",
                     failures, testsRun);
        return 1;
    }
    std::printf("Navi 33 lifecycle tests: %d passed, 0 failed\n", testsRun);
    return 0;
}
