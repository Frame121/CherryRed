#include "gfx11.h"

#include <string.h>

#include <linux/delay.h>
#include "gc_regs.h"
#include "osssys_regs.h"
#include "nbio_regs.h"

namespace {

// SDMA packet header: op in bits 7:0, sub_op in bits 15:8 (sdma_v6_0_pkt_open.h).
const u32 kSdmaOpWrite = 2, kSdmaSubopWriteLinear = 0;

inline u32 sdmaReg(struct rn_device *adev, u32 reg, u32 baseIdx) {
    return adev->reg_offset[RN_GC_HWIP][0][baseIdx] + reg;
}

}

// Note: the reg argument must reach RREG32_SOC15 unexpanded (it pastes reg##_BASE_IDX), so these are used directly below.
#define SDR(r) rn_rreg32(adev, sdmaReg(adev, r, r##_BASE_IDX))
#define SDW(r, v) rn_wreg32(adev, sdmaReg(adev, r, r##_BASE_IDX), (v))

extern "C" {

void rn_gfxhub_enable(struct rn_device *adev, const struct rn_gmc *gmc) {
    u32 tmp;

    // One 8-byte PTE per 4 KB GART page; the page at GART address A maps to VRAM offset A (valid | readable | writeable).
    for (u64 va = gmc->gart_start; va <= gmc->gart_end; va += 4096) {
        u64 pte = va | 0x1ull | 0x20ull | 0x40ull;
        u64 slot = gmc->pt_vram_offset + ((va - gmc->gart_start) >> 12) * 8;
        rn_vram_wr32(adev, slot, lower_32_bits(pte));
        rn_vram_wr32(adev, slot + 4, upper_32_bits(pte));
    }

    // init_gart_aperture_regs: the VMID 0 page table (physical VRAM offset | valid) and the GART range in 4 KB pages
    u64 pt_base = gmc->pt_vram_offset | 1ull;
    WREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32, lower_32_bits(pt_base));
    WREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_HI32, upper_32_bits(pt_base));
    WREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_LO32, (u32)(gmc->gart_start >> 12));
    WREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_HI32, (u32)(gmc->gart_start >> 44));
    WREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_LO32, (u32)(gmc->gart_end >> 12));
    WREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_HI32, (u32)(gmc->gart_end >> 44));

    // init_system_aperture_regs: AGP off (start above end), the system aperture covers VRAM
    const u64 agp_start = 0xFFFFFFFFFFFFull, agp_end = 0;
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_AGP_BASE, 0);
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_AGP_BOT, (u32)(agp_start >> 24));
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_AGP_TOP, (u32)(agp_end >> 24));
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_SYSTEM_APERTURE_LOW_ADDR, (u32)(min(gmc->fb_start, agp_start) >> 18));
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_SYSTEM_APERTURE_HIGH_ADDR, (u32)(max(gmc->fb_end, agp_end) >> 18));
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB, (u32)(gmc->scratch_vram_offset >> 12));
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB, (u32)(gmc->scratch_vram_offset >> 44));
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_LO32, (u32)(gmc->dummy_vram_offset >> 12));
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_HI32, (u32)(gmc->dummy_vram_offset >> 44));
    tmp = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_CNTL2);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_PROTECTION_FAULT_CNTL2, ACTIVE_PAGE_MIGRATION_PTE_READ_RETRY, 1);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_PROTECTION_FAULT_CNTL2, ENABLE_RETRY_FAULT_INTERRUPT, 1);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_CNTL2, tmp);

    // init_tlb_regs
    tmp = RREG32_SOC15(RN_GC, 0, regGCMC_VM_MX_L1_TLB_CNTL);
    tmp = REG_SET_FIELD(tmp, GCMC_VM_MX_L1_TLB_CNTL, ENABLE_L1_TLB, 1);
    tmp = REG_SET_FIELD(tmp, GCMC_VM_MX_L1_TLB_CNTL, SYSTEM_ACCESS_MODE, 3);
    tmp = REG_SET_FIELD(tmp, GCMC_VM_MX_L1_TLB_CNTL, ENABLE_ADVANCED_DRIVER_MODEL, 1);
    tmp = REG_SET_FIELD(tmp, GCMC_VM_MX_L1_TLB_CNTL, SYSTEM_APERTURE_UNMAPPED_ACCESS, 0);
    tmp = REG_SET_FIELD(tmp, GCMC_VM_MX_L1_TLB_CNTL, ECO_BITS, 0);
    tmp = REG_SET_FIELD(tmp, GCMC_VM_MX_L1_TLB_CNTL, MTYPE, 3);              // MTYPE_UC
    WREG32_SOC15(RN_GC, 0, regGCMC_VM_MX_L1_TLB_CNTL, tmp);

    // init_cache_regs (no translate_further: bank select 9, big-K fragment 6)
    tmp = RREG32_SOC15(RN_GC, 0, regGCVM_L2_CNTL);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL, ENABLE_L2_CACHE, 1);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL, ENABLE_L2_FRAGMENT_PROCESSING, 0);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL, ENABLE_DEFAULT_PAGE_OUT_TO_SYSTEM_MEMORY, 1);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL, L2_PDE0_CACHE_TAG_GENERATION_MODE, 0);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL, PDE_FAULT_CLASSIFICATION, 0);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL, CONTEXT1_IDENTITY_ACCESS_MODE, 1);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL, IDENTITY_MODE_FRAGMENT_SIZE, 0);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CNTL, tmp);
    tmp = RREG32_SOC15(RN_GC, 0, regGCVM_L2_CNTL2);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL2, INVALIDATE_ALL_L1_TLBS, 1);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL2, INVALIDATE_L2_CACHE, 1);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CNTL2, tmp);
    tmp = 0x80100007u;                                                       // gfxhub_v3_0_3.c redefines the default (gc_11_0_0 has 0x80120007)
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL3, BANK_SELECT, 9);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL3, L2_CACHE_BIGK_FRAGMENT_SIZE, 6);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CNTL3, tmp);
    tmp = 0x000000c1u;
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL4, VMC_TAP_PDE_REQUEST_PHYSICAL, 0);
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL4, VMC_TAP_PTE_REQUEST_PHYSICAL, 0);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CNTL4, tmp);
    tmp = 0x00003fe0u;
    tmp = REG_SET_FIELD(tmp, GCVM_L2_CNTL5, L2_CACHE_SMALLK_FRAGMENT_SIZE, 0);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CNTL5, tmp);

    // enable_system_domain (VMID 0, single level)
    tmp = RREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_CNTL);
    tmp = REG_SET_FIELD(tmp, GCVM_CONTEXT0_CNTL, ENABLE_CONTEXT, 1);
    tmp = REG_SET_FIELD(tmp, GCVM_CONTEXT0_CNTL, PAGE_TABLE_DEPTH, 0);
    tmp = REG_SET_FIELD(tmp, GCVM_CONTEXT0_CNTL, RETRY_PERMISSION_OR_INVALID_PAGE_FAULT, 0);
    WREG32_SOC15(RN_GC, 0, regGCVM_CONTEXT0_CNTL, tmp);

    // disable_identity_aperture
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_LO32, 0xFFFFFFFF);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_HI32, 0x0000000F);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_LO32, 0);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_HI32, 0);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_LO32, 0);
    WREG32_SOC15(RN_GC, 0, regGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_HI32, 0);
}

void rn_sdma0_test(struct rn_device *adev, u64 vaBase, u64 ringOff, u32 ringBytes, u64 targetOff, struct rn_sdma_result *res) {
    memset(res, 0, sizeof(*res));
    const u64 ringMc = vaBase + ringOff;
    const u64 rptrOff = ringOff + 0x1000, wptrOff = ringOff + 0x1008;      // pointer shadows after the ring page
    const u64 targetMc = vaBase + targetOff;
    u32 tmp;

    for (u32 o = 0; o < ringBytes; o += 4) rn_vram_wr32(adev, ringOff + o, 0);
    rn_vram_wr32(adev, rptrOff, 0); rn_vram_wr32(adev, rptrOff + 4, 0);
    rn_vram_wr32(adev, wptrOff, 0); rn_vram_wr32(adev, wptrOff + 4, 0);
    rn_vram_wr32(adev, targetOff, 0xCAFEDEADu);
    res->pre_value = rn_vram_rd32(adev, targetOff);
    res->step = 1;

    // the packet: WRITE_LINEAR of one dword, as sdma_v6_0_ring_test_ring
    const u32 pkt[5] = {kSdmaOpWrite | (kSdmaSubopWriteLinear << 8), lower_32_bits(targetMc), upper_32_bits(targetMc), 0, 0xDEADBEEFu};
    for (u32 i = 0; i < 5; i++) rn_vram_wr32(adev, ringOff + i * 4, pkt[i]);

    // sdma_v6_0_gfx_resume_instance, instance 0, no doorbell
    SDW(regSDMA0_SEM_WAIT_FAIL_TIMER_CNTL, 0);
    u32 rbBufsz = 0;
    while ((4u << rbBufsz) < ringBytes) rbBufsz++;                          // order_base_2(ring_size / 4)
    u32 rbCntl = SDR(regSDMA0_QUEUE0_RB_CNTL);
    rbCntl = REG_SET_FIELD(rbCntl, SDMA0_QUEUE0_RB_CNTL, RB_SIZE, rbBufsz);
    rbCntl = REG_SET_FIELD(rbCntl, SDMA0_QUEUE0_RB_CNTL, RB_PRIV, 1);
    SDW(regSDMA0_QUEUE0_RB_CNTL, rbCntl);
    SDW(regSDMA0_QUEUE0_RB_RPTR, 0);
    SDW(regSDMA0_QUEUE0_RB_RPTR_HI, 0);
    SDW(regSDMA0_QUEUE0_RB_WPTR, 0);
    SDW(regSDMA0_QUEUE0_RB_WPTR_HI, 0);
    SDW(regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_LO, lower_32_bits(vaBase + wptrOff));
    SDW(regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_HI, upper_32_bits(vaBase + wptrOff));
    SDW(regSDMA0_QUEUE0_RB_RPTR_ADDR_HI, upper_32_bits(vaBase + rptrOff));
    SDW(regSDMA0_QUEUE0_RB_RPTR_ADDR_LO, lower_32_bits(vaBase + rptrOff) & 0xFFFFFFFCu);
    rbCntl = REG_SET_FIELD(rbCntl, SDMA0_QUEUE0_RB_CNTL, RPTR_WRITEBACK_ENABLE, 1);
    rbCntl = REG_SET_FIELD(rbCntl, SDMA0_QUEUE0_RB_CNTL, WPTR_POLL_ENABLE, 0);
    rbCntl = REG_SET_FIELD(rbCntl, SDMA0_QUEUE0_RB_CNTL, F32_WPTR_POLL_ENABLE, 1);
    SDW(regSDMA0_QUEUE0_RB_BASE, (u32)(ringMc >> 8));
    SDW(regSDMA0_QUEUE0_RB_BASE_HI, (u32)(ringMc >> 40));
    SDW(regSDMA0_QUEUE0_MINOR_PTR_UPDATE, 1);
    SDW(regSDMA0_QUEUE0_RB_WPTR, 0);
    SDW(regSDMA0_QUEUE0_RB_WPTR_HI, 0);
    tmp = SDR(regSDMA0_QUEUE0_DOORBELL);
    tmp = REG_SET_FIELD(tmp, SDMA0_QUEUE0_DOORBELL, ENABLE, 0);
    SDW(regSDMA0_QUEUE0_DOORBELL, tmp);
    SDW(regSDMA0_QUEUE0_MINOR_PTR_UPDATE, 0);
    tmp = SDR(regSDMA0_UTCL1_CNTL);
    tmp = REG_SET_FIELD(tmp, SDMA0_UTCL1_CNTL, RESP_MODE, 3);
    tmp = REG_SET_FIELD(tmp, SDMA0_UTCL1_CNTL, REDO_DELAY, 9);
    SDW(regSDMA0_UTCL1_CNTL, tmp);
    res->step = 2;

    tmp = SDR(regSDMA0_F32_CNTL);                                           // unhalt the engine
    tmp = REG_SET_FIELD(tmp, SDMA0_F32_CNTL, HALT, 0);
    tmp = REG_SET_FIELD(tmp, SDMA0_F32_CNTL, TH1_RESET, 0);
    SDW(regSDMA0_F32_CNTL, tmp);
    rbCntl = REG_SET_FIELD(rbCntl, SDMA0_QUEUE0_RB_CNTL, RB_ENABLE, 1);
    SDW(regSDMA0_QUEUE0_RB_CNTL, rbCntl);
    u32 ibCntl = SDR(regSDMA0_QUEUE0_IB_CNTL);
    ibCntl = REG_SET_FIELD(ibCntl, SDMA0_QUEUE0_IB_CNTL, IB_ENABLE, 1);
    SDW(regSDMA0_QUEUE0_IB_CNTL, ibCntl);
    res->step = 3;

    // With F32_WPTR_POLL_ENABLE the SDMA firmware polls the wptr shadow in memory (the doorbell path of sdma_v6_0_ring_set_wptr
    // stores wptr << 2 there before ringing), so the shadow is written as well as the register.
    rn_vram_wr32(adev, wptrOff, 5u << 2);
    rn_vram_wr32(adev, wptrOff + 4, 0);
    SDW(regSDMA0_QUEUE0_RB_WPTR, 5u << 2);                                  // 5 dwords, wptr in bytes
    SDW(regSDMA0_QUEUE0_RB_WPTR_HI, 0);
    res->step = 4;

    for (int i = 0; i < 2000; i++) {                                        // up to 2 s
        res->value = rn_vram_rd32(adev, targetOff);
        if (res->value == 0xDEADBEEFu) { res->ok = 1; break; }
        msleep(1);
    }
    res->rptr = SDR(regSDMA0_QUEUE0_RB_RPTR);
    res->wptr = SDR(regSDMA0_QUEUE0_RB_WPTR);
    res->f32_cntl = SDR(regSDMA0_F32_CNTL);
    res->rb_cntl = SDR(regSDMA0_QUEUE0_RB_CNTL);
    res->fault_status = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_STATUS);
    res->fault_addr_lo = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_LO32);
    res->fault_addr_hi = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_HI32);
    res->sdma_status = SDR(regSDMA0_STATUS_REG);
    res->sdma_status1 = SDR(regSDMA0_STATUS1_REG);
    res->ctx_status = SDR(regSDMA0_QUEUE0_CONTEXT_STATUS);
    res->version = SDR(regSDMA0_VERSION);
    res->cntl = SDR(regSDMA0_CNTL);
    res->power_cntl = SDR(regSDMA0_POWER_CNTL);
    res->checksum = SDR(regSDMA0_UCODE_CHECKSUM);
    res->freeze = SDR(regSDMA0_FREEZE);
    res->chicken = SDR(regSDMA0_CHICKEN_BITS);
    res->program = SDR(regSDMA0_PROGRAM);
    res->ring0 = rn_vram_rd32(adev, ringOff);
    res->ring3 = rn_vram_rd32(adev, ringOff + 12);
    res->rptr_shadow = rn_vram_rd32(adev, rptrOff);
    res->wptr_shadow = rn_vram_rd32(adev, wptrOff);
    res->step = 5;
}

// soc21_grbm_select: pick the ME / pipe / VMID / queue that the per-queue registers below refer to.
static void grbmSelect(struct rn_device *adev, u32 me, u32 pipe, u32 vmid, u32 queue) {
    u32 t = REG_SET_FIELD(0, GRBM_GFX_CNTL, PIPEID, pipe);
    t = REG_SET_FIELD(t, GRBM_GFX_CNTL, MEID, me);
    t = REG_SET_FIELD(t, GRBM_GFX_CNTL, VMID, vmid);
    t = REG_SET_FIELD(t, GRBM_GFX_CNTL, QUEUEID, queue);
    WREG32_SOC15(RN_GC, 0, regGRBM_GFX_CNTL, t);
}

#define RN_PACKET3(op, n) ((3u << 30) | (((n) & 0x3FFFu) << 16) | (((op) & 0xFFu) << 8))
#define RN_PACKET3_NOP 0x10u
#define RN_PACKET3_SET_UCONFIG_REG 0x79u
#define RN_PACKET3_SET_UCONFIG_REG_START 0x0000c000u

void rn_gfx_ring_test(struct rn_device *adev, u64 vaBase, u64 ringOff, u32 ringBytes, const struct rn_cp_start *pc,
                      struct rn_gfx_result *res) {
    memset(res, 0, sizeof(*res));
    const u64 ringVa = vaBase + ringOff;
    const u64 rptrOff = ringOff + 0x1000, wptrOff = ringOff + 0x1008;      // pointer shadows after the ring page
    u32 tmp;
    res->me_cntl_before = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
    res->cp_stat_before = RREG32_SOC15(RN_GC, 0, regCP_STAT);

    // gfx_v11_0_config_gfx_rs64: the entry point of each RS64 processor, then a reset pulse of its pipes
    for (u32 pipe = 0; pipe < 2; pipe++) {
        grbmSelect(adev, 0, pipe, 0, 0);
        WREG32_SOC15(RN_GC, 0, regCP_PFP_PRGRM_CNTR_START, (pc->pfp_hi << 30) | (pc->pfp_lo >> 2));
        WREG32_SOC15(RN_GC, 0, regCP_PFP_PRGRM_CNTR_START_HI, pc->pfp_hi >> 2);
    }
    grbmSelect(adev, 0, 0, 0, 0);
    tmp = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, PFP_PIPE0_RESET, 1);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, PFP_PIPE1_RESET, 1);
    WREG32_SOC15(RN_GC, 0, regCP_ME_CNTL, tmp);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, PFP_PIPE0_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, PFP_PIPE1_RESET, 0);
    WREG32_SOC15(RN_GC, 0, regCP_ME_CNTL, tmp);
    for (u32 pipe = 0; pipe < 2; pipe++) {
        grbmSelect(adev, 0, pipe, 0, 0);
        WREG32_SOC15(RN_GC, 0, regCP_ME_PRGRM_CNTR_START, (pc->me_hi << 30) | (pc->me_lo >> 2));
        WREG32_SOC15(RN_GC, 0, regCP_ME_PRGRM_CNTR_START_HI, pc->me_hi >> 2);
    }
    grbmSelect(adev, 0, 0, 0, 0);
    tmp = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, ME_PIPE0_RESET, 1);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, ME_PIPE1_RESET, 1);
    WREG32_SOC15(RN_GC, 0, regCP_ME_CNTL, tmp);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, ME_PIPE0_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, ME_PIPE1_RESET, 0);
    WREG32_SOC15(RN_GC, 0, regCP_ME_CNTL, tmp);
    for (u32 pipe = 0; pipe < 4; pipe++) {
        grbmSelect(adev, 1, pipe, 0, 0);
        WREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_PRGRM_CNTR_START, (pc->mec_lo >> 2) | (pc->mec_hi << 30));
        WREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_PRGRM_CNTR_START_HI, pc->mec_hi >> 2);
    }
    grbmSelect(adev, 0, 0, 0, 0);
    tmp = RREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_CNTL);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE0_RESET, 1);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE1_RESET, 1);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE2_RESET, 1);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE3_RESET, 1);
    WREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_CNTL, tmp);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE0_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE1_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE2_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE3_RESET, 0);
    WREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_CNTL, tmp);
    res->step = 1;

    // the packet of ring_test_ring: SET_UCONFIG_REG SCRATCH_REG0 = 0xDEADBEEF, padded with a NOP packet to 8 dwords
    const u32 scratchDw = 0xc000u + 0x40u;                                   // regSCRATCH_REG0 is at GC base 1 (0xa000) + 0x2040
    const u32 pkt[8] = {RN_PACKET3(RN_PACKET3_SET_UCONFIG_REG, 1), scratchDw - RN_PACKET3_SET_UCONFIG_REG_START, 0xDEADBEEFu,
                        RN_PACKET3(RN_PACKET3_NOP, 3), 0, 0, 0, 0};
    for (u32 o = 0; o < ringBytes; o += 4) rn_vram_wr32(adev, ringOff + o, 0);
    for (u32 i = 0; i < 8; i++) rn_vram_wr32(adev, ringOff + i * 4, pkt[i]);
    rn_vram_wr32(adev, rptrOff, 0); rn_vram_wr32(adev, rptrOff + 4, 0);
    rn_vram_wr32(adev, wptrOff, 0); rn_vram_wr32(adev, wptrOff + 4, 0);
    WREG32_SOC15(RN_GC, 0, regSCRATCH_REG0, 0xCAFEDEADu);

    // The gfx queue of GC 11 is a hardware queue (CP_GFX_HQD_*) that Linux fills from an in-memory MQD (gfx_v11_0_gfx_mqd_init);
    // the legacy CP_RB0_* registers are not what the CP runs, and with CP_GFX_MQD_BASE_ADDR at 0 the CP reads the MQD at address 0
    // when it starts and faults (CPG, GCVM_L2_PROTECTION_FAULT_STATUS 0xd32, seen on hardware). So the same values go straight
    // into the HQD registers, and a zeroed MQD page stands behind it.
    const u64 mqdVa = vaBase + ringOff + 0x2000;
    for (u32 o = 0; o < 0x1000; o += 4) rn_vram_wr32(adev, ringOff + 0x2000 + o, 0);
    grbmSelect(adev, 0, 0, 0, 0);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_ACTIVE, 0);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_MQD_BASE_ADDR, lower_32_bits(mqdVa) & 0xfffffffcu);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_MQD_BASE_ADDR_HI, upper_32_bits(mqdVa));
    tmp = regCP_GFX_MQD_CONTROL_DEFAULT;
    tmp = REG_SET_FIELD(tmp, CP_GFX_MQD_CONTROL, VMID, 0);
    tmp = REG_SET_FIELD(tmp, CP_GFX_MQD_CONTROL, PRIV_STATE, 1);
    tmp = REG_SET_FIELD(tmp, CP_GFX_MQD_CONTROL, CACHE_POLICY, 0);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_MQD_CONTROL, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_VMID, 0);
    tmp = regCP_GFX_HQD_QUANTUM_DEFAULT;
    tmp = REG_SET_FIELD(tmp, CP_GFX_HQD_QUANTUM, QUANTUM_EN, 1);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_QUANTUM, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_BASE, lower_32_bits(ringVa >> 8));
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_BASE_HI, upper_32_bits(ringVa >> 8));
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_RPTR_ADDR, lower_32_bits(vaBase + rptrOff) & 0xfffffffcu);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_RPTR_ADDR_HI, upper_32_bits(vaBase + rptrOff) & 0xFFFFu);
    WREG32_SOC15(RN_GC, 0, regCP_RB_WPTR_POLL_ADDR_LO, lower_32_bits(vaBase + wptrOff) & 0xfffffffcu);
    WREG32_SOC15(RN_GC, 0, regCP_RB_WPTR_POLL_ADDR_HI, upper_32_bits(vaBase + wptrOff) & 0xFFFFu);
    u32 qOrder = 0;
    while ((4u << qOrder) < ringBytes) qOrder++;                             // order_base_2(queue_size / 4)
    tmp = regCP_GFX_HQD_CNTL_DEFAULT;
    tmp = REG_SET_FIELD(tmp, CP_GFX_HQD_CNTL, RB_BUFSZ, qOrder - 1);
    tmp = REG_SET_FIELD(tmp, CP_GFX_HQD_CNTL, RB_BLKSZ, qOrder - 3);
    tmp = REG_SET_FIELD(tmp, CP_GFX_HQD_CNTL, KMD_QUEUE, 1);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_CNTL, tmp);
    tmp = RREG32_SOC15(RN_GC, 0, regCP_RB_DOORBELL_CONTROL);                 // no doorbell: the wptr register is written directly
    tmp = REG_SET_FIELD(tmp, CP_RB_DOORBELL_CONTROL, DOORBELL_EN, 0);
    WREG32_SOC15(RN_GC, 0, regCP_RB_DOORBELL_CONTROL, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_RPTR, regCP_GFX_HQD_RPTR_DEFAULT);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_WPTR, 0);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_WPTR_HI, 0);
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_ACTIVE, 1);
    res->step = 2;

    // gfx_v11_0_cp_gfx_start, without the clear state: MAX_CONTEXT, DEVICE_ID, then unhalt PFP and ME
    WREG32_SOC15(RN_GC, 0, regCP_MAX_CONTEXT, 7);
    WREG32_SOC15(RN_GC, 0, regCP_DEVICE_ID, 1);
    tmp = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, ME_HALT, 0);
    tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, PFP_HALT, 0);
    WREG32_SOC15(RN_GC, 0, regCP_ME_CNTL, tmp);
    res->me_cntl_imm = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
    mdelay(10);
    res->me_cntl_10ms = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
    res->rb0_cntl_rd = RREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_CNTL);
    res->rb0_base_rd = RREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_BASE);
    res->rb0_base_hi_rd = RREG32_SOC15(RN_GC, 0, regCP_GFX_MQD_BASE_ADDR);
    res->rb_active_rd = RREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_ACTIVE);
    res->max_ctx_rd = RREG32_SOC15(RN_GC, 0, regCP_MAX_CONTEXT);
    res->device_id_rd = RREG32_SOC15(RN_GC, 0, regCP_DEVICE_ID);
    res->pfp_pc_rd = RREG32_SOC15(RN_GC, 0, regCP_PFP_PRGRM_CNTR_START);
    res->me_pc_rd = RREG32_SOC15(RN_GC, 0, regCP_ME_PRGRM_CNTR_START);
    for (int i = 0; i < 1000; i++) {                                         // CP_STAT reads 0 once the CP is idle and up
        if (RREG32_SOC15(RN_GC, 0, regCP_STAT) == 0) break;
        udelay(1);
    }
    res->step = 3;

    rn_vram_wr32(adev, wptrOff, 8);                                          // the polled shadow as well as the register
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_WPTR, 8);                           // 8 dwords
    WREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_WPTR_HI, 0);
    res->step = 4;
    static const u32 sampleDelayMs[8] = {0, 1, 1, 3, 5, 10, 30, 50};        // cumulative: 0, 1, 2, 5, 10, 20, 50, 100 ms
    for (u32 i = 0; i < 8; i++) {
        if (sampleDelayMs[i]) msleep(sampleDelayMs[i]);
        res->trace[i][0] = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
        res->trace[i][1] = RREG32_SOC15(RN_GC, 0, regCP_STAT);
        res->trace[i][2] = RREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_RPTR);
    }
    res->fault_status = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_STATUS);
    res->fault_addr_lo = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_LO32);
    res->fault_addr_hi = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_HI32);
    for (int i = 0; i < 2000; i++) {                                         // up to 2 s
        res->scratch = RREG32_SOC15(RN_GC, 0, regSCRATCH_REG0);
        if (res->scratch == 0xDEADBEEFu) { res->ok = 1; break; }
        msleep(1);
    }
    res->me_cntl = RREG32_SOC15(RN_GC, 0, regCP_ME_CNTL);
    res->cp_stat = RREG32_SOC15(RN_GC, 0, regCP_STAT);
    res->rptr = RREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_RPTR);
    res->wptr = RREG32_SOC15(RN_GC, 0, regCP_GFX_HQD_WPTR);
    res->rptr_shadow = rn_vram_rd32(adev, rptrOff);
    res->ring0 = rn_vram_rd32(adev, ringOff);
}

void rn_compute_ring_test(struct rn_device *adev, u64 vaBase, u64 ringOff, u32 ringBytes, struct rn_compute_result *res) {
    memset(res, 0, sizeof(*res));
    const u64 ringVa = vaBase + ringOff;
    const u64 rptrOff = ringOff + 0x1000, wptrOff = ringOff + 0x1008, mqdOff = ringOff + 0x2000, eopOff = ringOff + 0x4000;
    u32 tmp;

    // gfx_v11_0_cp_compute_enable(true): the four MEC pipes out of reset and active, ucode cache valid, MEC running
    tmp = RREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_CNTL);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_INVALIDATE_ICACHE, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE0_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE1_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE2_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE3_RESET, 0);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE0_ACTIVE, 1);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE1_ACTIVE, 1);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE2_ACTIVE, 1);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_PIPE3_ACTIVE, 1);
    tmp = REG_SET_FIELD(tmp, CP_MEC_RS64_CNTL, MEC_HALT, 0);
    WREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_CNTL, tmp);
    udelay(50);
    res->step = 1;

    // the packet: SET_UCONFIG_REG SCRATCH_REG0 = 0xDEADBEEF (a compute ring is tested with it too), padded to 8 dwords
    const u32 scratchDw = 0xc000u + 0x40u;
    const u32 pkt[8] = {RN_PACKET3(RN_PACKET3_SET_UCONFIG_REG, 1), scratchDw - RN_PACKET3_SET_UCONFIG_REG_START, 0xDEADBEEFu,
                        RN_PACKET3(RN_PACKET3_NOP, 3), 0, 0, 0, 0};
    for (u32 o = 0; o < ringBytes; o += 4) rn_vram_wr32(adev, ringOff + o, 0);
    for (u32 i = 0; i < 8; i++) rn_vram_wr32(adev, ringOff + i * 4, pkt[i]);
    for (u32 o = 0; o < 0x1000; o += 4) rn_vram_wr32(adev, mqdOff + o, 0);
    rn_vram_wr32(adev, mqdOff, 0xC0310800u);                                 // mqd->header
    for (u32 o = 0; o < 2048; o += 4) rn_vram_wr32(adev, eopOff + o, 0);
    rn_vram_wr32(adev, rptrOff, 0); rn_vram_wr32(adev, rptrOff + 4, 0);
    rn_vram_wr32(adev, wptrOff, 0); rn_vram_wr32(adev, wptrOff + 4, 0);
    WREG32_SOC15(RN_GC, 0, regSCRATCH_REG0, 0xBADC0DE0u);

    // gfx_v11_0_compute_mqd_init values, written like gfx_v11_0_kiq_init_register
    grbmSelect(adev, 1, 0, 0, 2);
    tmp = RREG32_SOC15(RN_GC, 0, regCP_PQ_WPTR_POLL_CNTL);                    // wptr polling on: no doorbell, the shadow is the doorbell
    tmp = REG_SET_FIELD(tmp, CP_PQ_WPTR_POLL_CNTL, EN, 1);
    WREG32_SOC15(RN_GC, 0, regCP_PQ_WPTR_POLL_CNTL, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_ACTIVE, 0);
    const u64 eopBase = (vaBase + eopOff) >> 8;
    WREG32_SOC15(RN_GC, 0, regCP_HQD_EOP_BASE_ADDR, lower_32_bits(eopBase));
    WREG32_SOC15(RN_GC, 0, regCP_HQD_EOP_BASE_ADDR_HI, upper_32_bits(eopBase));
    tmp = regCP_HQD_EOP_CONTROL_DEFAULT;
    tmp = REG_SET_FIELD(tmp, CP_HQD_EOP_CONTROL, EOP_SIZE, 8);               // order_base_2(GFX11_MEC_HPD_SIZE 2048 / 4) - 1
    WREG32_SOC15(RN_GC, 0, regCP_HQD_EOP_CONTROL, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_DOORBELL_CONTROL, regCP_HQD_PQ_DOORBELL_CONTROL_DEFAULT);   // doorbell disabled
    WREG32_SOC15(RN_GC, 0, regCP_MQD_BASE_ADDR, lower_32_bits(vaBase + mqdOff) & 0xfffffffcu);
    WREG32_SOC15(RN_GC, 0, regCP_MQD_BASE_ADDR_HI, upper_32_bits(vaBase + mqdOff));
    tmp = regCP_MQD_CONTROL_DEFAULT;
    tmp = REG_SET_FIELD(tmp, CP_MQD_CONTROL, VMID, 0);
    WREG32_SOC15(RN_GC, 0, regCP_MQD_CONTROL, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_BASE, lower_32_bits(ringVa >> 8));
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_BASE_HI, upper_32_bits(ringVa >> 8));
    u32 qOrder = 0;
    while ((4u << qOrder) < ringBytes) qOrder++;                             // order_base_2(queue_size / 4)
    tmp = regCP_HQD_PQ_CONTROL_DEFAULT;
    tmp = REG_SET_FIELD(tmp, CP_HQD_PQ_CONTROL, QUEUE_SIZE, qOrder - 1);
    tmp = REG_SET_FIELD(tmp, CP_HQD_PQ_CONTROL, RPTR_BLOCK_SIZE, 9);         // order_base_2(4096 / 4) - 1
    tmp = REG_SET_FIELD(tmp, CP_HQD_PQ_CONTROL, UNORD_DISPATCH, 1);
    tmp = REG_SET_FIELD(tmp, CP_HQD_PQ_CONTROL, TUNNEL_DISPATCH, 0);
    tmp = REG_SET_FIELD(tmp, CP_HQD_PQ_CONTROL, PRIV_STATE, 1);
    tmp = REG_SET_FIELD(tmp, CP_HQD_PQ_CONTROL, KMD_QUEUE, 1);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_CONTROL, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_RPTR_REPORT_ADDR, lower_32_bits(vaBase + rptrOff) & 0xfffffffcu);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_RPTR_REPORT_ADDR_HI, upper_32_bits(vaBase + rptrOff) & 0xFFFFu);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_POLL_ADDR, lower_32_bits(vaBase + wptrOff) & 0xfffffffcu);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_POLL_ADDR_HI, upper_32_bits(vaBase + wptrOff) & 0xFFFFu);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_RPTR, 0);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_LO, 0);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_HI, 0);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_VMID, 0);
    tmp = regCP_HQD_PERSISTENT_STATE_DEFAULT;
    tmp = REG_SET_FIELD(tmp, CP_HQD_PERSISTENT_STATE, PRELOAD_SIZE, 0x55);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PERSISTENT_STATE, tmp);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_ACTIVE, 1);
    res->active_rd = RREG32_SOC15(RN_GC, 0, regCP_HQD_ACTIVE);
    res->pq_control_rd = RREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_CONTROL);
    res->mqd_base_rd = RREG32_SOC15(RN_GC, 0, regCP_MQD_BASE_ADDR);
    res->step = 2;

    rn_vram_wr32(adev, wptrOff, 8);                                          // 8 dwords, in the polled shadow and in the register
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_LO, 8);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_HI, 0);
    res->step = 3;
    static const u32 sampleDelayMs[8] = {0, 1, 1, 3, 5, 10, 30, 50};        // cumulative: 0, 1, 2, 5, 10, 20, 50, 100 ms
    for (u32 i = 0; i < 8; i++) {
        if (sampleDelayMs[i]) msleep(sampleDelayMs[i]);
        res->trace[i][0] = RREG32_SOC15(RN_GC, 0, regCP_MEC_RS64_CNTL);
        res->trace[i][1] = RREG32_SOC15(RN_GC, 0, regCP_HQD_ACTIVE);
        res->trace[i][2] = RREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_RPTR);
    }
    for (int i = 0; i < 2000; i++) {                                         // up to 2 s
        res->scratch = RREG32_SOC15(RN_GC, 0, regSCRATCH_REG0);
        if (res->scratch == 0xDEADBEEFu) { res->ok = 1; break; }
        msleep(1);
    }
    res->rptr = RREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_RPTR);
    res->wptr = RREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_LO);
    res->fault_status = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_STATUS);
    res->fault_addr_lo = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_LO32);
    res->fault_addr_hi = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_HI32);
    res->rptr_shadow = rn_vram_rd32(adev, rptrOff);
    res->ring0 = rn_vram_rd32(adev, ringOff);
    grbmSelect(adev, 0, 0, 0, 0);
}

#define RN_PACKET3_SET_SH_REG 0x76u
#define RN_PACKET3_SET_SH_REG_START 0x00002c00u
#define RN_PACKET3_DISPATCH_DIRECT 0x15u
#define RN_PACKET3_RELEASE_MEM 0x49u
#define RN_CACHE_FLUSH_AND_INV_TS_EVENT 0x14u

// Compute state registers (gc_11_0_x_offset.h, all in GC segment 0): the SET_SH_REG offset is the register's absolute dword minus 0x2c00.
#define RN_COMPUTE_START_X 0x1ba4u
#define RN_COMPUTE_NUM_THREAD_X 0x1ba7u
#define RN_COMPUTE_PGM_LO 0x1bacu
#define RN_COMPUTE_PGM_RSRC1 0x1bb2u
#define RN_COMPUTE_RESOURCE_LIMITS 0x1bb5u
#define RN_COMPUTE_PGM_RSRC3 0x1bc8u

void rn_compute_dispatch_test(struct rn_device *adev, u64 vaBase, u64 ringOff, u64 shaderOff, u64 fenceOff, u64 targetOff, u32 intSel,
                              struct rn_dispatch_result *res) {
    memset(res, 0, sizeof(*res));
    const u64 shaderVa = vaBase + shaderOff, fenceVa = vaBase + fenceOff;
    const u64 wptrOff = ringOff + 0x1008;
    const u32 gcBase = adev->reg_offset[RN_GC_HWIP][0][0];
    #define SHREG(reg) (gcBase + (reg) - RN_PACKET3_SET_SH_REG_START)

    // the shader: s_endpgm (GFX11 SOPP opcode 48) alone, or a store of one dword followed by s_endpgm; s_code_end padding up to 256 bytes
    u32 code[16];
    u32 nc = 0;
    if (targetOff) {
        const u64 targetVa = vaBase + targetOff;
        code[nc++] = 0xBE8000FFu; code[nc++] = lower_32_bits(targetVa);     // s_mov_b32 s0, <address low>
        code[nc++] = 0xBE8100FFu; code[nc++] = upper_32_bits(targetVa);     // s_mov_b32 s1, <address high>
        code[nc++] = 0x7E020280u;                                           // v_mov_b32 v1, 0           (offset)
        code[nc++] = 0x7E0402FFu; code[nc++] = 0x600DC0DEu;                 // v_mov_b32 v2, 0x600DC0DE  (data)
        code[nc++] = 0xDC6A0000u; code[nc++] = 0x00000201u;                 // global_store_b32 v1, v2, s[0:1]
        code[nc++] = 0xBC7C0000u;                                           // s_waitcnt_vscnt null, 0x0
        rn_vram_wr32(adev, targetOff, 0xDEAD0000u);
        res->target_before = rn_vram_rd32(adev, targetOff);
    }
    code[nc++] = 0xBFB00000u;                                               // s_endpgm
    for (u32 i = 0; i < nc; i++) rn_vram_wr32(adev, shaderOff + i * 4, code[i]);
    for (u32 o = nc * 4; o < 256; o += 4) rn_vram_wr32(adev, shaderOff + o, 0xBF9F0000u);
    rn_vram_wr32(adev, fenceOff, 0x0BADF00Du);
    res->fence_before = rn_vram_rd32(adev, fenceOff);

    u32 ring[64];
    u32 n = 0;
    ring[n++] = RN_PACKET3(RN_PACKET3_SET_SH_REG, 2); ring[n++] = SHREG(RN_COMPUTE_PGM_LO);
    ring[n++] = lower_32_bits(shaderVa >> 8); ring[n++] = upper_32_bits(shaderVa >> 8);
    ring[n++] = RN_PACKET3(RN_PACKET3_SET_SH_REG, 2); ring[n++] = SHREG(RN_COMPUTE_PGM_RSRC1);
    ring[n++] = 0x002C0000u;                                                 // float mode 0xc0, DX10_CLAMP
    ring[n++] = 0x00000080u;                                                 // TGID_X_EN
    ring[n++] = RN_PACKET3(RN_PACKET3_SET_SH_REG, 1); ring[n++] = SHREG(RN_COMPUTE_PGM_RSRC3); ring[n++] = 0;
    ring[n++] = RN_PACKET3(RN_PACKET3_SET_SH_REG, 3); ring[n++] = SHREG(RN_COMPUTE_START_X);
    ring[n++] = 0; ring[n++] = 0; ring[n++] = 0;
    ring[n++] = RN_PACKET3(RN_PACKET3_SET_SH_REG, 3); ring[n++] = SHREG(RN_COMPUTE_NUM_THREAD_X);
    ring[n++] = 1; ring[n++] = 1; ring[n++] = 1;
    ring[n++] = RN_PACKET3(RN_PACKET3_SET_SH_REG, 6); ring[n++] = SHREG(RN_COMPUTE_RESOURCE_LIMITS);
    ring[n++] = 0;                                                           // RESOURCE_LIMITS
    ring[n++] = 0xFFFFFFFFu;                                                 // STATIC_THREAD_MGMT_SE0
    ring[n++] = 0xFFFFFFFFu;                                                 // STATIC_THREAD_MGMT_SE1
    ring[n++] = 0;                                                           // TMPRING_SIZE
    ring[n++] = 0xFFFFFFFFu;                                                 // STATIC_THREAD_MGMT_SE2
    ring[n++] = 0xFFFFFFFFu;                                                 // STATIC_THREAD_MGMT_SE3
    ring[n++] = RN_PACKET3(RN_PACKET3_DISPATCH_DIRECT, 3);
    ring[n++] = 1; ring[n++] = 1; ring[n++] = 1;
    ring[n++] = 0x00000001u | 0x00000004u | 0x00000040u | 0x00008000u;      // COMPUTE_SHADER_EN | FORCE_START_AT_000 | ORDER_MODE | CS_W32_EN
    // RELEASE_MEM as gfx_v11_0_ring_emit_fence: flush, then write a 32-bit value to the fence address
    ring[n++] = RN_PACKET3(RN_PACKET3_RELEASE_MEM, 6);
    ring[n++] = (1u << 22) | (1u << 21) | (1u << 13) | (1u << 12) | (3u << 25) | RN_CACHE_FLUSH_AND_INV_TS_EVENT | (5u << 8);
    ring[n++] = (1u << 29) | (intSel << 24);                                 // DATA_SEL 1: 32-bit data; INT_SEL 2: interrupt once the write is confirmed
    ring[n++] = lower_32_bits(fenceVa); ring[n++] = upper_32_bits(fenceVa);
    ring[n++] = 0xF00DF00Du; ring[n++] = 0; ring[n++] = 0;
    while (((8 + n) & 7) != 0) { ring[n++] = RN_PACKET3(RN_PACKET3_NOP, 0); ring[n++] = 0; }   // the ring already holds 8 dwords
    res->step = 1;

    const u32 start = 8;                                                     // after the SCRATCH_REG0 packet of the first test
    for (u32 i = 0; i < n; i++) rn_vram_wr32(adev, ringOff + (start + i) * 4, ring[i]);
    grbmSelect(adev, 1, 0, 0, 2);
    rn_vram_wr32(adev, wptrOff, start + n);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_LO, start + n);
    WREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_HI, 0);
    res->step = 2;
    static const u32 sampleDelayMs[6] = {0, 1, 4, 15, 80, 400};              // cumulative: 0, 1, 5, 20, 100, 500 ms
    for (u32 i = 0; i < 6; i++) {
        if (sampleDelayMs[i]) msleep(sampleDelayMs[i]);
        res->trace[i][0] = RREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_RPTR);
        res->trace[i][1] = rn_vram_rd32(adev, fenceOff);
    }
    for (int i = 0; i < 1500; i++) {                                         // up to 1.5 s more
        res->fence = rn_vram_rd32(adev, fenceOff);
        if (res->fence == 0xF00DF00Du) { res->ok = 1; break; }
        msleep(1);
    }
    res->rptr = RREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_RPTR);
    res->wptr = RREG32_SOC15(RN_GC, 0, regCP_HQD_PQ_WPTR_LO);
    res->active = RREG32_SOC15(RN_GC, 0, regCP_HQD_ACTIVE);
    res->scratch = RREG32_SOC15(RN_GC, 0, regSCRATCH_REG0);
    if (targetOff) res->target = rn_vram_rd32(adev, targetOff);
    res->fault_status = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_STATUS);
    res->fault_addr_lo = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_LO32);
    res->fault_addr_hi = RREG32_SOC15(RN_GC, 0, regGCVM_L2_PROTECTION_FAULT_ADDR_HI32);
    grbmSelect(adev, 0, 0, 0, 0);
    #undef SHREG
}

void rn_ih_enable(struct rn_device *adev, u64 vaBase, u64 ringOff, u32 ringBytes, u64 wptrShadowOff) {
    u32 tmp;
    for (u32 o = 0; o < ringBytes; o += 4) rn_vram_wr32(adev, ringOff + o, 0);
    rn_vram_wr32(adev, wptrShadowOff, 0); rn_vram_wr32(adev, wptrShadowOff + 4, 0);

    // ih_v6_0_toggle_interrupts(false): ring and interrupts off, pointers to 0
    tmp = RREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RB_ENABLE, 0);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, ENABLE_INTR, 0);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL, tmp);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_RPTR, 0);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_WPTR, 0);

    // nbio_v4_3_ih_control: no dummy read (there is no MSI and no system dummy page here), and the ring is in VRAM, so non-snooped
    WREG32_SOC15(RN_NBIO, 0, regBIF_BX0_INTERRUPT_CNTL2, 0);
    tmp = RREG32_SOC15(RN_NBIO, 0, regBIF_BX0_INTERRUPT_CNTL);
    tmp = REG_SET_FIELD(tmp, BIF_BX0_INTERRUPT_CNTL, IH_DUMMY_RD_OVERRIDE, 1);
    tmp = REG_SET_FIELD(tmp, BIF_BX0_INTERRUPT_CNTL, IH_DUMMY_RD_EN, 0);
    tmp = REG_SET_FIELD(tmp, BIF_BX0_INTERRUPT_CNTL, IH_REQ_NONSNOOP_EN, 1);
    WREG32_SOC15(RN_NBIO, 0, regBIF_BX0_INTERRUPT_CNTL, tmp);

    // ih_v6_0_enable_ring
    const u64 ringVa = vaBase + ringOff;
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_BASE, lower_32_bits(ringVa >> 8));
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_BASE_HI, upper_32_bits(ringVa >> 8) & 0xff);
    u32 order = 0;
    while ((4u << order) < ringBytes) order++;                               // order_base_2(ring_size / 4)
    tmp = RREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, MC_SPACE, 4);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 1);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_OVERFLOW_ENABLE, 1);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RB_SIZE, order);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_WRITEBACK_ENABLE, 1);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, MC_SNOOP, 0);                       // VRAM
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, MC_RO, 0);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, MC_VMID, 0);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RPTR_REARM, 0);                     // no MSI
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL, tmp);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_WPTR_ADDR_LO, lower_32_bits(vaBase + wptrShadowOff));
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_WPTR_ADDR_HI, upper_32_bits(vaBase + wptrShadowOff) & 0xFFFFu);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_WPTR, 0);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_RPTR, 0);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_DOORBELL_RPTR, 0);                      // no doorbell for the read pointer

    // ih_v6_0_toggle_interrupts(true): clear the overflow bit by pulsing it, then enable the ring and the interrupts
    tmp = RREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, RB_ENABLE, 1);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 0);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL, tmp);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 1);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL, tmp);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 0);
    tmp = REG_SET_FIELD(tmp, IH_RB_CNTL, ENABLE_INTR, 1);
    WREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL, tmp);

    // gfx_v11_0_set_compute_eop_interrupt_state: the end-of-pipe timestamp interrupt of MEC 1 pipe 0 (the queue under test)
    tmp = RREG32_SOC15(RN_GC, 0, regCP_ME1_PIPE0_INT_CNTL);
    tmp = REG_SET_FIELD(tmp, CP_ME1_PIPE0_INT_CNTL, TIME_STAMP_INT_ENABLE, 1);
    WREG32_SOC15(RN_GC, 0, regCP_ME1_PIPE0_INT_CNTL, tmp);
}

void rn_ih_read(struct rn_device *adev, u64 ringOff, u64 wptrShadowOff, struct rn_ih_result *res) {
    memset(res, 0, sizeof(*res));
    res->rb_cntl_rd = RREG32_SOC15(RN_OSSSYS, 0, regIH_RB_CNTL);
    res->wptr_reg = RREG32_SOC15(RN_OSSSYS, 0, regIH_RB_WPTR);
    res->rptr_reg = RREG32_SOC15(RN_OSSSYS, 0, regIH_RB_RPTR);
    res->base_rd = RREG32_SOC15(RN_OSSSYS, 0, regIH_RB_BASE);
    res->cp_int_rd = RREG32_SOC15(RN_GC, 0, regCP_ME1_PIPE0_INT_CNTL);
    res->wptr_shadow = rn_vram_rd32(adev, wptrShadowOff);
    for (u32 i = 0; i < 16; i++) res->entry[i] = rn_vram_rd32(adev, ringOff + i * 4);
}

}
