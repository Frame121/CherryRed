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

// Program counter start of the RS64 CP firmware, from the firmware headers (gfx_firmware_header_v2_0.ucode_start_addr_lo/hi).
struct rn_sdma_run_result {
    u32 rptr, wptr, rptr_shadow;
    u32 fault_status, fault_addr_lo, fault_addr_hi;   // GCVM_L2_PROTECTION_FAULT_*
    int ok;                                           // the read pointer reached the end of the packets
};

struct rn_cp_start {
    u32 pfp_lo, pfp_hi, me_lo, me_hi, mec_lo, mec_hi;
};

struct rn_gfx_result {
    u32 step;                                 // 0 = not started, higher = how far it got
    u32 me_cntl_before, cp_stat_before;       // CP_ME_CNTL and CP_STAT before the CP is touched
    u32 me_cntl, cp_stat, rptr, wptr, scratch;
    u32 rptr_shadow, ring0;                   // memory read back after the test
    u32 trace[8][3];                          // CP_ME_CNTL, CP_STAT, CP_RB0_RPTR at 0, 1, 2, 5, 10, 20, 50, 100 ms after the wptr write
    u32 fault_status, fault_addr_lo, fault_addr_hi;   // GCVM_L2_PROTECTION_FAULT_*, after the test
    u32 me_cntl_imm, me_cntl_10ms;            // CP_ME_CNTL right after the unhalt write, and 10 ms later
    u32 rb0_cntl_rd, rb0_base_rd, rb0_base_hi_rd, rb_active_rd, max_ctx_rd, device_id_rd, pfp_pc_rd, me_pc_rd;   // read back of what was written
    int ok;
};

struct rn_compute_result {
    u32 step;
    u32 scratch, active_rd, pq_control_rd, mqd_base_rd, rptr, wptr, rptr_shadow, ring0;
    u32 trace[8][3];                          // CP_MEC_RS64_CNTL, CP_HQD_ACTIVE, CP_HQD_PQ_RPTR at 0, 1, 2, 5, 10, 20, 50, 100 ms after the wptr write
    u32 fault_status, fault_addr_lo, fault_addr_hi;
    int ok;
};

struct rn_dispatch_result {
    u32 step;
    u32 fence, fence_before, rptr, wptr, active, scratch;
    u32 target, target_before;                // the dword the shader stores to (0 if no store was asked for)
    u32 trace[6][2];                          // CP_HQD_PQ_RPTR and the fence at 0, 1, 5, 20, 100, 500 ms after the wptr write
    u32 fault_status, fault_addr_lo, fault_addr_hi;
    int ok;
};

struct rn_ih_result {
    u32 step;
    u32 rb_cntl_rd, wptr_reg, wptr_shadow, rptr_reg, base_rd, cp_int_rd;
    u32 entry[16];                            // the first two interrupt vectors of the ring (8 dwords each)
};

#ifdef __cplusplus
extern "C" {
#endif

// Programs the GC memory hub: system aperture on the VRAM range, VMID 0 enabled with its page table, L1 TLB and L2 cache.
void rn_gfxhub_enable(struct rn_device *adev, const struct rn_gmc *gmc);

// The VMID 0 page table (VRAM, at gmc.pt_vram_offset) maps the GART window gmc.gart_start..gart_end in 4 KB pages. rn_gfxhub_enable
// zeroes it; rn_gart_map_page then makes page `page` of the window point at a system memory page (bus address as the IOMMU or the
// physical address gives it: valid, system, snooped, readable, writeable). Do this before rn_gfxhub_enable, which also flushes the TLBs.
void rn_gart_map_page(struct rn_device *adev, const struct rn_gmc *gmc, u32 page, u64 bus_addr);
void rn_gart_clear(struct rn_device *adev, const struct rn_gmc *gmc);
// With the hub already on: change or remove a PTE (bus 0 = unmap), then flush VMID 0's TLBs through invalidation engine 17, the one
// Linux reserves for the GART (gmc_v11_0_flush_vm_hub). Returns 0 once the engine acknowledges, -1 on timeout.
int rn_gart_update_page(struct rn_device *adev, const struct rn_gmc *gmc, u32 page, u64 bus_addr);
int rn_gart_flush(struct rn_device *adev);

// Runs a packet list on SDMA0's gfx ring (the setup of rn_sdma0_test, reprogrammed) and waits for the read pointer to reach its end.
// Addresses inside the packets are whatever the SDMA can translate: MC addresses in the VRAM aperture, or GART addresses.
void rn_sdma0_run(struct rn_device *adev, u64 va_base, u64 ring_vram_offset, u32 ring_bytes, const u32 *pkt, u32 dwords,
                  struct rn_sdma_run_result *res);

// Starts SDMA0 on a ring of ring_bytes at ring_vram_offset (the read and write pointer shadows follow in the same page) and
// asks it to write a magic value to target_vram_offset. Everything lives in VRAM, so no GART mapping is needed.
// va_base is added to the offsets to form the addresses the SDMA uses: RN_VRAM_MC_BASE (system aperture) or 0 (the identity GART window).
void rn_sdma0_test(struct rn_device *adev, u64 va_base, u64 ring_vram_offset, u32 ring_bytes, u64 target_vram_offset, struct rn_sdma_result *res);

// First command on the graphics ring (gfx_v11_0_config_gfx_rs64, gfx_v11_0_cp_gfx_resume and gfx_v11_0_ring_test_ring): points the CP
// at its firmware entry, programs ring 0 of the gfx pipe 0 in VRAM (no doorbell, no MES) and asks it to write a magic value to
// SCRATCH_REG0 with SET_UCONFIG_REG. Needs rn_gfxhub_enable and the RLC booted. Written from the Linux sources, NOT yet run on hardware.
void rn_gfx_ring_test(struct rn_device *adev, u64 va_base, u64 ring_vram_offset, u32 ring_bytes, const struct rn_cp_start *pc,
                      struct rn_gfx_result *res);

// First command on a compute queue: the MEC pipes are activated and one hardware queue (MEC 1, pipe 0, queue 2) is programmed straight
// into the CP_HQD_* registers the way gfx_v11_0_kiq_init_register does for the KIQ, then SET_UCONFIG_REG writes SCRATCH_REG0.
// Needs rn_gfx_ring_test to have run first (it points the MEC at its firmware). Written from the Linux sources.
void rn_compute_ring_test(struct rn_device *adev, u64 va_base, u64 ring_vram_offset, u32 ring_bytes, struct rn_compute_result *res);

// First shader dispatch: on the compute queue that rn_compute_ring_test left active, a shader that is a single s_endpgm is set up
// (SET_SH_REG: program address, resources, thread counts) and run as one workgroup of one thread (DISPATCH_DIRECT), followed by a
// RELEASE_MEM that writes 0xF00DF00D to a fence in VRAM when the dispatch is done. Written from nvd.h and the register headers, and
// from the packet layout of gfx_v11_0_ring_emit_fence; NOT yet run on hardware.
// With target_vram_offset nonzero the shader is not just s_endpgm: it stores 0x600DC0DE to that VRAM dword (global_store_b32, the
// instruction words hand-assembled from the encodings in LLVM's MC tests, llvm/test/MC/AMDGPU/gfx11_asm_*.s).
void rn_compute_dispatch_test(struct rn_device *adev, u64 va_base, u64 ring_vram_offset, u64 shader_vram_offset, u64 fence_vram_offset,
                              u64 target_vram_offset, u64 target_gpu_va, volatile u32 *host_target, u32 int_sel,
                              int (*wait_done)(void *ctx, u32 timeout_ms), void *wait_ctx, struct rn_dispatch_result *res);
// (With wait_done set, the fence is not polled: after the submission the caller's function waits, e.g. for the interrupt, and the fence
// is read once afterwards.)
// (With target_gpu_va and host_target set, the shader stores to that GPU address instead - a GART-mapped host page - and the result is
// read from host_target; target_vram_offset is then ignored.)

// Interrupt handler (ih_v6_0_irq_init, simplified): ring 0 of OSSSYS in VRAM, its write pointer written back to memory, no MSI and no
// doorbell; the CP's end-of-pipe timestamp interrupt for MEC pipe 0 is enabled. A RELEASE_MEM with int_sel 2 then makes the CP
// push an interrupt vector into the ring, which rn_ih_read shows. Written from the Linux sources; NOT yet run on hardware.
// Doorbells: the NBIO doorbell aperture is switched on, and while rn_set_doorbell has a base, the compute queue is programmed with a
// doorbell (CP_HQD_PQ_DOORBELL_CONTROL, wptr polling off) and every submission is a 64-bit write of the wptr to that doorbell instead of a
// register write. base is the mapped doorbell BAR (BAR2), dword_index the queue's doorbell in dwords (mec_ring0 << 1 = 6 here).
// before[0..2] receive DOORBELL_APER_EN, S2A_DOORBELL_ENTRY_0_CTRL and _3_CTRL as they were; APER_EN is only written if it was 0, and the
// two S2A entries (nbio_v4_3_gc_doorbell_init) only if write_s2a is set.
void rn_doorbell_aperture_enable(struct rn_device *adev, int write_s2a, u32 before[3]);
void rn_set_doorbell(volatile void *base, u32 dword_index);

// msi: IH_RB_CNTL.RPTR_REARM, the ring re-arms the message-signalled interrupt when the read pointer is written.
void rn_ih_enable(struct rn_device *adev, u64 va_base, u64 ring_vram_offset, u32 ring_bytes, u64 wptr_shadow_vram_offset, int msi);
// Acknowledge: the read pointer is set to the current write pointer (which re-arms the next MSI). Returns the write pointer.
u32 rn_ih_ack(struct rn_device *adev, u32 ring_bytes);
void rn_ih_read(struct rn_device *adev, u64 ring_vram_offset, u64 wptr_shadow_vram_offset, struct rn_ih_result *res);

#ifdef __cplusplus
}
#endif

#endif
