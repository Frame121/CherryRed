#ifndef COBALT64_NAVI33_GC_LOAD_HPP
#define COBALT64_NAVI33_GC_LOAD_HPP

#include <stdint.h>

#include "Navi33PspLoad.hpp"

// Pure, host-testable pieces of the GC firmware stage: the image set Linux
// hands to the PSP with GFX_CMD_ID_LOAD_IP_FW for GC 11.0.2 / SDMA 6.0.2,
// in the AMDGPU_UCODE_ID order, ending with RLC_G (after which
// GFX_CMD_ID_AUTOLOAD_RLC starts the RLC autoload). No IOKit includes.
//
// Provenance:
//  - The GFX_FW_TYPE values and the per-header payload extraction are
//    amdgpu_ucode.h / amdgpu_gfx_cp_init_microcode / amdgpu_gfx_rlc_init_
//    microcode / imu_v11_0_init_microcode / mes_v11_0_init_microcode /
//    sdma_v6_0_init_microcode (Linux, MIT); the same table RedNavi's
//    gen_gc_fw_header.py encodes and verified on Navi 32 (GC 11.0.3).
//  - The extraction offsets were additionally validated against the real
//    hash-pinned GC 11.0.2 / SDMA 6.0.2 blobs (see
//    tests/test_Navi33GcFirmware.cpp): 26 images, RLC header v2.3,
//    CP/SDMA header v2.0, MES/IMU header v1.0.

namespace cobalt64::navi33::psp {

// GFX_FW_TYPE values (psp_gfx_if.h) this stage loads.
inline constexpr uint32_t kGfxFwTypeRlcV = 7u;
inline constexpr uint32_t kGfxFwTypeRlcG = 8u;
inline constexpr uint32_t kGfxFwTypeRlcGpmMem = 20u;
inline constexpr uint32_t kGfxFwTypeRlcSrmMem = 21u;
inline constexpr uint32_t kGfxFwTypeRlcSrmCntl = 22u;
inline constexpr uint32_t kGfxFwTypeRlcP = 25u;
inline constexpr uint32_t kGfxFwTypeRlcIram = 26u;
inline constexpr uint32_t kGfxFwTypeCpMes = 33u;
inline constexpr uint32_t kGfxFwTypeMesStack = 34u;
inline constexpr uint32_t kGfxFwTypeRlcDramBoot = 48u;
inline constexpr uint32_t kGfxFwTypeImuI = 68u;
inline constexpr uint32_t kGfxFwTypeImuD = 69u;
inline constexpr uint32_t kGfxFwTypeSdmaTh0 = 71u;
inline constexpr uint32_t kGfxFwTypeSdmaTh1 = 72u;
inline constexpr uint32_t kGfxFwTypeCpMesKiq = 81u;
inline constexpr uint32_t kGfxFwTypeMesKiqStack = 82u;
inline constexpr uint32_t kGfxFwTypeRs64Pfp = 87u;
inline constexpr uint32_t kGfxFwTypeRs64Me = 88u;
inline constexpr uint32_t kGfxFwTypeRs64Mec = 89u;
inline constexpr uint32_t kGfxFwTypeRs64PfpP0Stack = 90u;
inline constexpr uint32_t kGfxFwTypeRs64PfpP1Stack = 91u;
inline constexpr uint32_t kGfxFwTypeRs64MeP0Stack = 92u;
inline constexpr uint32_t kGfxFwTypeRs64MeP1Stack = 93u;
inline constexpr uint32_t kGfxFwTypeRs64MecP0Stack = 94u;
inline constexpr uint32_t kGfxFwTypeRs64MecP1Stack = 95u;
inline constexpr uint32_t kGfxFwTypeRs64MecP2Stack = 96u;
inline constexpr uint32_t kGfxFwTypeRs64MecP3Stack = 97u;

// The blobs the kext embeds (fw_gc_11_0_2_* / fw_sdma_6_0_2).
inline constexpr uint32_t kGcBlobPfp = 0;
inline constexpr uint32_t kGcBlobMe = 1;
inline constexpr uint32_t kGcBlobMec = 2;
inline constexpr uint32_t kGcBlobRlc = 3;
inline constexpr uint32_t kGcBlobImu = 4;
inline constexpr uint32_t kGcBlobMes = 5;
inline constexpr uint32_t kGcBlobMes1 = 6;
inline constexpr uint32_t kGcBlobSdma = 7;
inline constexpr uint32_t kGcBlobCount = 8;

struct GcBlobRef {
    const uint8_t *data = nullptr;
    uint32_t len = 0;
};

struct GcImage {
    const char *name = "?";
    uint32_t fwType = 0;
    uint32_t blobIndex = 0;
    uint32_t offsetBytes = 0;   // absolute offset inside the blob
    uint32_t sizeBytes = 0;
};

inline constexpr uint32_t kMaxGcImages = 32;

// gfx_firmware_header_v2_0 (CP RS64): ucode size at 36, data size at 44,
// data offset at 48.
struct Rs64Payloads {
    uint32_t ucodeOffset = 0;
    uint32_t ucodeSize = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;
};

inline bool parseRs64Header(const GcBlobRef &blob) noexcept
{
    if (!blob.data || blob.len < 64)
        return false;
    if (readU32Le(blob.data + 0) != blob.len)
        return false;
    if (readU16Le(blob.data + 8) != 2 || readU16Le(blob.data + 10) != 0)
        return false;  // gfx_firmware_header_v2_0 only
    Rs64Payloads p{readU32Le(blob.data + 24), readU32Le(blob.data + 36),
                   readU32Le(blob.data + 48), readU32Le(blob.data + 44)};
    const uint64_t end = static_cast<uint64_t>(p.dataOffset) + p.dataSize;
    return p.ucodeSize != 0 && p.dataSize != 0 &&
           static_cast<uint64_t>(p.ucodeOffset) + p.ucodeSize <= blob.len &&
           end <= blob.len;
}

inline bool parseMesHeaderV1(const GcBlobRef &blob) noexcept
{
    if (!blob.data || blob.len < 64)
        return false;
    if (readU32Le(blob.data + 0) != blob.len)
        return false;
    if (readU16Le(blob.data + 8) != 1 || readU16Le(blob.data + 10) != 0)
        return false;
    // header v1.0: ucode size/offset at 36/40, data size/offset at 48/52
    const uint32_t ucodeSize = readU32Le(blob.data + 36);
    const uint32_t ucodeOff = readU32Le(blob.data + 40);
    const uint32_t dataSize = readU32Le(blob.data + 48);
    const uint32_t dataOff = readU32Le(blob.data + 52);
    const uint64_t end1 = static_cast<uint64_t>(ucodeOff) + ucodeSize;
    const uint64_t end2 = static_cast<uint64_t>(dataOff) + dataSize;
    return ucodeSize != 0 && dataSize != 0 &&
           end1 <= blob.len && end2 <= blob.len;
}

inline bool parseImuHeaderV1(const GcBlobRef &blob) noexcept
{
    if (!blob.data || blob.len < 48)
        return false;
    if (readU32Le(blob.data + 0) != blob.len)
        return false;
    if (readU16Le(blob.data + 8) != 1 || readU16Le(blob.data + 10) != 0)
        return false;
    // iram size at 32, dram size at 40; the dram image follows the iram image
    // inside the ucode array (imu_v11_0_init_microcode).
    const uint32_t iramSize = readU32Le(blob.data + 32);
    const uint32_t dramSize = readU32Le(blob.data + 40);
    const uint32_t arrayOff = readU32Le(blob.data + 24);
    const uint64_t end = static_cast<uint64_t>(arrayOff) + iramSize + dramSize;
    return iramSize != 0 && dramSize != 0 && end <= blob.len;
}

inline bool parseRlcHeaderV23(const GcBlobRef &blob) noexcept
{
    if (!blob.data || blob.len < 208)
        return false;
    if (readU32Le(blob.data + 0) != blob.len)
        return false;
    if (readU16Le(blob.data + 8) != 2 || readU16Le(blob.data + 10) != 3)
        return false;  // the layout below is the v2.3 one; refuse others
    return true;
}

// Adds one image if sizeBytes > 0; bounds-checked against its blob.
inline bool addImage(GcImage (&out)[kMaxGcImages], uint32_t &count,
                     const char *name, uint32_t fwType,
                     const GcBlobRef (&blobs)[kGcBlobCount], uint32_t blobIndex,
                     uint32_t offsetBytes, uint32_t sizeBytes) noexcept
{
    if (sizeBytes == 0)
        return true;  // psp_*_fw.bin_desc.size_bytes == 0 in Linux: skipped
    if (count >= kMaxGcImages || blobIndex >= kGcBlobCount)
        return false;
    const GcBlobRef &blob = blobs[blobIndex];
    if (!blob.data || static_cast<uint64_t>(offsetBytes) + sizeBytes > blob.len)
        return false;
    out[count++] = GcImage{name, fwType, blobIndex, offsetBytes, sizeBytes};
    return true;
}

// Builds the LOAD_IP_FW plan. Returns 0 on any parse/bounds failure
// (fail-closed); otherwise the image count, with RLC_G LAST.
inline uint32_t buildGcImagePlan(const GcBlobRef (&blobs)[kGcBlobCount],
                                 GcImage (&out)[kMaxGcImages])
{
    uint32_t count = 0;
    if (!parseRs64Header(blobs[kGcBlobPfp]) || !parseRs64Header(blobs[kGcBlobMe]) ||
        !parseRs64Header(blobs[kGcBlobMec]) || !parseMesHeaderV1(blobs[kGcBlobMes]) ||
        !parseMesHeaderV1(blobs[kGcBlobMes1]) || !parseImuHeaderV1(blobs[kGcBlobImu]) ||
        !parseRlcHeaderV23(blobs[kGcBlobRlc]))
        return 0;

    const GcBlobRef &pfp = blobs[kGcBlobPfp];
    const GcBlobRef &me = blobs[kGcBlobMe];
    const GcBlobRef &mec = blobs[kGcBlobMec];
    const GcBlobRef &rlc = blobs[kGcBlobRlc];
    const GcBlobRef &imu = blobs[kGcBlobImu];
    const GcBlobRef &mes = blobs[kGcBlobMes];
    const GcBlobRef &mes1 = blobs[kGcBlobMes1];
    const GcBlobRef &sdma = blobs[kGcBlobSdma];

    // SDMA 6.0 (header v2.0): context thread, then control thread
    if (!addImage(out, count, "SDMA_UCODE_TH0", kGfxFwTypeSdmaTh0, blobs,
                  kGcBlobSdma, readU32Le(sdma.data + 24), readU32Le(sdma.data + 36)))
        return 0;
    if (!addImage(out, count, "SDMA_UCODE_TH1", kGfxFwTypeSdmaTh1, blobs,
                  kGcBlobSdma, readU32Le(sdma.data + 48), readU32Le(sdma.data + 52)))
        return 0;

    // CP RS64 + per-pipe stacks
    if (!addImage(out, count, "CP_RS64_PFP", kGfxFwTypeRs64Pfp, blobs, kGcBlobPfp,
                  readU32Le(pfp.data + 24), readU32Le(pfp.data + 36)) ||
        !addImage(out, count, "CP_RS64_PFP_P0_STACK", kGfxFwTypeRs64PfpP0Stack, blobs,
                  kGcBlobPfp, readU32Le(pfp.data + 48), readU32Le(pfp.data + 44)) ||
        !addImage(out, count, "CP_RS64_PFP_P1_STACK", kGfxFwTypeRs64PfpP1Stack, blobs,
                  kGcBlobPfp, readU32Le(pfp.data + 48), readU32Le(pfp.data + 44)) ||
        !addImage(out, count, "CP_RS64_ME", kGfxFwTypeRs64Me, blobs, kGcBlobMe,
                  readU32Le(me.data + 24), readU32Le(me.data + 36)) ||
        !addImage(out, count, "CP_RS64_ME_P0_STACK", kGfxFwTypeRs64MeP0Stack, blobs,
                  kGcBlobMe, readU32Le(me.data + 48), readU32Le(me.data + 44)) ||
        !addImage(out, count, "CP_RS64_ME_P1_STACK", kGfxFwTypeRs64MeP1Stack, blobs,
                  kGcBlobMe, readU32Le(me.data + 48), readU32Le(me.data + 44)) ||
        !addImage(out, count, "CP_RS64_MEC", kGfxFwTypeRs64Mec, blobs, kGcBlobMec,
                  readU32Le(mec.data + 24), readU32Le(mec.data + 36)))
        return 0;
    for (uint32_t n = 0; n < 4; ++n) {
        static const uint32_t kMecTypes[4] = {
            kGfxFwTypeRs64MecP0Stack, kGfxFwTypeRs64MecP1Stack,
            kGfxFwTypeRs64MecP2Stack, kGfxFwTypeRs64MecP3Stack};
        if (!addImage(out, count, "CP_RS64_MEC_STACK", kMecTypes[n], blobs,
                      kGcBlobMec, readU32Le(mec.data + 48), readU32Le(mec.data + 44)))
            return 0;
    }

    // MES scheduler pipe, then KIQ pipe (header v1.0)
    if (!addImage(out, count, "CP_MES", kGfxFwTypeCpMes, blobs, kGcBlobMes,
                  readU32Le(mes.data + 40), readU32Le(mes.data + 36)) ||
        !addImage(out, count, "CP_MES_DATA", kGfxFwTypeMesStack, blobs, kGcBlobMes,
                  readU32Le(mes.data + 52), readU32Le(mes.data + 48)) ||
        !addImage(out, count, "CP_MES1", kGfxFwTypeCpMesKiq, blobs, kGcBlobMes1,
                  readU32Le(mes1.data + 40), readU32Le(mes1.data + 36)) ||
        !addImage(out, count, "CP_MES1_DATA", kGfxFwTypeMesKiqStack, blobs,
                  kGcBlobMes1, readU32Le(mes1.data + 52), readU32Le(mes1.data + 48)))
        return 0;

    // IMU: iram then dram (dram directly follows iram in the ucode array)
    const uint32_t imuArrayOff = readU32Le(imu.data + 24);
    if (!addImage(out, count, "IMU_I", kGfxFwTypeImuI, blobs, kGcBlobImu,
                  imuArrayOff, readU32Le(imu.data + 32)) ||
        !addImage(out, count, "IMU_D", kGfxFwTypeImuD, blobs, kGcBlobImu,
                  imuArrayOff + readU32Le(imu.data + 32), readU32Le(imu.data + 40)))
        return 0;

    // RLC v2.3 restore list, then P/V, then RLC_G last (the autoload trigger
    // watches for type 8).
    if (!addImage(out, count, "RLC_RESTORE_LIST_SRM_CNTL", kGfxFwTypeRlcSrmCntl,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 120), readU32Le(rlc.data + 116)) ||
        !addImage(out, count, "RLC_RESTORE_LIST_GPM_MEM", kGfxFwTypeRlcGpmMem,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 136), readU32Le(rlc.data + 132)) ||
        !addImage(out, count, "RLC_RESTORE_LIST_SRM_MEM", kGfxFwTypeRlcSrmMem,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 152), readU32Le(rlc.data + 148)) ||
        !addImage(out, count, "RLC_IRAM", kGfxFwTypeRlcIram,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 160), readU32Le(rlc.data + 156)) ||
        !addImage(out, count, "RLC_DRAM", kGfxFwTypeRlcDramBoot,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 168), readU32Le(rlc.data + 164)) ||
        !addImage(out, count, "RLC_P", kGfxFwTypeRlcP,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 184), readU32Le(rlc.data + 180)) ||
        !addImage(out, count, "RLC_V", kGfxFwTypeRlcV,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 200), readU32Le(rlc.data + 196)) ||
        !addImage(out, count, "RLC_G", kGfxFwTypeRlcG,
                  blobs, kGcBlobRlc, readU32Le(rlc.data + 24), readU32Le(rlc.data + 20)))
        return 0;

    return count;
}

}  // namespace cobalt64::navi33::psp

#endif
