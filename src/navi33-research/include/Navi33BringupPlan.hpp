#ifndef COBALT64_NAVI33_BRINGUP_PLAN_HPP
#define COBALT64_NAVI33_BRINGUP_PLAN_HPP

#include <cstddef>
#include <cstdint>

namespace cobalt64::research {

enum class BringupStage : std::uint8_t {
    Identity = 0,
    ResourceSurvey,
    FirmwareAudit,
    MemoryManagement,
    Interrupts,
    CommandSubmission,
    Display,
    Acceleration,
    Metal,
    Count,
};

enum class GateState : std::uint8_t {
    Unknown = 0,
    EvidenceCollected,
    Reviewed,
    ValidatedOnTarget,
};

struct BringupGate {
    BringupStage stage;
    GateState state;
    bool hardwareWritesPermitted;
};

struct BringupPlanStatus {
    bool valid;
    bool canAdvance;
    bool hardwareWritesPermitted;
    BringupStage firstBlockedStage;
    const char* reason;
};

// This is a fail-closed planning model, not a hardware driver. A stage cannot
// advance until all earlier stages have target-appropriate evidence. Hardware
// writes remain disabled in this model until a future, separately reviewed
// implementation replaces this policy.
BringupPlanStatus validateBringupPlan(const BringupGate* gates,
                                      std::size_t count) noexcept;
const char* bringupStageName(BringupStage stage) noexcept;

}  // namespace cobalt64::research

#endif
