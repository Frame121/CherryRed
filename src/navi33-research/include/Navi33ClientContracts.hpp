#ifndef COBALT64_NAVI33_CLIENT_CONTRACTS_HPP
#define COBALT64_NAVI33_CLIENT_CONTRACTS_HPP

// C4 user-client ABI contracts + the C6 Metal/accelerator readiness gate.
//
// This is the layer the future Metal-facing runtime (compute-first plan,
// C4/C6) submits through. Everything here is pure, host-testable logic: no
// IOKit includes, no heap, no exceptions, no hardware access of any kind.
// Alignment and size policy are passed in by the caller as target-derived
// values; nothing in this header claims GFX11 hardware constants.
//
// The gate encodes the compute-first architecture decision: the user-client
// opens only when the layers the compute path actually rides on (PCI
// transport, firmware, memory, queue submission) are hardware-verified.
// Display, the Apple IOAccelerator surface, and the Metal runtime itself are
// deliberately NOT required for a headless compute proof.

#include "Navi33IntegrationContracts.hpp"

#include <stdint.h>

namespace cobalt64::research::client {

inline constexpr uint32_t kClientAbiVersion = 1;

// Fixed handle-table capacity. A client policy may cap below this; it can
// never exceed it.
inline constexpr uint32_t kHandleCapacity = 4096;

enum class ClientOp : uint8_t {
    AllocateBuffer = 1,
    ReleaseBuffer = 2,
    SubmitCompute = 3,
    WaitFence = 4,
};

enum class ClientError : uint8_t {
    None = 0,
    InvalidPolicy,
    InvalidByteCount,
    ByteCountTooLarge,
    ClientBudgetExhausted,
    InvalidAlignment,
    HandleTableFull,
    HandleNotOpen,
    InvalidFenceValue,
    GateClosedPciTransport,
    GateClosedFirmware,
    GateClosedMemory,
    GateClosedQueueSubmission,
};

inline const char *clientErrorName(ClientError error) noexcept {
    switch (error) {
        case ClientError::None: return "none";
        case ClientError::InvalidPolicy: return "invalid-policy";
        case ClientError::InvalidByteCount: return "invalid-byte-count";
        case ClientError::ByteCountTooLarge: return "byte-count-too-large";
        case ClientError::ClientBudgetExhausted: return "client-budget-exhausted";
        case ClientError::InvalidAlignment: return "invalid-alignment";
        case ClientError::HandleTableFull: return "handle-table-full";
        case ClientError::HandleNotOpen: return "handle-not-open";
        case ClientError::InvalidFenceValue: return "invalid-fence-value";
        case ClientError::GateClosedPciTransport: return "gate-closed-pci-transport";
        case ClientError::GateClosedFirmware: return "gate-closed-firmware";
        case ClientError::GateClosedMemory: return "gate-closed-memory";
        case ClientError::GateClosedQueueSubmission: return "gate-closed-queue-submission";
    }
    return "unknown";
}

// Target-derived client policy. The bring-up stages fill these from hardware
// evidence; until then every value a test uses is explicitly synthetic.
struct ClientPolicy {
    uint32_t abiVersion = kClientAbiVersion;
    uint64_t maxBufferBytes = 0;      // per-buffer cap
    uint64_t maxClientBytes = 0;      // per-client VRAM budget
    uint32_t minAlignment = 0;        // bytes, power of two
    uint32_t maxAlignment = 0;        // bytes, power of two, >= minAlignment
    uint32_t maxOpenHandles = 0;      // <= kHandleCapacity
};

inline bool policyValid(const ClientPolicy &policy) noexcept {
    const bool alignmentOk =
        policy.minAlignment != 0 &&
        (policy.minAlignment & (policy.minAlignment - 1)) == 0 &&
        policy.maxAlignment >= policy.minAlignment &&
        (policy.maxAlignment & (policy.maxAlignment - 1)) == 0;
    return policy.maxBufferBytes != 0 &&
           policy.maxClientBytes != 0 &&
           policy.maxClientBytes >= policy.maxBufferBytes &&
           alignmentOk &&
           policy.maxOpenHandles != 0 &&
           policy.maxOpenHandles <= kHandleCapacity;
}

inline bool verified(IntegrationState state) noexcept {
    return state == IntegrationState::HardwareVerified;
}

// The compute-first gate (C4). Deliberately does NOT require the display,
// IOAccelerator, or Metal layers: those are C5/C6 milestones on top.
inline ClientError computeGate(const IntegrationReadiness &readiness) noexcept {
    if (!verified(readiness.pciTransport))
        return ClientError::GateClosedPciTransport;
    if (!verified(readiness.firmware))
        return ClientError::GateClosedFirmware;
    if (!verified(readiness.memory))
        return ClientError::GateClosedMemory;
    if (!verified(readiness.queueSubmission))
        return ClientError::GateClosedQueueSubmission;
    return ClientError::None;
}

// Evidence the bring-up kext publishes as IORegistry properties. The
// accelerator maps these onto the readiness model; every field the compute
// path depends on is an explicit, named contract property.
struct BringupEvidenceFlags {
    bool bringupStarted = false;      // "Navi33Bringup,Stage" present
    bool firmwareVerified = false;    // "Navi33Bringup,FirmwareVerified"   (Stage 5.3)
    bool memoryVerified = false;      // "Navi33Bringup,MemoryVerified"      (Stage 5.4)
    bool queueVerified = false;       // "Navi33Bringup,QueueVerified"       (Stage 5.5)
};

inline IntegrationReadiness readinessFromBringupEvidence(
    const BringupEvidenceFlags &flags) noexcept {
    IntegrationReadiness readiness;
    readiness.pciTransport = flags.bringupStarted
                                 ? IntegrationState::HardwareVerified
                                 : IntegrationState::Missing;
    readiness.firmware = flags.firmwareVerified
                             ? IntegrationState::HardwareVerified
                             : IntegrationState::Missing;
    readiness.memory = flags.memoryVerified
                           ? IntegrationState::HardwareVerified
                           : IntegrationState::Missing;
    readiness.queueSubmission = flags.queueVerified
                                    ? IntegrationState::HardwareVerified
                                    : IntegrationState::Missing;
    // Layers above are future milestones; they stay Missing and are not part
    // of the compute gate.
    readiness.ioAccelerator = IntegrationState::Missing;
    readiness.metal = IntegrationState::Missing;
    readiness.display = IntegrationState::Missing;
    return readiness;
}

// Per-client VRAM budget and handle table. Fixed capacity, no heap, no
// exceptions: usable in the kernel as-is. Handles are generation-tagged so a
// stale handle can never address a recycled allocation.
class ClientBudget {
public:
    explicit ClientBudget(const ClientPolicy &policy) noexcept
        : policy_(policy) {}

    const ClientPolicy &policy() const noexcept { return policy_; }

    // Preconditions: outHandle != nullptr. A zero-sized allocation is
    // rejected; alignment is inclusive-bounded by the policy.
    ClientError allocate(uint64_t byteCount, uint32_t alignment,
                         uint32_t *outHandle) noexcept {
        if (!policyValid(policy_))
            return ClientError::InvalidPolicy;
        if (byteCount == 0 || byteCount > policy_.maxBufferBytes)
            return ClientError::ByteCountTooLarge;
        if (alignment < policy_.minAlignment ||
            alignment > policy_.maxAlignment ||
            (alignment & (alignment - 1)) != 0)
            return ClientError::InvalidAlignment;
        if (totalBytes_ > policy_.maxClientBytes ||
            policy_.maxClientBytes - totalBytes_ < byteCount)
            return ClientError::ClientBudgetExhausted;

        uint32_t slot = kInvalidHandle;
        for (uint32_t i = 0; i < policy_.maxOpenHandles; ++i) {
            if (!slots_[i].open) {
                slot = i;
                break;
            }
        }
        if (slot == kInvalidHandle)
            return ClientError::HandleTableFull;

        // Advancing the slot's own generation invalidates every stale handle
        // that ever pointed at this slot, and only those.
        ++slots_[slot].generation;
        slots_[slot].open = true;
        slots_[slot].bytes = byteCount;
        totalBytes_ += byteCount;
        *outHandle = makeHandle(slot);
        return ClientError::None;
    }

    ClientError release(uint32_t handle) noexcept {
        uint32_t slot = 0;
        const ClientError err = lookupSlot(handle, &slot);
        if (err != ClientError::None)
            return err;
        slots_[slot].open = false;
        totalBytes_ -= slots_[slot].bytes;
        slots_[slot].bytes = 0;
        return ClientError::None;
    }

    bool isOpen(uint32_t handle) const noexcept {
        uint32_t slot = 0;
        return lookupSlot(handle, &slot) == ClientError::None;
    }

    uint64_t allocatedBytes() const noexcept { return totalBytes_; }

    static uint32_t slotOf(uint32_t handle) noexcept {
        return handle >> kGenerationBits;
    }
    static uint32_t generationOf(uint32_t handle) noexcept {
        return handle & ((1u << kGenerationBits) - 1u);
    }
    static constexpr uint32_t kInvalidHandle = 0xFFFFFFFFu;

private:
    static constexpr uint32_t kGenerationBits = 16;

    uint32_t makeHandle(uint32_t slot) const noexcept {
        return (slot << kGenerationBits) |
               (slots_[slot].generation & ((1u << kGenerationBits) - 1u));
    }

    ClientError lookupSlot(uint32_t handle, uint32_t *slot) const noexcept {
        if (handle == kInvalidHandle)
            return ClientError::HandleNotOpen;
        const uint32_t s = slotOf(handle);
        if (s >= policy_.maxOpenHandles)
            return ClientError::HandleNotOpen;
        if (!slots_[s].open || slots_[s].generation != generationOf(handle))
            return ClientError::HandleNotOpen;  // closed or stale
        *slot = s;
        return ClientError::None;
    }

    struct Slot {
        uint64_t bytes = 0;
        uint32_t generation = 0;
        bool open = false;
    };

    ClientPolicy policy_;
    Slot slots_[kHandleCapacity] = {};
    uint64_t totalBytes_ = 0;
};

}  // namespace cobalt64::research::client

#endif
