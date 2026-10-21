#include "Navi33BuildConfiguration.hpp"
#include "Navi33DriverCore.hpp"

namespace cobalt64::research {

Navi33DriverCore::Navi33DriverCore(Navi33Platform& platform) noexcept
    : platform_(platform) {}

Navi33DriverCore::~Navi33DriverCore() noexcept {
    if (cleanupRequired_) {
        platform_.stop();
    }
}

Navi33LifecycleResult Navi33DriverCore::identify(
    const Navi33DeviceIdentity& identity) {
    if (state_ != Navi33DriverState::Uninitialized) {
        return rejectTransition("Invalid lifecycle transition: identify");
    }

    selection_ = selectArchitecture(identity);
    if (!selection_.accepted ||
        selection_.route != ArchitectureRoute::Navi33ResearchOnly) {
        state_ = Navi33DriverState::Failed;
        return {false, state_, Navi33LifecycleError::IdentityRejected,
                selection_.message};
    }

    state_ = Navi33DriverState::Identified;
    return succeed(
        "Identity accepted for research lifecycle only; hardware is not initialized.");
}

Navi33LifecycleResult Navi33DriverCore::prepare() {
    if (state_ != Navi33DriverState::Identified) {
        return rejectTransition("Invalid lifecycle transition: prepare");
    }

    cleanupRequired_ = true;
    if (!platform_.prepare()) {
        platform_.stop();
        cleanupRequired_ = false;
        return fail(Navi33LifecycleError::PlatformFailure,
                    "Platform preparation failed; cleanup was requested.");
    }

    state_ = Navi33DriverState::Prepared;
    return succeed("Platform preparation callback succeeded.");
}

Navi33LifecycleResult Navi33DriverCore::start() {
    if (state_ != Navi33DriverState::Prepared) {
        return rejectTransition("Invalid lifecycle transition: start");
    }

    if (!platform_.start()) {
        platform_.stop();
        cleanupRequired_ = false;
        return fail(Navi33LifecycleError::PlatformFailure,
                    "Platform start callback failed; cleanup was requested.");
    }

    state_ = Navi33DriverState::Running;
    return succeed(
        "Platform start callback succeeded; hardware functionality is not established.");
}

Navi33LifecycleResult Navi33DriverCore::stop() {
    if (state_ == Navi33DriverState::Uninitialized ||
        state_ == Navi33DriverState::Stopped) {
        return rejectTransition("Invalid lifecycle transition: stop");
    }

    if (cleanupRequired_) {
        platform_.stop();
        cleanupRequired_ = false;
    }
    state_ = Navi33DriverState::Stopped;
    return succeed("Lifecycle stopped; no hardware capability is implied.");
}

Navi33DriverState Navi33DriverCore::state() const noexcept {
    return state_;
}

const DeviceValidation& Navi33DriverCore::selection() const noexcept {
    return selection_;
}

Navi33LifecycleResult Navi33DriverCore::rejectTransition(
    const char* message) const {
    return {false, state_, Navi33LifecycleError::InvalidTransition,
            message};
}

Navi33LifecycleResult Navi33DriverCore::fail(Navi33LifecycleError error,
                                             const char* message) {
    state_ = Navi33DriverState::Failed;
    return {false, state_, error, message};
}

Navi33LifecycleResult Navi33DriverCore::succeed(const char* message) const {
    return {true, state_, Navi33LifecycleError::None, message};
}

}  // namespace cobalt64::research
