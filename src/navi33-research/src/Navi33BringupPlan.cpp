#include "Navi33BuildConfiguration.hpp"
#include "Navi33BringupPlan.hpp"

namespace cobalt64::research {
namespace {
constexpr std::size_t kStageCount = static_cast<std::size_t>(BringupStage::Count);

bool knownStage(BringupStage stage) noexcept {
    return static_cast<std::size_t>(stage) < kStageCount;
}
bool knownState(GateState state) noexcept {
    return state == GateState::Unknown || state == GateState::EvidenceCollected ||
           state == GateState::Reviewed || state == GateState::ValidatedOnTarget;
}
}

const char* bringupStageName(BringupStage stage) noexcept {
    switch (stage) {
        case BringupStage::Identity: return "identity";
        case BringupStage::ResourceSurvey: return "resource-survey";
        case BringupStage::FirmwareAudit: return "firmware-audit";
        case BringupStage::MemoryManagement: return "memory-management";
        case BringupStage::Interrupts: return "interrupts";
        case BringupStage::CommandSubmission: return "command-submission";
        case BringupStage::Display: return "display";
        case BringupStage::Acceleration: return "acceleration";
        case BringupStage::Metal: return "metal";
        case BringupStage::Count: break;
    }
    return "invalid";
}

BringupPlanStatus validateBringupPlan(const BringupGate* gates,
                                      std::size_t count) noexcept {
    if (gates == nullptr || count != kStageCount) {
        return {false, false, false, BringupStage::Identity,
                "Exactly one gate for every known stage is required."};
    }
    bool seen[kStageCount]{};
    for (std::size_t i = 0; i < count; ++i) {
        if (!knownStage(gates[i].stage) || !knownState(gates[i].state)) {
            return {false, false, false, BringupStage::Identity,
                    "Unknown stage or gate state; fail closed."};
        }
        const auto index = static_cast<std::size_t>(gates[i].stage);
        if (seen[index]) {
            return {false, false, false, gates[i].stage,
                    "Duplicate stage gate; fail closed."};
        }
        seen[index] = true;
        // The plan is intentionally incapable of authorizing hardware writes.
        if (gates[i].hardwareWritesPermitted) {
            return {false, false, false, gates[i].stage,
                    "Hardware writes are forbidden by the research plan."};
        }
    }
    for (std::size_t i = 0; i < kStageCount; ++i) {
        if (!seen[i]) {
            return {false, false, false, static_cast<BringupStage>(i),
                    "A required stage gate is missing."};
        }
    }
    for (std::size_t i = 0; i < kStageCount; ++i) {
        const BringupGate* current = nullptr;
        for (std::size_t j = 0; j < count; ++j) {
            if (static_cast<std::size_t>(gates[j].stage) == i) {
                current = &gates[j];
                break;
            }
        }
        if (current->state != GateState::ValidatedOnTarget) {
            return {true, false, false, current->stage,
                    "Bring-up is blocked until each preceding stage is validated on target hardware."};
        }
    }
    return {true, true, false, BringupStage::Count,
            "All plan stages are marked validated; this model still never authorizes hardware writes."};
}

}  // namespace cobalt64::research
