#ifndef COBALT64_NAVI33_GFX11_BRINGUP_HPP
#define COBALT64_NAVI33_GFX11_BRINGUP_HPP

#include <cstddef>
#include <cstdint>

namespace cobalt64::research {

enum class Gfx11InitStage : std::uint8_t {
    Detached, IdentityVerified, FirmwareVerified, PspReady, SmuReady,
    GcReady, SdmaReady, MemoryReady, QueueReady, ComputeReady, Failed
};
enum class Gfx11InitError : std::uint8_t {
    None, InvalidTransition, MissingIdentityEvidence, MissingFirmwareEvidence,
    MissingPsp, MissingSmu, MissingGc, MissingSdma, MissingMemory,
    MissingQueue, InvalidFailureTransition
};
struct Gfx11InitEvidence {
    bool exactPciIdentity = false;
    bool vbiosMatched = false;
    bool firmwareManifestValidated = false;
    bool pspAuthenticated = false;
    bool smuResponding = false;
    bool gcIpVerified = false;
    bool sdmaIpVerified = false;
    bool memoryTopologyVerified = false;
    bool gpuvmPolicyVerified = false;
    bool computeQueuePolicyVerified = false;
    bool mesFirmwareVerified = false;
    bool hardwareComputeVerified = false;
};
struct Gfx11InitState {
    Gfx11InitStage stage = Gfx11InitStage::Detached;
    Gfx11InitError error = Gfx11InitError::None;
    std::uint32_t transitions = 0;
};
Gfx11InitState advanceGfx11Init(const Gfx11InitState& state,
                                Gfx11InitStage next,
                                const Gfx11InitEvidence& evidence) noexcept;

struct GpuMemoryRegion {
    std::uint64_t base = 0;
    std::uint64_t bytes = 0;
    std::uint64_t alignment = 0;
};
struct GpuMemoryMapping {
    std::uint64_t gpuVirtualAddress = 0;
    std::uint64_t physicalAddress = 0;
    std::uint64_t bytes = 0;
    std::uint16_t vmid = 0;
    bool read = false;
    bool write = false;
};
enum class GpuMemoryError : std::uint8_t {
    None, InvalidRegion, InvalidMapping, Misaligned, AddressOverflow,
    OutsideRegion, InvalidVmid, PermissionsDenied, Overlap
};
struct GpuMemoryResult { bool valid = false; GpuMemoryError error = GpuMemoryError::None; };
GpuMemoryResult validateGpuMemoryRegion(const GpuMemoryRegion& region) noexcept;
GpuMemoryResult validateGpuMemoryMapping(const GpuMemoryRegion& region,
                                         const GpuMemoryMapping& mapping,
                                         std::uint16_t vmidCount) noexcept;

struct Gfx11QueueConfig {
    std::uint64_t ringGpuAddress = 0;
    std::uint32_t ringBytes = 0;
    std::uint32_t entryBytes = 0;
    std::uint32_t alignment = 0;
    std::uint16_t pipe = 0;
    std::uint16_t queue = 0;
    std::uint16_t vmid = 0;
};
struct Gfx11QueueCursor {
    std::uint32_t readPointer = 0;
    std::uint32_t writePointer = 0;
    std::uint64_t lastSequence = 0;
    std::uint64_t completedSequence = 0;
};
enum class Gfx11QueueError : std::uint8_t {
    None, InvalidConfig, InvalidCursor, RingFull, InvalidSubmission,
    SequenceRegression, CompletionAhead
};
struct Gfx11QueueResult { bool valid = false; Gfx11QueueError error = Gfx11QueueError::None; };
Gfx11QueueResult validateGfx11Queue(const Gfx11QueueConfig& config,
                                    const Gfx11QueueCursor& cursor) noexcept;
Gfx11QueueResult validateGfx11QueueSubmit(const Gfx11QueueConfig& config,
                                          const Gfx11QueueCursor& cursor,
                                          std::uint32_t entries,
                                          std::uint64_t sequence) noexcept;
Gfx11QueueResult validateGfx11QueueCompletion(const Gfx11QueueCursor& cursor,
                                              std::uint64_t completed) noexcept;

enum class MetalBridgeStage : std::uint8_t {
    Unavailable, KernelServiceReady, UserClientReady, AcceleratorReady,
    MetalDeviceAdvertised, Failed
};
enum class MetalBridgeError : std::uint8_t {
    None, InvalidTransition, MissingKernelService, MissingUserClient,
    MissingAccelerator, MissingHardwareCompute, MissingFenceSupport
};
struct MetalBridgeEvidence {
    bool kernelServicePublished = false;
    bool userClientValidated = false;
    bool ioAcceleratorRegistered = false;
    bool hardwareComputeVerified = false;
    bool fenceAndInterruptsVerified = false;
};
struct MetalBridgeState {
    MetalBridgeStage stage = MetalBridgeStage::Unavailable;
    MetalBridgeError error = MetalBridgeError::None;
};
MetalBridgeState advanceMetalBridge(const MetalBridgeState& state,
                                    MetalBridgeStage next,
                                    const MetalBridgeEvidence& evidence) noexcept;

} // namespace cobalt64::research
#endif
