#!/usr/bin/env python3
"""Generate the GC 11 register constants the Navi 33 memory-hub and SDMA
stages need, from the real Linux asic_reg headers (git-ignored input).

    python3 tools/gen-navi33-gc-regs.py <gc_11_0_0_offset.h> <gc_11_0_0_sh_mask.h> src/navi33-bringup/gc11_regs.h

GC 11.0.2 (RX 7600) uses the gc_11_0_0 register header; only Navi 32
(GC 11.0.3) has its own (gfxhub_v3_0_3 differs from gfxhub v3.0.0/2 — the
hard-won fact in RedNavi's notes). The source headers are MIT-licensed Linux
files; this generator records the source commit in the output provenance.

Every requested register and field must resolve or the generator fails —
a missing register is a hard error, never a guess.
"""
import re
import sys

# Registers the memory-hub (gfxhub) and SDMA stages touch. name -> used as
# kReg<CamelCase> in the generated header.
REGISTERS = [
    "regGCMC_VM_FB_LOCATION_BASE",
    "regGCMC_VM_FB_LOCATION_TOP",
    "regGCMC_VM_AGP_BASE",
    "regGCMC_VM_AGP_BOT",
    "regGCMC_VM_AGP_TOP",
    "regGCMC_VM_SYSTEM_APERTURE_LOW_ADDR",
    "regGCMC_VM_SYSTEM_APERTURE_HIGH_ADDR",
    "regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB",
    "regGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB",
    "regGCMC_VM_MX_L1_TLB_CNTL",
    "regGCVM_L2_CNTL",
    "regGCVM_L2_CNTL2",
    "regGCVM_L2_CNTL3",
    "regGCVM_L2_CNTL4",
    "regGCVM_L2_CNTL5",
    "regGCVM_CONTEXT0_CNTL",
    "regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32",
    "regGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_HI32",
    "regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_LO32",
    "regGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_HI32",
    "regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_LO32",
    "regGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_HI32",
    "regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_LO32",
    "regGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_HI32",
    "regGCVM_L2_PROTECTION_FAULT_CNTL2",
    "regGCVM_L2_PROTECTION_FAULT_STATUS",
    "regGCVM_L2_PROTECTION_FAULT_ADDR_LO32",
    "regGCVM_L2_PROTECTION_FAULT_ADDR_HI32",
    "regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_LO32",
    "regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_HI32",
    "regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_LO32",
    "regGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_HI32",
    "regGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_LO32",
    "regGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_HI32",
    "regSDMA0_SEM_WAIT_FAIL_TIMER_CNTL",
    "regSDMA0_QUEUE0_RB_CNTL",
    "regSDMA0_QUEUE0_RB_RPTR",
    "regSDMA0_QUEUE0_RB_RPTR_HI",
    "regSDMA0_QUEUE0_RB_WPTR",
    "regSDMA0_QUEUE0_RB_WPTR_HI",
    "regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_LO",
    "regSDMA0_QUEUE0_RB_WPTR_POLL_ADDR_HI",
    "regSDMA0_QUEUE0_RB_RPTR_ADDR_HI",
    "regSDMA0_QUEUE0_RB_RPTR_ADDR_LO",
    "regSDMA0_QUEUE0_RB_BASE",
    "regSDMA0_QUEUE0_RB_BASE_HI",
    "regSDMA0_QUEUE0_MINOR_PTR_UPDATE",
    "regSDMA0_QUEUE0_DOORBELL",
    "regSDMA0_QUEUE0_IB_CNTL",
    "regSDMA0_QUEUE0_CONTEXT_STATUS",
    "regSDMA0_UTCL1_CNTL",
    "regSDMA0_F32_CNTL",
    "regSDMA0_STATUS_REG",
    "regSDMA0_STATUS1_REG",
    "regSDMA0_VERSION",
    "regSDMA0_CNTL",
    "regSDMA0_POWER_CNTL",
    "regSDMA0_UCODE_CHECKSUM",
    "regSDMA0_FREEZE",
    "regSDMA0_CHICKEN_BITS",
    "regSDMA0_PROGRAM",
]

# (register, field) pairs the stages set via the REG_SET_FIELD equivalent.
FIELDS = [
    ("regGCVM_L2_PROTECTION_FAULT_CNTL2", "ACTIVE_PAGE_MIGRATION_PTE_READ_RETRY"),
    ("regGCVM_L2_PROTECTION_FAULT_CNTL2", "ENABLE_RETRY_FAULT_INTERRUPT"),
    ("regGCMC_VM_MX_L1_TLB_CNTL", "ENABLE_L1_TLB"),
    ("regGCMC_VM_MX_L1_TLB_CNTL", "SYSTEM_ACCESS_MODE"),
    ("regGCMC_VM_MX_L1_TLB_CNTL", "ENABLE_ADVANCED_DRIVER_MODEL"),
    ("regGCMC_VM_MX_L1_TLB_CNTL", "SYSTEM_APERTURE_UNMAPPED_ACCESS"),
    ("regGCMC_VM_MX_L1_TLB_CNTL", "ECO_BITS"),
    ("regGCMC_VM_MX_L1_TLB_CNTL", "MTYPE"),
    ("regGCVM_L2_CNTL", "ENABLE_L2_CACHE"),
    ("regGCVM_L2_CNTL", "ENABLE_L2_FRAGMENT_PROCESSING"),
    ("regGCVM_L2_CNTL", "ENABLE_DEFAULT_PAGE_OUT_TO_SYSTEM_MEMORY"),
    ("regGCVM_L2_CNTL", "L2_PDE0_CACHE_TAG_GENERATION_MODE"),
    ("regGCVM_L2_CNTL", "PDE_FAULT_CLASSIFICATION"),
    ("regGCVM_L2_CNTL", "CONTEXT1_IDENTITY_ACCESS_MODE"),
    ("regGCVM_L2_CNTL", "IDENTITY_MODE_FRAGMENT_SIZE"),
    ("regGCVM_L2_CNTL2", "INVALIDATE_ALL_L1_TLBS"),
    ("regGCVM_L2_CNTL2", "INVALIDATE_L2_CACHE"),
    ("regGCVM_L2_CNTL3", "BANK_SELECT"),
    ("regGCVM_L2_CNTL3", "L2_CACHE_BIGK_FRAGMENT_SIZE"),
    ("regGCVM_L2_CNTL4", "VMC_TAP_PDE_REQUEST_PHYSICAL"),
    ("regGCVM_L2_CNTL4", "VMC_TAP_PTE_REQUEST_PHYSICAL"),
    ("regGCVM_L2_CNTL5", "L2_CACHE_SMALLK_FRAGMENT_SIZE"),
    ("regGCVM_CONTEXT0_CNTL", "ENABLE_CONTEXT"),
    ("regGCVM_CONTEXT0_CNTL", "PAGE_TABLE_DEPTH"),
    ("regGCVM_CONTEXT0_CNTL", "RETRY_PERMISSION_OR_INVALID_PAGE_FAULT"),
    ("regSDMA0_QUEUE0_RB_CNTL", "RB_SIZE"),
    ("regSDMA0_QUEUE0_RB_CNTL", "RB_PRIV"),
    ("regSDMA0_QUEUE0_RB_CNTL", "RPTR_WRITEBACK_ENABLE"),
    ("regSDMA0_QUEUE0_RB_CNTL", "WPTR_POLL_ENABLE"),
    ("regSDMA0_QUEUE0_RB_CNTL", "F32_WPTR_POLL_ENABLE"),
    ("regSDMA0_QUEUE0_RB_CNTL", "RB_ENABLE"),
    ("regSDMA0_QUEUE0_DOORBELL", "ENABLE"),
    ("regSDMA0_QUEUE0_IB_CNTL", "IB_ENABLE"),
    ("regSDMA0_UTCL1_CNTL", "RESP_MODE"),
    ("regSDMA0_UTCL1_CNTL", "REDO_DELAY"),
    ("regSDMA0_F32_CNTL", "HALT"),
    ("regSDMA0_F32_CNTL", "TH1_RESET"),
]


def snake_to_camel(name):
    parts = name.split("_")
    return parts[0] + "".join(p.capitalize() for p in parts[1:])


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    offset_path, mask_path, out_path = sys.argv[1:4]

    offset_text = open(offset_path, encoding="utf-8", errors="replace").read()
    mask_text = open(mask_path, encoding="utf-8", errors="replace").read()

    def reg_value(name):
        m = re.search(r"^#define\s+%s\s+(0x[0-9A-Fa-f]+)\s*$" % re.escape(name),
                      offset_text, re.M)
        if not m:
            raise SystemExit(f"register {name} not found in {offset_path}")
        return int(m.group(1), 16)

    def reg_base_idx(name):
        m = re.search(r"^#define\s+%s_BASE_IDX\s+(\d+)\s*$" % re.escape(name),
                      offset_text, re.M)
        if not m:
            raise SystemExit(f"register {name}_BASE_IDX not found in {offset_path}")
        return int(m.group(1))

    def field_mask(reg, field):
        # The MASK defines are pre-shifted full masks, and the sh_mask file
        # names them WITHOUT the "reg" prefix (GCVM_L2_CNTL__ENABLE_L2_CACHE_MASK).
        bare = reg[len("reg"):]
        m = re.search(r"^#define\s+%s__%s_MASK\s+(0x[0-9A-Fa-f]+)L?\s*$"
                      % (re.escape(bare), re.escape(field)), mask_text, re.M)
        if not m:
            raise SystemExit(f"field {bare}__{field}_MASK not found in {mask_path}")
        return int(m.group(1), 16)

    lines = []
    lines.append("// Generated by tools/gen-navi33-gc-regs.py from the Linux gc_11_0_0")
    lines.append("// asic_reg headers (MIT) — the register file GC 11.0.2 uses. Do not edit;")
    lines.append("// regenerate. Source headers are git-ignored; provenance: torvalds/linux")
    lines.append("// master 36aad21d50d1 at generation time.")
    lines.append("#pragma once")
    lines.append("")
    lines.append("namespace cobalt64::navi33::gc11 {")
    lines.append("")
    lines.append("// Register dword offsets and their IP base index (SOC15 addressing:")
    lines.append("// byte = (base_dwords[baseIdx] + dword) * 4).")
    for name in REGISTERS:
        value = reg_value(name)
        idx = reg_base_idx(name)
        camel = snake_to_camel(name[len("reg"):])
        lines.append(f"inline constexpr uint32_t kReg{camel} = 0x{value:x}u;")
        lines.append(f"inline constexpr uint32_t kReg{camel}BaseIdx = {idx}u;")
    lines.append("")
    lines.append("// Pre-shifted field masks + their shifts (the REG_SET_FIELD inputs).")
    for reg, field in FIELDS:
        mask = field_mask(reg, field)
        shift = (mask & -mask).bit_length() - 1
        camel_reg = snake_to_camel(reg[len("reg"):])
        lines.append(f"inline constexpr uint32_t k{camel_reg}{field}Mask = 0x{mask:x}u;")
        lines.append(f"inline constexpr uint32_t k{camel_reg}{field}Shift = {shift}u;")
    lines.append("")
    lines.append("// REG_SET_FIELD equivalent (Linux amdgpu bitfields.h semantics: the")
    lines.append("// MASK defines are already shifted).")
    lines.append("inline constexpr uint32_t setField(uint32_t orig, uint32_t mask,")
    lines.append("                                  uint32_t shift, uint32_t value) noexcept {")
    lines.append("    return (orig & ~mask) | ((value << shift) & mask);")
    lines.append("}")
    lines.append("")
    lines.append("}  // namespace cobalt64::navi33::gc11")
    lines.append("")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"wrote {out_path}: {len(REGISTERS)} registers, {len(FIELDS)} fields")
    return 0


if __name__ == "__main__":
    sys.exit(main())
