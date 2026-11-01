#include "Navi33BuildConfiguration.hpp"
#include "Navi33BringupPlan.hpp"
#include <cstdio>
using namespace cobalt64::research;
namespace {
int failures = 0;
int count = 0;
void check(bool ok, const char* name) { ++count; if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\\n", name); } }
BringupGate base[static_cast<unsigned>(BringupStage::Count)] = {
 {BringupStage::Identity, GateState::Unknown, false},
 {BringupStage::ResourceSurvey, GateState::Unknown, false},
 {BringupStage::FirmwareAudit, GateState::Unknown, false},
 {BringupStage::MemoryManagement, GateState::Unknown, false},
 {BringupStage::Interrupts, GateState::Unknown, false},
 {BringupStage::CommandSubmission, GateState::Unknown, false},
 {BringupStage::Display, GateState::Unknown, false},
 {BringupStage::Acceleration, GateState::Unknown, false},
 {BringupStage::Metal, GateState::Unknown, false},
};
}
int main() {
 auto s = validateBringupPlan(base, sizeof(base)/sizeof(base[0]));
 check(s.valid && !s.canAdvance && !s.hardwareWritesPermitted && s.firstBlockedStage == BringupStage::Identity, "empty evidence blocks at identity");
 BringupGate reordered[9]; for (unsigned i = 0; i < 9; ++i) reordered[i] = base[i]; reordered[0].stage = BringupStage::ResourceSurvey;
 check(!validateBringupPlan(reordered, 9).valid, "duplicate stage fails closed");
 check(!validateBringupPlan(base, 8).valid, "missing gate fails closed");
 BringupGate writes[9]; for (unsigned i = 0; i < 9; ++i) writes[i] = base[i]; writes[0].hardwareWritesPermitted = true;
 check(!validateBringupPlan(writes, 9).valid, "hardware write permission is rejected");
 BringupGate invalid[9]; for (unsigned i = 0; i < 9; ++i) invalid[i] = base[i]; invalid[0].state = static_cast<GateState>(99);
 check(!validateBringupPlan(invalid, 9).valid, "unknown gate state fails closed");
 BringupGate ready[9]; for (unsigned i = 0; i < 9; ++i) { ready[i] = base[i]; ready[i].state = GateState::ValidatedOnTarget; }
 s = validateBringupPlan(ready, 9);
 check(s.valid && s.canAdvance && !s.hardwareWritesPermitted, "target validation never grants write permission");
 check(bringupStageName(BringupStage::Metal)[0] == 'm', "stage names are stable and readable");
 BringupGate unknownStage[9]; for (unsigned i = 0; i < 9; ++i) unknownStage[i] = base[i]; unknownStage[0].stage = static_cast<BringupStage>(99);
 check(!validateBringupPlan(unknownStage, 9).valid && !validateBringupPlan(unknownStage, 9).hardwareWritesPermitted, "unknown stage fails closed");
 BringupGate partial[9]; for (unsigned i = 0; i < 9; ++i) { partial[i] = base[i]; partial[i].state = GateState::ValidatedOnTarget; } partial[2].state = GateState::EvidenceCollected; partial[0].state = GateState::Reviewed;
 s = validateBringupPlan(partial, 9);
 check(s.valid && !s.canAdvance && s.firstBlockedStage == BringupStage::Identity && !s.hardwareWritesPermitted, "earliest incomplete stage blocks even when later stages are validated");
 BringupGate shuffled[9]; for (unsigned i = 0; i < 9; ++i) shuffled[i] = ready[8 - i];
 s = validateBringupPlan(shuffled, 9);
 check(s.valid && s.canAdvance && !s.hardwareWritesPermitted, "gate ordering does not change result and never grants hardware writes");
 std::printf("Navi33 bring-up plan tests: %d, failures: %d\\n", count, failures);
 return failures == 0 ? 0 : 1;
}
