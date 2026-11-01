#include "gc_regs_navi33.h"
#include <stdio.h>

#define CHECK_EQ(actual, expected, label) do { \
    if ((actual) != (expected)) { \
        fprintf(stderr, "FAIL: %s: got 0x%x expected 0x%x\\n", label, \
                (unsigned)(actual), (unsigned)(expected)); \
        ++failures; \
    } \
} while (0)

int main() {
    int failures = 0;
    // Offsets emitted from Linux gc_11_0_0_offset.h; Navi 33 reports GC 11.0.2.
    CHECK_EQ(regGCMC_VM_AGP_BASE, 0x167c, "GCMC_VM_AGP_BASE");
    CHECK_EQ(regGCMC_VM_AGP_BOT, 0x167b, "GCMC_VM_AGP_BOT");
    CHECK_EQ(regGCMC_VM_SYSTEM_APERTURE_LOW_ADDR, 0x167d, "SYSTEM_APERTURE_LOW");
    CHECK_EQ(regGCVM_L2_CNTL, 0x15bc, "GCVM_L2_CNTL");
    CHECK_EQ(regGCVM_L2_CNTL3, 0x15be, "GCVM_L2_CNTL3");
    CHECK_EQ(regGCVM_CONTEXT0_CNTL, 0x1688, "GCVM_CONTEXT0_CNTL");
    CHECK_EQ(regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32, 0x16f3, "VM_PAGE_TABLE_BASE_LO");
    CHECK_EQ(regGCVM_L2_PROTECTION_FAULT_STATUS, 0x15c8, "VM_FAULT_STATUS");
    CHECK_EQ(regGCVM_L2_PROTECTION_FAULT_ADDR_LO32, 0x15c9, "VM_FAULT_ADDR_LO");
    CHECK_EQ(regRLC_CNTL, 0x4c00, "RLC_CNTL");
    CHECK_EQ(regRLC_GPM_STAT, 0x4e6b, "RLC_GPM_STAT");
    CHECK_EQ(regRLC_RLCS_BOOTLOAD_STATUS, 0x4e82, "RLC_BOOTLOAD_STATUS");
    CHECK_EQ(regGRBM_STATUS, 0x0da4, "GRBM_STATUS");
    CHECK_EQ(regSDMA0_F32_CNTL_BASE_IDX, 1, "SDMA register base segment");
    CHECK_EQ(regGCVM_L2_CNTL3_DEFAULT, 0x80120007u, "GC 11.0.0 L2 control default");
    CHECK_EQ(GCVM_L2_CNTL__ENABLE_L2_CACHE_MASK, 0x1u, "L2 enable mask");
    CHECK_EQ(GCVM_L2_CNTL3__BANK_SELECT_MASK, 0x3fu, "L2 bank select mask");
    CHECK_EQ(GCVM_L2_CNTL3__L2_CACHE_BIGK_FRAGMENT_SIZE_MASK, 0x000f8000u, "L2 fragment mask");
    if (failures) {
        fprintf(stderr, "Navi 33 GC register tests: %d failure(s)\\n", failures);
        return 1;
    }
    puts("Navi 33 GC register tests: PASS (17 offset/mask/default checks)");
    return 0;
}
