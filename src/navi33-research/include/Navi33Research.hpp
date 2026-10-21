#ifndef COBALT64_NAVI33_RESEARCH_HPP
#define COBALT64_NAVI33_RESEARCH_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace cobalt64::research {

inline constexpr std::uint16_t kAmdVendorId = 0x1002;
inline constexpr std::uint16_t kRx7600DeviceId = 0x7480;
inline constexpr std::uint16_t kNavi48Rx9070XtDeviceId = 0x7550;
inline constexpr std::uint16_t kNavi48R9700DeviceId = 0x7551;

enum class ArchitectureFamily {
    Unknown,
    Navi33,
    Navi48,
};

enum class ArchitectureVersionState {
    Unknown,
    ResearchModelOnly,
    Unsupported,
};

// Text fields are borrowed and read synchronously; no input view is retained.
// Backing storage must remain valid until the called function returns.
struct Navi33DeviceIdentity {
    std::uint16_t vendorId = 0;
    std::uint16_t deviceId = 0;
    ArchitectureFamily claimedFamily = ArchitectureFamily::Unknown;
    ArchitectureVersionState versionState = ArchitectureVersionState::Unknown;
    std::string_view architectureLabel;
};

enum class CapabilityState {
    Unknown,
    Unsupported,
};

struct Navi33Capabilities {
    CapabilityState physicalIpInventory = CapabilityState::Unknown;
    CapabilityState firmwareRequirements = CapabilityState::Unknown;
    CapabilityState hardwareInitialization = CapabilityState::Unsupported;
    CapabilityState macOSIntegration = CapabilityState::Unsupported;
};

enum class Confidence {
    Unknown,
    Low,
    Medium,
    High,
};

enum class EvidenceKind {
    Unknown,
    SyntheticFixture,
    GenericUpstreamSource,
    BoardAttributedArtifact,
    MacOSInterfaceReview,
    TargetHardwareTest,
};

struct IpVersion {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
};

struct IpBlockDescriptor {
    std::string_view blockName;
    std::optional<IpVersion> version;
    std::string_view evidenceSource;
    Confidence confidence = Confidence::Unknown;
    EvidenceKind evidenceKind = EvidenceKind::Unknown;
};

enum class ProvenanceState {
    Unknown,
    DocumentedOnly,
    IndependentlyVerified,
};

enum class LicenseReviewState {
    Unreviewed,
    HumanReviewRequired,
    HumanReviewed,
};

enum class AvailabilityState {
    Unknown,
    NotAvailable,
    PresentUnverified,
    PresentVerified,
};

struct FirmwareRequirement {
    std::string_view identifier;
    ProvenanceState provenance = ProvenanceState::Unknown;
    LicenseReviewState licenseReview = LicenseReviewState::Unreviewed;
    AvailabilityState availability = AvailabilityState::Unknown;
    EvidenceKind evidenceKind = EvidenceKind::Unknown;
};

enum class ArchitectureRoute {
    Rejected,
    Navi33ResearchOnly,
    Navi48NoInitialization,
};

enum class RejectionReason {
    None,
    InvalidVendor,
    UnknownDevice,
    UnknownArchitecture,
    ArchitectureMismatch,
    UnsupportedArchitectureVersion,
    InvalidArchitectureState,
    MissingArchitectureLabel,
    IncompleteIpEvidence,
    IncompleteFirmwareEvidence,
};

struct DeviceValidation {
    bool accepted = false;
    ArchitectureRoute route = ArchitectureRoute::Rejected;
    RejectionReason rejectionReason = RejectionReason::None;
    // API-produced diagnostics point to static text.
    const char* message = "";
};

struct EvidenceValidation {
    bool structurallyValid = false;
    bool claimsComplete = false;
    RejectionReason rejectionReason = RejectionReason::None;
    const char* message = "";
};

enum class DriverStage {
    EvidenceCollection,
    IdentityValidation,
    ArchitectureSelection,
    OfflineUnitTesting,
    IntegrationPlanning,
};

Navi33DeviceIdentity rx7600ResearchIdentity();
DeviceValidation selectArchitecture(const Navi33DeviceIdentity& identity);
EvidenceValidation validateIpBlockEvidence(const IpBlockDescriptor& block);
EvidenceValidation validateFirmwareEvidence(const FirmwareRequirement& firmware);
bool firmwareEvidenceClaimsComplete(const FirmwareRequirement& firmware);
bool evidenceKindClaimsBoardAttribution(EvidenceKind kind) noexcept;
bool hardwareInitializationAllowed(const DeviceValidation& selection) noexcept;

}  // namespace cobalt64::research

#endif
