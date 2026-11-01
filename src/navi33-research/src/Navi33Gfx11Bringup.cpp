#include "Navi33Gfx11Bringup.hpp"

#include <limits>

namespace cobalt64::research {
namespace {
bool powerOfTwo(std::uint64_t v) noexcept { return v && !(v & (v - 1)); }
bool rangeValid(std::uint64_t base, std::uint64_t bytes) noexcept {
    return bytes != 0 && base <= std::numeric_limits<std::uint64_t>::max() - bytes;
}
bool aligned(std::uint64_t value, std::uint64_t alignment) noexcept {
    return powerOfTwo(alignment) && (value & (alignment - 1)) == 0;
}
}

Gfx11InitState advanceGfx11Init(const Gfx11InitState& s,
                                Gfx11InitStage next,
                                const Gfx11InitEvidence& e) noexcept {
    if (s.stage == Gfx11InitStage::Failed || next == Gfx11InitStage::Detached ||
        next == Gfx11InitStage::Failed ||
        static_cast<unsigned>(next) != static_cast<unsigned>(s.stage) + 1)
        return {s.stage, Gfx11InitError::InvalidTransition, s.transitions};
    Gfx11InitError error = Gfx11InitError::None;
    switch (next) {
        case Gfx11InitStage::IdentityVerified:
            if (!e.exactPciIdentity || !e.vbiosMatched) error = Gfx11InitError::MissingIdentityEvidence;
            break;
        case Gfx11InitStage::FirmwareVerified:
            if (!e.firmwareManifestValidated) error = Gfx11InitError::MissingFirmwareEvidence;
            break;
        case Gfx11InitStage::PspReady:
            if (!e.pspAuthenticated) error = Gfx11InitError::MissingPsp;
            break;
        case Gfx11InitStage::SmuReady:
            if (!e.smuResponding) error = Gfx11InitError::MissingSmu;
            break;
        case Gfx11InitStage::GcReady:
            if (!e.gcIpVerified || !e.mesFirmwareVerified) error = Gfx11InitError::MissingGc;
            break;
        case Gfx11InitStage::SdmaReady:
            if (!e.sdmaIpVerified) error = Gfx11InitError::MissingSdma;
            break;
        case Gfx11InitStage::MemoryReady:
            if (!e.memoryTopologyVerified || !e.gpuvmPolicyVerified) error = Gfx11InitError::MissingMemory;
            break;
        case Gfx11InitStage::QueueReady:
            if (!e.computeQueuePolicyVerified) error = Gfx11InitError::MissingQueue;
            break;
        case Gfx11InitStage::ComputeReady:
            if (!e.hardwareComputeVerified) error = Gfx11InitError::MissingQueue;
            break;
        default: error = Gfx11InitError::InvalidTransition; break;
    }
    if (error != Gfx11InitError::None) return {s.stage, error, s.transitions};
    return {next, Gfx11InitError::None, s.transitions + 1};
}

GpuMemoryResult validateGpuMemoryRegion(const GpuMemoryRegion& r) noexcept {
    if (r.bytes == 0 || r.alignment == 0 || !powerOfTwo(r.alignment))
        return {false, GpuMemoryError::InvalidRegion};
    if (!aligned(r.base, r.alignment) || (r.bytes & (r.alignment - 1)))
        return {false, GpuMemoryError::Misaligned};
    if (!rangeValid(r.base, r.bytes)) return {false, GpuMemoryError::AddressOverflow};
    return {true, GpuMemoryError::None};
}

GpuMemoryResult validateGpuMemoryMapping(const GpuMemoryRegion& r,
                                         const GpuMemoryMapping& m,
                                         std::uint16_t vmidCount) noexcept {
    if (!validateGpuMemoryRegion(r).valid || m.bytes == 0)
        return {false, GpuMemoryError::InvalidMapping};
    if (m.vmid >= vmidCount || vmidCount == 0)
        return {false, GpuMemoryError::InvalidVmid};
    if (!m.read && !m.write) return {false, GpuMemoryError::PermissionsDenied};
    if (!aligned(m.gpuVirtualAddress, r.alignment) ||
        !aligned(m.physicalAddress, r.alignment) ||
        (m.bytes & (r.alignment - 1)))
        return {false, GpuMemoryError::Misaligned};
    if (!rangeValid(m.gpuVirtualAddress, m.bytes) ||
        !rangeValid(m.physicalAddress, m.bytes))
        return {false, GpuMemoryError::AddressOverflow};
    if (m.physicalAddress < r.base ||
        m.physicalAddress + m.bytes > r.base + r.bytes)
        return {false, GpuMemoryError::OutsideRegion};
    return {true, GpuMemoryError::None};
}

Gfx11QueueResult validateGfx11Queue(const Gfx11QueueConfig& c,
                                    const Gfx11QueueCursor& p) noexcept {
    if (c.ringBytes < 256 || !powerOfTwo(c.ringBytes) ||
        c.entryBytes == 0 || c.ringBytes % c.entryBytes != 0 ||
        !powerOfTwo(c.alignment) || c.alignment < c.entryBytes ||
        !aligned(c.ringGpuAddress, c.alignment) || c.vmid >= 16 ||
        c.pipe >= 2 || c.queue >= 2)
        return {false, Gfx11QueueError::InvalidConfig};
    if (p.readPointer >= c.ringBytes || p.writePointer >= c.ringBytes ||
        p.readPointer % c.entryBytes || p.writePointer % c.entryBytes ||
        p.completedSequence > p.lastSequence)
        return {false, Gfx11QueueError::InvalidCursor};
    return {true, Gfx11QueueError::None};
}

Gfx11QueueResult validateGfx11QueueSubmit(const Gfx11QueueConfig& c,
                                          const Gfx11QueueCursor& p,
                                          std::uint32_t entries,
                                          std::uint64_t sequence) noexcept {
    const auto base = validateGfx11Queue(c, p);
    if (!base.valid) return base;
    if (entries == 0 || sequence == 0)
        return {false, Gfx11QueueError::InvalidSubmission};
    if (sequence <= p.lastSequence)
        return {false, Gfx11QueueError::SequenceRegression};
    const std::uint64_t capacity = c.ringBytes / c.entryBytes;
    const std::uint64_t used = p.writePointer >= p.readPointer
        ? (p.writePointer - p.readPointer) / c.entryBytes
        : capacity - (p.readPointer - p.writePointer) / c.entryBytes;
    if (entries >= capacity || used + entries >= capacity)
        return {false, Gfx11QueueError::RingFull};
    return {true, Gfx11QueueError::None};
}

Gfx11QueueResult validateGfx11QueueCompletion(const Gfx11QueueCursor& p,
                                              std::uint64_t completed) noexcept {
    if (completed < p.completedSequence || completed > p.lastSequence)
        return {false, Gfx11QueueError::CompletionAhead};
    return {true, Gfx11QueueError::None};
}

MetalBridgeState advanceMetalBridge(const MetalBridgeState& s,
                                    MetalBridgeStage next,
                                    const MetalBridgeEvidence& e) noexcept {
    if (s.stage == MetalBridgeStage::Failed || next == MetalBridgeStage::Unavailable ||
        next == MetalBridgeStage::Failed ||
        static_cast<unsigned>(next) != static_cast<unsigned>(s.stage) + 1)
        return {s.stage, MetalBridgeError::InvalidTransition};
    MetalBridgeError error = MetalBridgeError::None;
    switch (next) {
        case MetalBridgeStage::KernelServiceReady:
            if (!e.kernelServicePublished) error = MetalBridgeError::MissingKernelService;
            break;
        case MetalBridgeStage::UserClientReady:
            if (!e.userClientValidated) error = MetalBridgeError::MissingUserClient;
            break;
        case MetalBridgeStage::AcceleratorReady:
            if (!e.ioAcceleratorRegistered) error = MetalBridgeError::MissingAccelerator;
            break;
        case MetalBridgeStage::MetalDeviceAdvertised:
            if (!e.hardwareComputeVerified) error = MetalBridgeError::MissingHardwareCompute;
            else if (!e.fenceAndInterruptsVerified) error = MetalBridgeError::MissingFenceSupport;
            break;
        default: error = MetalBridgeError::InvalidTransition; break;
    }
    if (error != MetalBridgeError::None) return {s.stage, error};
    return {next, MetalBridgeError::None};
}
} // namespace cobalt64::research
