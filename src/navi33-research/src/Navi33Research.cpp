#include "Navi33BuildConfiguration.hpp"
#include "Navi33Research.hpp"

namespace cobalt64::research {
namespace {

DeviceValidation rejected(RejectionReason reason, const char* message) {
    return {false, ArchitectureRoute::Rejected, reason, message};
}

bool isNavi48Device(std::uint16_t deviceId) {
    return deviceId == kNavi48Rx9070XtDeviceId ||
           deviceId == kNavi48R9700DeviceId;
}

bool isKnownArchitectureVersionState(ArchitectureVersionState state) {
    return state == ArchitectureVersionState::Unknown ||
           state == ArchitectureVersionState::ResearchModelOnly ||
           state == ArchitectureVersionState::Unsupported;
}

bool isKnownConfidence(Confidence confidence) {
    return confidence == Confidence::Low || confidence == Confidence::Medium ||
           confidence == Confidence::High;
}

bool isKnownEvidenceKind(EvidenceKind kind) {
    return kind == EvidenceKind::SyntheticFixture ||
           kind == EvidenceKind::GenericUpstreamSource ||
           kind == EvidenceKind::BoardAttributedArtifact ||
           kind == EvidenceKind::MacOSInterfaceReview ||
           kind == EvidenceKind::TargetHardwareTest;
}

}  // namespace

Navi33DeviceIdentity rx7600ResearchIdentity() {
    return {kAmdVendorId, kRx7600DeviceId, ArchitectureFamily::Navi33,
            ArchitectureVersionState::Unknown, "Navi 33"};
}

DeviceValidation selectArchitecture(const Navi33DeviceIdentity& identity) {
    if (identity.vendorId != kAmdVendorId) {
        return rejected(RejectionReason::InvalidVendor,
                        "The identity does not name AMD vendor 0x1002.");
    }

    if (identity.architectureLabel.empty()) {
        return rejected(RejectionReason::MissingArchitectureLabel,
                        "An explicit architecture label is required.");
    }

    if (!isKnownArchitectureVersionState(identity.versionState)) {
        return rejected(RejectionReason::InvalidArchitectureState,
                        "The architecture state is invalid; selection fails closed.");
    }

    if (identity.versionState == ArchitectureVersionState::Unsupported) {
        return rejected(RejectionReason::UnsupportedArchitectureVersion,
                        "The declared architecture version is unsupported; selection fails closed.");
    }

    if (identity.deviceId == kRx7600DeviceId) {
        if (identity.claimedFamily == ArchitectureFamily::Unknown) {
            return rejected(RejectionReason::UnknownArchitecture,
                            "PCI ID 1002:7480 does not establish an architecture family; Navi 33 must be explicitly selected for research.");
        }
        if (identity.claimedFamily != ArchitectureFamily::Navi33 ||
            identity.architectureLabel != "Navi 33") {
            return rejected(RejectionReason::ArchitectureMismatch,
                            "PCI ID 1002:7480 is accepted only as a Navi 33 research identity, never as Navi 48.");
        }

        return {true, ArchitectureRoute::Navi33ResearchOnly,
                RejectionReason::None,
                "Identity is selected for offline Navi 33 research only; a PCI ID match does not prove board compatibility."};
    }

    if (isNavi48Device(identity.deviceId)) {
        if (identity.claimedFamily == ArchitectureFamily::Unknown) {
            return rejected(RejectionReason::UnknownArchitecture,
                            "This device ID does not establish an architecture family; Navi 48 must be explicitly declared.");
        }
        if (identity.claimedFamily != ArchitectureFamily::Navi48 ||
            identity.architectureLabel != "Navi 48") {
            return rejected(RejectionReason::ArchitectureMismatch,
                            "The documented Navi 48 device identity conflicts with the declared architecture.");
        }

        return {true, ArchitectureRoute::Navi48NoInitialization,
                RejectionReason::None,
                "Navi 48 is distinguished but has no initialization route in this research module."};
    }

    return rejected(RejectionReason::UnknownDevice,
                    "The PCI device ID is not recognized by this research model.");
}

EvidenceValidation validateIpBlockEvidence(const IpBlockDescriptor& block) {
    if (block.blockName.empty()) {
        return {false, false, RejectionReason::IncompleteIpEvidence,
                "IP block name is required."};
    }

    if (!block.version.has_value()) {
        if (!block.evidenceSource.empty() || block.confidence != Confidence::Unknown ||
            block.evidenceKind != EvidenceKind::Unknown) {
            return {false, false, RejectionReason::IncompleteIpEvidence,
                    "An unknown IP version cannot carry source, confidence, or evidence-kind claims."};
        }
        return {true, false, RejectionReason::None,
                "IP evidence is structurally valid but incomplete; the version remains unknown."};
    }

    if (block.version->major == 0 || block.evidenceSource.empty() ||
        !isKnownConfidence(block.confidence) ||
        !isKnownEvidenceKind(block.evidenceKind)) {
        return {false, false, RejectionReason::IncompleteIpEvidence,
                "A stated IP version requires a source, confidence, and recognized evidence kind."};
    }

    return {true, true, RejectionReason::None,
            "IP version metadata is complete for its declared evidence kind; the claim still requires independent review."};
}

EvidenceValidation validateFirmwareEvidence(const FirmwareRequirement& firmware) {
    if (firmware.identifier.empty()) {
        return {true, false, RejectionReason::IncompleteFirmwareEvidence,
                "Firmware identifier is required; no firmware filename is inferred."};
    }

    if (!firmwareEvidenceClaimsComplete(firmware)) {
        return {true, false, RejectionReason::IncompleteFirmwareEvidence,
                "Firmware evidence claims are incomplete; source kind, provenance, availability, and human license-review fields must be explicitly marked before research review."};
    }

    return {true, true, RejectionReason::None,
            "Firmware metadata claims are marked complete for research review; this does not authenticate them or authorize loading or redistribution."};
}

bool firmwareEvidenceClaimsComplete(const FirmwareRequirement& firmware) {
    return !firmware.identifier.empty() &&
           firmware.provenance == ProvenanceState::IndependentlyVerified &&
           firmware.licenseReview == LicenseReviewState::HumanReviewed &&
           firmware.availability == AvailabilityState::PresentVerified &&
           isKnownEvidenceKind(firmware.evidenceKind);
}

bool evidenceKindClaimsBoardAttribution(EvidenceKind kind) noexcept {
    return kind == EvidenceKind::BoardAttributedArtifact ||
           kind == EvidenceKind::TargetHardwareTest;
}

bool hardwareInitializationAllowed(const DeviceValidation& selection) noexcept {
    (void)selection;
    return false;
}

}  // namespace cobalt64::research
