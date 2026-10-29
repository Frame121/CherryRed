// GC 11 memory hub and SDMA 6 bring-up written against the compat layer (struct rn_device, RREG32_SOC15). The sequences follow
// Linux: gfxhub_v3_0_gart_enable and sdma_v6_0_gfx_resume_instance / sdma_v6_0_ring_test_ring.
#ifndef REDNAVI_GFX11_H
#define REDNAVI_GFX11_H

#include <amd/rn_device.h>

#define RN_VRAM_MC_BASE 0x8000000000ull       // MC address of the start of VRAM on this card (MMHUB FB_LOCATION_BASE 0x8000)

struct rn_gmc {
    u64 fb_start, fb_end;                     // VRAM in MC addresses
    u64 gart_start, gart_end;                 // GART aperture (VMID 0 page table)
    u64 pt_vram_offset;                       // VRAM offset of the VMID 0 page table; rn_gfxhub_enable fills it with PTEs mapping
                                              // each GART page to the VRAM offset equal to its address (identity window)
    u64 scratch_vram_offset;                  // pages used as default and fault pages
    u64 dummy_vram_offset;
};

struct rn_sdma_result {
    u32 step;                                 // 0 = not started, higher = how far it got
    u32 rptr, wptr, f32_cntl, rb_cntl, value; // registers/memory read after the test
    u32 pre_value;                            // the target right after it was filled, to check the VRAM path itself
    u32 fault_status, fault_addr_lo, fault_addr_hi;   // GCVM_L2_PROTECTION_FAULT_*
    u32 sdma_status, sdma_status1, ctx_status;
    u32 version, cntl, power_cntl, checksum, freeze, chicken, program;   // is the block reachable at all?
    u32 ring0, ring3, rptr_shadow, wptr_shadow;                          // memory read back after the test
    int ok;
};

#ifdef __cplusplus
extern "C" {
#endif

// Programs the GC memory hub: system aperture on the VRAM range, VMID 0 enabled with its page table, L1 TLB and L2 cache.
void rn_gfxhub_enable(struct rn_device *adev, const struct rn_gmc *gmc);

// Starts SDMA0 on a ring of ring_bytes at ring_vram_offset (the read and write pointer shadows follow in the same page) and
// asks it to write a magic value to target_vram_offset. Everything lives in VRAM, so no GART mapping is needed.
// va_base is added to the offsets to form the addresses the SDMA uses: RN_VRAM_MC_BASE (system aperture) or 0 (the identity GART window).
void rn_sdma0_test(struct rn_device *adev, u64 va_base, u64 ring_vram_offset, u32 ring_bytes, u64 target_vram_offset, struct rn_sdma_result *res);

#ifdef __cplusplus
}
#endif

#endif
