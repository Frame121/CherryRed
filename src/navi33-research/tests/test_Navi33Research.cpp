#include "Navi33BuildConfiguration.hpp"
#include "Navi33Research.hpp"

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

bool hasMessage(const char* message) {
    return message != nullptr && message[0] != '\0';
}

Navi33DeviceIdentity navi48Identity() {
    return {kAmdVendorId, kNavi48Rx9070XtDeviceId, ArchitectureFamily::Navi48,
            ArchitectureVersionState::Unknown, "Navi 48"};
}

DeviceValidation validateLocalIdentityText() {
    const std::string label = "Navi 33";
    const Navi33DeviceIdentity identity{
        kAmdVendorId, kRx7600DeviceId, ArchitectureFamily::Navi33,
        ArchitectureVersionState::Unknown, label};
    return selectArchitecture(identity);
}

EvidenceValidation validateLocalIpText() {
    const std::string blockName = "GFX";
    const std::string source = "synthetic test source";
    const IpBlockDescriptor block{
        blockName, IpVersion{11, 0}, source, Confidence::Low,
        EvidenceKind::GenericUpstreamSource};
    return validateIpBlockEvidence(block);
}

EvidenceValidation validateLocalFirmwareText() {
    const std::string identifier = "synthetic-firmware-id";
    const FirmwareRequirement firmware{
        identifier, ProvenanceState::IndependentlyVerified,
        LicenseReviewState::HumanReviewed, AvailabilityState::PresentVerified,
        EvidenceKind::SyntheticFixture};
    return validateFirmwareEvidence(firmware);
}

}  // namespace

int main() {
    const Navi33DeviceIdentity target = rx7600ResearchIdentity();
    const DeviceValidation targetSelection = selectArchitecture(target);

    check(target.vendorId == 0x1002 && target.deviceId == 0x7480 &&
              target.architectureLabel == "Navi 33",
          "target PCI identity is represented explicitly");
    check(targetSelection.accepted &&
              targetSelection.route == ArchitectureRoute::Navi33ResearchOnly,
          "RX 7600 identity selects only the Navi 33 research model");
    const DeviceValidation localIdentityText = validateLocalIdentityText();
    check(localIdentityText.accepted &&
              std::string_view(localIdentityText.message) ==
                  "Identity is selected for offline Navi 33 research only; a PCI ID match does not prove board compatibility.",
          "identity validation borrows live caller text and returns independent static diagnostics");

    Navi33DeviceIdentity unknown = target;
    unknown.deviceId = 0xFFFF;
    const DeviceValidation unknownSelection = selectArchitecture(unknown);
    check(!unknownSelection.accepted &&
              unknownSelection.rejectionReason == RejectionReason::UnknownDevice &&
              hasMessage(unknownSelection.message),
          "unknown device IDs are rejected with a reason");
    check(std::string_view(unknownSelection.message) ==
              "The PCI device ID is not recognized by this research model.",
          "validation diagnostics preserve their exact text without owned strings");

    Navi33DeviceIdentity unknownFamily = target;
    unknownFamily.claimedFamily = ArchitectureFamily::Unknown;
    unknownFamily.architectureLabel = {};
    const DeviceValidation unknownFamilySelection =
        selectArchitecture(unknownFamily);
    check(!unknownFamilySelection.accepted &&
              unknownFamilySelection.rejectionReason ==
                  RejectionReason::MissingArchitectureLabel &&
              hasMessage(unknownFamilySelection.message),
          "missing architecture labels are rejected descriptively");
    unknownFamily.architectureLabel = "Unknown";
    const DeviceValidation declaredUnknownFamilySelection =
        selectArchitecture(unknownFamily);
    check(!declaredUnknownFamilySelection.accepted &&
              declaredUnknownFamilySelection.rejectionReason ==
                  RejectionReason::UnknownArchitecture &&
              hasMessage(declaredUnknownFamilySelection.message),
          "unknown architecture families are rejected despite a matching PCI ID");
    check(selectArchitecture(
              Navi33DeviceIdentity{kAmdVendorId, kRx7600DeviceId,
                                   ArchitectureFamily::Navi33,
                                   ArchitectureVersionState::Unknown, {}})
                  .rejectionReason == RejectionReason::MissingArchitectureLabel,
          "an empty default string_view is rejected without reading its data");
    Navi33DeviceIdentity malformedLabel = target;
    malformedLabel.architectureLabel = "Navi 48";
    check(selectArchitecture(malformedLabel).rejectionReason ==
              RejectionReason::ArchitectureMismatch,
          "a live but mismatched architecture label fails closed");

    Navi33DeviceIdentity wrongVendor = target;
    wrongVendor.vendorId = 0x1234;
    check(!selectArchitecture(wrongVendor).accepted,
          "non-AMD vendor IDs are rejected");

    const DeviceValidation navi48Selection = selectArchitecture(navi48Identity());
    check(navi48Selection.accepted &&
              navi48Selection.route == ArchitectureRoute::Navi48NoInitialization,
          "Navi 48 is distinguished without an initialization route");

    Navi33DeviceIdentity misroutedTarget = target;
    misroutedTarget.claimedFamily = ArchitectureFamily::Navi48;
    misroutedTarget.architectureLabel = "Navi 48";
    const DeviceValidation misroutedSelection = selectArchitecture(misroutedTarget);
    check(!misroutedSelection.accepted &&
              misroutedSelection.route != ArchitectureRoute::Navi48NoInitialization,
          "PCI ID 1002:7480 can never select Navi 48");
    check(!hardwareInitializationAllowed(targetSelection) &&
              !hardwareInitializationAllowed(navi48Selection),
          "all architecture selections stop before hardware initialization");

    Navi33DeviceIdentity invalidFamily = target;
    invalidFamily.claimedFamily = static_cast<ArchitectureFamily>(99);
    const DeviceValidation invalidFamilySelection =
        selectArchitecture(invalidFamily);
    check(!invalidFamilySelection.accepted &&
              invalidFamilySelection.route == ArchitectureRoute::Rejected &&
              invalidFamilySelection.rejectionReason ==
                  RejectionReason::ArchitectureMismatch &&
              !hardwareInitializationAllowed(invalidFamilySelection),
          "out-of-range architecture families fail closed");

    const Navi33Capabilities capabilities{};
    check(capabilities.physicalIpInventory == CapabilityState::Unknown &&
              capabilities.firmwareRequirements == CapabilityState::Unknown &&
              capabilities.hardwareInitialization == CapabilityState::Unsupported &&
              capabilities.macOSIntegration == CapabilityState::Unsupported,
          "capabilities default to explicit unknown or unsupported states");

    const IpBlockDescriptor unknownIp{"GFX", std::nullopt, "", Confidence::Unknown};
    const EvidenceValidation unknownIpResult = validateIpBlockEvidence(unknownIp);
    check(unknownIpResult.structurallyValid && !unknownIpResult.claimsComplete &&
              unknownIpResult.rejectionReason == RejectionReason::None &&
              !unknownIp.version.has_value(),
          "unknown IP evidence is valid as a record but not presented as complete");

    IpBlockDescriptor untypedIp{"GFX", IpVersion{11, 0},
                                "generic source string", Confidence::Low};
    const EvidenceValidation untypedIpResult =
        validateIpBlockEvidence(untypedIp);
    check(!untypedIpResult.claimsComplete &&
              untypedIpResult.rejectionReason ==
                  RejectionReason::IncompleteIpEvidence,
          "an untyped source string cannot complete an IP evidence claim");

    IpBlockDescriptor incompleteIp{"GFX", IpVersion{0, 0}, "", Confidence::Unknown};
    const EvidenceValidation incompleteIpResult = validateIpBlockEvidence(incompleteIp);
    check(!incompleteIpResult.structurallyValid &&
              !incompleteIpResult.claimsComplete &&
              incompleteIpResult.rejectionReason == RejectionReason::IncompleteIpEvidence &&
              hasMessage(incompleteIpResult.message),
          "incomplete IP evidence fails with a descriptive error");

    const FirmwareRequirement unknownFirmware{};
    const EvidenceValidation firmwareResult = validateFirmwareEvidence(unknownFirmware);
    check(!firmwareEvidenceClaimsComplete(unknownFirmware) &&
              firmwareResult.structurallyValid &&
              !firmwareResult.claimsComplete &&
              firmwareResult.rejectionReason == RejectionReason::IncompleteFirmwareEvidence &&
              hasMessage(firmwareResult.message),
          "unverified firmware cannot pass evidence validation");

    const IpBlockDescriptor evidencedIp{
        "GFX", IpVersion{11, 0},
        "synthetic fixture for the generic-upstream category",
        Confidence::Low, EvidenceKind::GenericUpstreamSource};
    const EvidenceValidation evidencedIpResult =
        validateIpBlockEvidence(evidencedIp);
    check(evidencedIpResult.structurallyValid &&
              evidencedIpResult.claimsComplete &&
              std::string_view(evidencedIpResult.message).find(
                  "requires independent review") != std::string_view::npos,
          "generic upstream IP metadata remains a claim requiring independent review");
    const EvidenceValidation localIpText = validateLocalIpText();
    check(localIpText.structurallyValid && localIpText.claimsComplete &&
              std::string_view(localIpText.message).find(
                  "requires independent review") != std::string_view::npos,
          "IP evidence validation does not retain caller-owned text");
    const IpBlockDescriptor emptyIpName{
        {}, IpVersion{11, 0}, "source", Confidence::Low,
        EvidenceKind::GenericUpstreamSource};
    check(validateIpBlockEvidence(emptyIpName).rejectionReason ==
              RejectionReason::IncompleteIpEvidence,
          "an empty IP block name is rejected");
    const IpBlockDescriptor emptyIpSource{
        "GFX", IpVersion{11, 0}, {}, Confidence::Low,
        EvidenceKind::GenericUpstreamSource};
    check(validateIpBlockEvidence(emptyIpSource).rejectionReason ==
              RejectionReason::IncompleteIpEvidence,
          "an empty IP evidence source is rejected");
    check(!evidenceKindClaimsBoardAttribution(evidencedIp.evidenceKind) &&
              !evidenceKindClaimsBoardAttribution(
                  EvidenceKind::SyntheticFixture) &&
              !evidenceKindClaimsBoardAttribution(
                  EvidenceKind::MacOSInterfaceReview) &&
              evidenceKindClaimsBoardAttribution(
                  EvidenceKind::BoardAttributedArtifact) &&
              evidenceKindClaimsBoardAttribution(
                  EvidenceKind::TargetHardwareTest),
          "evidence categories distinguish generic, synthetic, interface, board, and target claims");
    IpBlockDescriptor invalidEvidenceKind{
        "GFX", IpVersion{11, 0}, "test source", Confidence::Low,
        static_cast<EvidenceKind>(99)};
    check(!validateIpBlockEvidence(invalidEvidenceKind).claimsComplete &&
              !evidenceKindClaimsBoardAttribution(
                  static_cast<EvidenceKind>(99)),
          "unknown evidence-kind values fail closed");

    Navi33DeviceIdentity unsupported = target;
    unsupported.versionState = ArchitectureVersionState::Unsupported;
    const DeviceValidation unsupportedResult = selectArchitecture(unsupported);
    check(!unsupportedResult.accepted &&
              unsupportedResult.rejectionReason ==
                  RejectionReason::UnsupportedArchitectureVersion &&
              hasMessage(unsupportedResult.message),
          "unsupported architecture versions fail closed");

    Navi33DeviceIdentity invalidState = target;
    invalidState.versionState = static_cast<ArchitectureVersionState>(99);
    const DeviceValidation invalidStateResult = selectArchitecture(invalidState);
    check(!invalidStateResult.accepted &&
              invalidStateResult.rejectionReason ==
                  RejectionReason::InvalidArchitectureState &&
              hasMessage(invalidStateResult.message),
          "out-of-range architecture states fail closed");

    IpBlockDescriptor invalidConfidence{"GFX", IpVersion{11, 0}, "synthetic test",
                                        static_cast<Confidence>(99)};
    const EvidenceValidation invalidConfidenceResult =
        validateIpBlockEvidence(invalidConfidence);
    check(!invalidConfidenceResult.structurallyValid &&
              !invalidConfidenceResult.claimsComplete &&
              invalidConfidenceResult.rejectionReason ==
                  RejectionReason::IncompleteIpEvidence &&
              hasMessage(invalidConfidenceResult.message),
          "out-of-range confidence values do not validate IP evidence");

    FirmwareRequirement completeFirmware{
        "synthetic-test-id", ProvenanceState::IndependentlyVerified,
        LicenseReviewState::HumanReviewed, AvailabilityState::PresentVerified,
        EvidenceKind::SyntheticFixture};
    const EvidenceValidation completeFirmwareResult =
        validateFirmwareEvidence(completeFirmware);
    check(completeFirmwareResult.structurallyValid &&
              completeFirmwareResult.claimsComplete &&
              firmwareEvidenceClaimsComplete(completeFirmware) &&
              std::string_view(completeFirmwareResult.message).find(
                  "does not authenticate them") != std::string_view::npos,
          "firmware state flags are claims, not authentication or loading authorization");
    const EvidenceValidation localFirmwareText = validateLocalFirmwareText();
    check(localFirmwareText.structurallyValid &&
              localFirmwareText.claimsComplete &&
              std::string_view(localFirmwareText.message).find(
                  "does not authenticate them") != std::string_view::npos,
          "firmware validation does not retain caller-owned identifiers");
    const FirmwareRequirement emptyFirmwareIdentifier{};
    const EvidenceValidation emptyFirmwareResult =
        validateFirmwareEvidence(emptyFirmwareIdentifier);
    check(emptyFirmwareResult.structurallyValid &&
              !emptyFirmwareResult.claimsComplete &&
              emptyFirmwareResult.rejectionReason ==
                  RejectionReason::IncompleteFirmwareEvidence,
          "an empty firmware identifier remains incomplete without unsafe access");

    const FirmwareRequirement partialFirmware{
        "synthetic-test-id", ProvenanceState::IndependentlyVerified,
        LicenseReviewState::HumanReviewed, AvailabilityState::PresentUnverified,
        EvidenceKind::SyntheticFixture};
    check(!firmwareEvidenceClaimsComplete(partialFirmware) &&
              !validateFirmwareEvidence(partialFirmware).claimsComplete,
          "one unverified firmware field prevents evidence readiness");

    const FirmwareRequirement invalidFirmware{
        "synthetic-test-id", static_cast<ProvenanceState>(99),
        static_cast<LicenseReviewState>(99), static_cast<AvailabilityState>(99),
        EvidenceKind::SyntheticFixture};
    const EvidenceValidation invalidFirmwareResult =
        validateFirmwareEvidence(invalidFirmware);
    check(!firmwareEvidenceClaimsComplete(invalidFirmware) &&
              invalidFirmwareResult.structurallyValid &&
              !invalidFirmwareResult.claimsComplete &&
              invalidFirmwareResult.rejectionReason ==
                  RejectionReason::IncompleteFirmwareEvidence,
          "out-of-range firmware evidence states do not become complete claims");

    FirmwareRequirement untypedFirmware{
        "firmware-id", ProvenanceState::IndependentlyVerified,
        LicenseReviewState::HumanReviewed, AvailabilityState::PresentVerified};
    check(!firmwareEvidenceClaimsComplete(untypedFirmware) &&
              !validateFirmwareEvidence(untypedFirmware).claimsComplete,
          "firmware completeness requires an explicit evidence category");
    FirmwareRequirement invalidFirmwareKind{
        "firmware-id", ProvenanceState::IndependentlyVerified,
        LicenseReviewState::HumanReviewed, AvailabilityState::PresentVerified,
        static_cast<EvidenceKind>(99)};
    check(!firmwareEvidenceClaimsComplete(invalidFirmwareKind) &&
              !validateFirmwareEvidence(invalidFirmwareKind).claimsComplete,
          "unknown firmware evidence-kind values fail closed");

    if (failures != 0) {
        std::fprintf(stderr, "%d of %d Navi 33 research tests failed\n",
                     failures, testsRun);
        return 1;
    }
    std::printf("Navi 33 research tests: %d passed, 0 failed\n", testsRun);
    return 0;
}
