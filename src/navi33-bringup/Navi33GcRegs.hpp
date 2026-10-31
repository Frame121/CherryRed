#ifndef COBALT64_NAVI33_GC_REGS_HPP
#define COBALT64_NAVI33_GC_REGS_HPP

#include <stdint.h>

// Navi 33 (GC 11.0.2 / SDMA 6.0.2) register dwords and base indices.
// Generated and reconciled from AMD gc_11_0_0 and Linux amdgpu headers.
//
// On Navi 33 (PCI 1002:7480):
//   GC IP Instance 0 Base 0: 0x00001260 dwords (0x4980 bytes)
//   GC IP Instance 0 Base 1: 0x0000A000 dwords (0x28000 bytes)

namespace cobalt64::navi33::gc {

constexpr uint32_t kGcBase0Dwords = 0x00001260u;
constexpr uint32_t kGcBase1Dwords = 0x0000A000u;

inline constexpr uint64_t gcByteOffset(uint32_t baseIdx, uint32_t dword) noexcept {
    const uint32_t base = (baseIdx == 1) ? kGcBase1Dwords : kGcBase0Dwords;
    return (static_cast<uint64_t>(base) + dword) * 4ull;
}

// ---------------- GFXHUB / GMC Registers (BASE_IDX 0) ----------------
constexpr uint32_t regGCMC_VM_FB_LOCATION_BASE = 0x1688u;
constexpr uint32_t regGCMC_VM_FB_LOCATION_TOP  = 0x1689u;
constexpr uint32_t regGCMC_VM_AGP_BASE         = 0x168cu;
constexpr uint32_t regGCMC_VM_AGP_BOT          = 0x168bu;
constexpr uint32_t regGCMC_VM_AGP_TOP          = 0x168au;
constexpr uint32_t regGCMC_VM_SYSTEM_APERTURE_LOW_ADDR       = 0x168du;
constexpr uint32_t regGCMC_VM_SYSTEM_APERTURE_HIGH_ADDR      = 0x168eu;
constexpr uint32_t regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB = 0x15a8u;
constexpr uint32_t regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB = 0x15a9u;
constexpr uint32_t regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_LO32 = 0x15cfu;
constexpr uint32_t regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_HI32 = 0x15d0u;
constexpr uint32_t regGCVM_L2_PROTECTION_FAULT_CNTL2         = 0x15c9u;
constexpr uint32_t regGCMC_VM_MX_L1_TLB_CNTL                = 0x168fu;
constexpr uint32_t regGCVM_L2_CNTL                          = 0x15c0u;
constexpr uint32_t regGCVM_L2_CNTL2                         = 0x15c1u;
constexpr uint32_t regGCVM_L2_CNTL3                         = 0x15c2u;
constexpr uint32_t regGCVM_L2_CNTL4                         = 0x15c3u;
constexpr uint32_t regGCVM_L2_CNTL5                         = 0x15c4u;
constexpr uint32_t regGCVM_CONTEXT0_CNTL                    = 0x1580u;
constexpr uint32_t regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32 = 0x15a0u;
constexpr uint32_t regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_HI32 = 0x15a1u;
constexpr uint32_t regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_LO32 = 0x159au;
constexpr uint32_t regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_HI32 = 0x159bu;
constexpr uint32_t regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_LO32 = 0x159cu;
constexpr uint32_t regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_HI32 = 0x159du;

// ---------------- SDMA0 Registers ----------------
constexpr uint32_t regSDMA0_SEM_WAIT_FAIL_TIMER_CNTL = 0x0022u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_CNTL          = 0x0080u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_BASE          = 0x0081u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_BASE_HI       = 0x0082u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_RPTR          = 0x0083u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_RPTR_HI       = 0x0084u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_WPTR          = 0x0085u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_WPTR_HI       = 0x0086u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_RPTR_ADDR_LO  = 0x0089u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_RPTR_ADDR_HI  = 0x0088u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_LO = 0x00b3u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_HI = 0x00b2u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_MINOR_PTR_UPDATE = 0x00b5u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_DOORBELL         = 0x0092u; // BASE_IDX 0
constexpr uint32_t regSDMA0_QUEUE0_IB_CNTL          = 0x008au; // BASE_IDX 0
constexpr uint32_t regSDMA0_F32_CNTL                = 0x589au; // BASE_IDX 1

// ---------------- CP & GFX Ring Registers ----------------
constexpr uint32_t regGRBM_GFX_CNTL                 = 0x0900u; // BASE_IDX 1
constexpr uint32_t regCP_PFP_PRGRM_CNTR_START       = 0x0926u; // BASE_IDX 1
constexpr uint32_t regCP_PFP_PRGRM_CNTR_START_HI    = 0x0927u; // BASE_IDX 1
constexpr uint32_t regCP_ME_PRGRM_CNTR_START        = 0x0928u; // BASE_IDX 1
constexpr uint32_t regCP_ME_PRGRM_CNTR_START_HI     = 0x0929u; // BASE_IDX 1
constexpr uint32_t regCP_MEC_RS64_PRGRM_CNTR_START  = 0x2900u; // BASE_IDX 1
constexpr uint32_t regCP_MEC_RS64_PRGRM_CNTR_START_HI = 0x2901u; // BASE_IDX 1
constexpr uint32_t regCP_MEC_RS64_CNTL              = 0x2904u; // BASE_IDX 1
constexpr uint32_t regCP_ME_CNTL                    = 0x0924u; // BASE_IDX 1
constexpr uint32_t regCP_STAT                       = 0x0f40u; // BASE_IDX 0
constexpr uint32_t regCP_RB_WPTR_DELAY              = 0x0f61u; // BASE_IDX 0
constexpr uint32_t regCP_RB_VMID                    = 0x1df1u; // BASE_IDX 0
constexpr uint32_t regCP_RB0_CNTL                   = 0x1de1u; // BASE_IDX 0
constexpr uint32_t regCP_RB0_WPTR                   = 0x1df4u; // BASE_IDX 0
constexpr uint32_t regCP_RB0_WPTR_HI                = 0x1df5u; // BASE_IDX 0
constexpr uint32_t regCP_RB0_RPTR                   = 0x0f60u; // BASE_IDX 0
constexpr uint32_t regCP_RB0_RPTR_ADDR              = 0x1de3u; // BASE_IDX 0
constexpr uint32_t regCP_RB0_RPTR_ADDR_HI           = 0x1de4u; // BASE_IDX 0
constexpr uint32_t regCP_RB_WPTR_POLL_ADDR_LO       = 0x1e8bu; // BASE_IDX 0
constexpr uint32_t regCP_RB_WPTR_POLL_ADDR_HI       = 0x1e8cu; // BASE_IDX 0
constexpr uint32_t regCP_RB0_BASE                   = 0x1de0u; // BASE_IDX 0
constexpr uint32_t regCP_RB0_BASE_HI                = 0x1e51u; // BASE_IDX 0
constexpr uint32_t regCP_RB_ACTIVE                  = 0x1f40u; // BASE_IDX 0
constexpr uint32_t regCP_RB_DOORBELL_CONTROL        = 0x1e8du; // BASE_IDX 0
constexpr uint32_t regCP_MAX_CONTEXT                = 0x1e4eu; // BASE_IDX 0
constexpr uint32_t regCP_DEVICE_ID                  = 0x1debu; // BASE_IDX 0
constexpr uint32_t regSCRATCH_REG0                  = 0x2040u; // BASE_IDX 1

}  // namespace cobalt64::navi33::gc

#endif // COBALT64_NAVI33_GC_REGS_HPP
