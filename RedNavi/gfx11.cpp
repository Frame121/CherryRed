#include "gfx11.h"

#include <string.h>

#include <linux/delay.h>
#include "gc_regs.h"

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

}
