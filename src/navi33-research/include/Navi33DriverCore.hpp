#ifndef COBALT64_NAVI33_DRIVER_CORE_HPP
#define COBALT64_NAVI33_DRIVER_CORE_HPP

#include "Navi33Research.hpp"

namespace cobalt64::research {

enum class Navi33DriverState {
    Uninitialized,
    Identified,
    Prepared,
    Running,
    Failed,
    Stopped,
};

enum class Navi33LifecycleError {
    None,
    InvalidTransition,
    IdentityRejected,
    PlatformFailure,
};

struct Navi33LifecycleResult {
    bool succeeded = false;
    Navi33DriverState state = Navi33DriverState::Uninitialized;
    Navi33LifecycleError error = Navi33LifecycleError::None;
    const char* message = "";
};

class Navi33Platform {
public:
    virtual ~Navi33Platform() = default;
    virtual bool prepare() noexcept = 0;
    virtual bool start() noexcept = 0;
    virtual void stop() noexcept = 0;
};

class Navi33DriverCore {
public:
    explicit Navi33DriverCore(Navi33Platform& platform) noexcept;
    ~Navi33DriverCore() noexcept;

    Navi33LifecycleResult identify(const Navi33DeviceIdentity& identity);
    Navi33LifecycleResult prepare();
    Navi33LifecycleResult start();
    Navi33LifecycleResult stop();

    Navi33DriverState state() const noexcept;
    const DeviceValidation& selection() const noexcept;

private:
    Navi33LifecycleResult rejectTransition(const char* message) const;
    Navi33LifecycleResult fail(Navi33LifecycleError error, const char* message);
    Navi33LifecycleResult succeed(const char* message) const;

    Navi33Platform& platform_;
    Navi33DriverState state_ = Navi33DriverState::Uninitialized;
    DeviceValidation selection_{};
    bool cleanupRequired_ = false;
};

}  // namespace cobalt64::research

#endif
