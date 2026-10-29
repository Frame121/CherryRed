#include <amd/rn_device.h>
#include <string.h>

extern "C" {

void rn_device_init(struct rn_device *adev, const char *name, volatile void *rmmio, size_t rmmio_size) {
    memset(adev, 0, sizeof(*adev));
    adev->dev.name = name;
    adev->rmmio = static_cast<volatile u8 *>(rmmio);
    adev->rmmio_size = rmmio_size;
    spin_lock_init(&adev->mmio_idx_lock);
}

// Registers inside the mapped BAR are read directly. Anything past its end goes through MM_INDEX/MM_DATA,
// which is what amdgpu does for registers above the BAR size.
u32 rn_rreg32(struct rn_device *adev, u32 reg) {
    size_t off = (size_t)reg * 4;
    if (off + 4 <= adev->rmmio_size) return readl(adev->rmmio + off);
    spin_lock(&adev->mmio_idx_lock);
    writel((u32)off, adev->rmmio + RN_MM_INDEX);
    u32 v = readl(adev->rmmio + RN_MM_DATA);
    spin_unlock(&adev->mmio_idx_lock);
    return v;
}

void rn_wreg32(struct rn_device *adev, u32 reg, u32 v) {
    size_t off = (size_t)reg * 4;
    if (off + 4 <= adev->rmmio_size) { writel(v, adev->rmmio + off); return; }
    spin_lock(&adev->mmio_idx_lock);
    writel((u32)off, adev->rmmio + RN_MM_INDEX);
    writel(v, adev->rmmio + RN_MM_DATA);
    spin_unlock(&adev->mmio_idx_lock);
}


u32 rn_vram_rd32(struct rn_device *adev, u64 off) {
    spin_lock(&adev->mmio_idx_lock);
    writel((u32)off | 0x80000000u, adev->rmmio + RN_MM_INDEX);
    writel((u32)(off >> 31), adev->rmmio + RN_MM_INDEX_HI);
    u32 v = readl(adev->rmmio + RN_MM_DATA);
    spin_unlock(&adev->mmio_idx_lock);
    return v;
}

void rn_vram_wr32(struct rn_device *adev, u64 off, u32 v) {
    spin_lock(&adev->mmio_idx_lock);
    writel((u32)off | 0x80000000u, adev->rmmio + RN_MM_INDEX);
    writel((u32)(off >> 31), adev->rmmio + RN_MM_INDEX_HI);
    writel(v, adev->rmmio + RN_MM_DATA);
    spin_unlock(&adev->mmio_idx_lock);
}

}
