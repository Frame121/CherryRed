#ifndef COBALT64_NAVI33_PSP_LOAD_HPP
#define COBALT64_NAVI33_PSP_LOAD_HPP

#include <stdint.h>

// Pure, host-testable pieces of the PSP 13.0.7 firmware-load stage (C1, the
// first WRITE stage of the compute-first plan). No IOKit includes: this header
// compiles both in the kext and in the host unit tests.
//
// Provenance of every constant here:
//  - The PSP firmware header layout (common_firmware_header +
//    psp_firmware_header_v2_0 / psp_fw_bin_desc) and the bootloader command
//    codes are Linux amdgpu_ucode.h / amdgpu_psp.h (MIT); the load order is
//    psp_hw_start() in amdgpu_psp.c, and the per-component mailbox handshake is
//    psp_v13_0_bootloader_load_component() / _load_sos() in psp_v13_0.c
//    (mp_13_0_2_offset.h register file, shared by PSP 13.0.7).
//  - The psp_13_0_7_sos.bin component table (5 bins: SYS_DRV, SOS, TOC, KDB,
//    SPL) was read from the blob's documented v2.0 header — metadata only,
//    not firmware code. Sizes are contiguous and end exactly at
//    ucode_size_bytes.
//  - MP0/MMHUB base addresses are instance-0 base[0] of hardware IDs 255/34
//    in this card's captured IP Discovery table (RX7600-Linux-Evidence/,
//    SHA-256 4b809b32…; same values in RedNavi-collab-review/docs/logs/navi33/
//    ip_discovery.txt). Discovery bases are dword units: byte = (base+dword)*4.
//  - The MM_INDEX/MM_DATA window addressing (BAR5 byte 0x00/0x04/0x18,
//    index = vramByte | 0x80000000, hi = vramByte >> 31) is the sequence
//    RedNavi verified on a real RX 7800 XT (Navi 32); the Navi 33 behaviour is
//    UNVERIFIED until this stage runs on the card.

namespace cobalt64::navi33::psp {

// --- PSP firmware header v2.0 (amdgpu_ucode.h) -----------------------------
inline constexpr uint32_t kCommonHeaderBytes = 32;   // common_firmware_header
inline constexpr uint32_t kV2CountOffset = 32;       // psp_fw_bin_count
inline constexpr uint32_t kBinDescStride = 16;       // psp_fw_bin_desc
inline constexpr uint32_t kMaxBins = 16;

enum FwType : uint32_t {
    FwUnknown = 0,   // PSP_FW_TYPE_PSP_UNKNOWN
    FwSos = 1,       // PSP_FW_TYPE_PSP_SOS
    FwSysDrv = 2,    // PSP_FW_TYPE_PSP_SYS_DRV
    FwKdb = 3,       // PSP_FW_TYPE_PSP_KDB
    FwToc = 4,       // PSP_FW_TYPE_PSP_TOC
    FwSpl = 5,       // PSP_FW_TYPE_PSP_SPL
    FwRl = 6,        // PSP_FW_TYPE_PSP_RL
    FwSocDrv = 7,    // PSP_FW_TYPE_PSP_SOC_DRV
    FwIntfDrv = 8,   // PSP_FW_TYPE_PSP_INTF_DRV
    FwDbgDrv = 9,    // PSP_FW_TYPE_PSP_DBG_DRV
    FwRasDrv = 10,   // PSP_FW_TYPE_PSP_RAS_DRV
};

// psp_bootloader_cmd (amdgpu_psp.h)
inline constexpr uint32_t kBlLoadSysDrv     = 0x10000u;
inline constexpr uint32_t kBlLoadSosDrv     = 0x20000u;
inline constexpr uint32_t kBlLoadKeyDb      = 0x80000u;
inline constexpr uint32_t kBlLoadSocDrv     = 0xB0000u;
inline constexpr uint32_t kBlLoadDbgDrv     = 0xC0000u;
inline constexpr uint32_t kBlLoadIntfDrv    = 0xD0000u;
inline constexpr uint32_t kBlLoadRasDrv     = 0xE0000u;
inline constexpr uint32_t kBlLoadTosSplTab  = 0x10000000u;

struct PspBinDesc {
    uint32_t fwType = 0;
    uint32_t fwVersion = 0;
    uint32_t offsetBytes = 0;   // payload offset from the start of the file
    uint32_t sizeBytes = 0;
};

struct SosImage {
    uint32_t ucodeVersion = 0;
    uint32_t ucodeSize = 0;       // ucode_size_bytes
    uint32_t arrayOffset = 0;     // ucode_array_offset_bytes
    uint32_t binCount = 0;
    PspBinDesc bins[kMaxBins];
};

inline constexpr uint32_t readU32Le(const uint8_t* p) noexcept
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline constexpr uint16_t readU16Le(const uint8_t* p) noexcept
{
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

// Bounds-checked parser for the v2.0 SOS header. Every field is validated:
// a truncated header, a bogus count, or a component whose payload escapes the
// declared ucode array makes the whole image invalid (fail-closed).
inline bool parseSosHeaderV2(const uint8_t* data, uint32_t dataLen, SosImage& out)
{
    out = SosImage();
    if (!data || dataLen < kCommonHeaderBytes)
        return false;

    const uint32_t declaredSize = readU32Le(data + 0);
    const uint32_t headerSize   = readU32Le(data + 4);
    const uint16_t major        = readU16Le(data + 8);
    const uint16_t minor        = readU16Le(data + 10);
    const uint32_t ucodeVer     = readU32Le(data + 16);
    const uint32_t ucodeSize    = readU32Le(data + 20);
    const uint32_t arrayOff     = readU32Le(data + 24);

    // Only the v2.0 layout is handled (13.0.7 uses it; v1 headers would be a
    // different load path and are refused here rather than misparsed).
    if (major != 2 || minor != 0)
        return false;
    if (headerSize < kCommonHeaderBytes + 4 || declaredSize != dataLen)
        return false;
    if (arrayOff > declaredSize || ucodeSize > declaredSize - arrayOff)
        return false;

    const uint32_t count = readU32Le(data + kV2CountOffset);
    if (count == 0 || count > kMaxBins)
        return false;
    const uint64_t tableEnd = static_cast<uint64_t>(kV2CountOffset + 4) +
                              static_cast<uint64_t>(count) * kBinDescStride;
    if (tableEnd > declaredSize)
        return false;

    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t base = kV2CountOffset + 4 + i * kBinDescStride;
        const uint32_t fwType  = readU32Le(data + base + 0);
        const uint32_t version = readU32Le(data + base + 4);
        const uint32_t off     = readU32Le(data + base + 8);
        const uint32_t size    = readU32Le(data + base + 12);
        if (fwType < FwSos || fwType > FwRasDrv || size == 0)
            return false;
        // Bin offsets are relative to ucode_array_offset_bytes (verified on
        // the real 13.0.7 blob: arrayOff + last offset + size == file size).
        const uint64_t payloadEnd = static_cast<uint64_t>(off) + size;
        if (payloadEnd > ucodeSize)
            return false;
        if (static_cast<uint64_t>(arrayOff) + payloadEnd >
            static_cast<uint64_t>(declaredSize))
            return false;
        out.bins[i] = {fwType, version, off, size};
    }

    out.ucodeVersion = ucodeVer;
    out.ucodeSize = ucodeSize;
    out.arrayOffset = arrayOff;
    out.binCount = count;
    return true;
}

// First bin of the given type, or -1.
inline int findBin(const SosImage& img, uint32_t fwType) noexcept
{
    for (uint32_t i = 0; i < img.binCount; ++i)
        if (img.bins[i].fwType == fwType)
            return static_cast<int>(i);
    return -1;
}

struct LoadStep {
    uint32_t cmd = 0;
    uint32_t fileOffset = 0;   // absolute file offset of the payload bytes
    uint32_t sizeBytes = 0;
    uint32_t version = 0;
    uint32_t fwType = FwUnknown;
    const char* name = "?";
};

inline constexpr uint32_t kMaxLoadSteps = 8;

// psp_hw_start() order (amdgpu_psp.c): KDB, SPL (the KDB image again, command
// LOAD_TOS_SPL_TABLE — psp_v13_0_bootloader_load_spl uses psp->kdb),
// SYS_DRV, SOC_DRV, INTF_DRV, DBG_DRV, RAS_DRV, then SOS last. Components the
// file does not carry are skipped (their psp_*_fw.bin_desc.size_bytes is 0 in
// Linux). Returns the number of steps; 0 = nothing loadable (refuse).
inline uint32_t buildLoadPlan(const SosImage& img, LoadStep (&plan)[kMaxLoadSteps])
{
    uint32_t n = 0;
    auto addBin = [&](uint32_t fwType, uint32_t cmd, const char* name) {
        const int i = findBin(img, fwType);
        if (i < 0 || n >= kMaxLoadSteps)
            return;
        // fileOffset is the absolute file offset: array-relative + arrayOffset.
        plan[n++] = {cmd, img.arrayOffset + img.bins[i].offsetBytes,
                     img.bins[i].sizeBytes, img.bins[i].fwVersion, fwType,
                     name};
    };

    addBin(FwKdb, kBlLoadKeyDb, "KDB");
    // SPL rides on the KDB image (psp_v13_0_bootloader_load_spl uses psp->kdb).
    const int kdb = findBin(img, FwKdb);
    const int spl = findBin(img, FwSpl);
    if (kdb >= 0 && spl >= 0 && n < kMaxLoadSteps) {
        plan[n++] = {kBlLoadTosSplTab, img.arrayOffset + img.bins[kdb].offsetBytes,
                     img.bins[kdb].sizeBytes, img.bins[spl].fwVersion, FwSpl,
                     "SPL"};
    }
    addBin(FwSysDrv, kBlLoadSysDrv, "SYS_DRV");
    addBin(FwSocDrv, kBlLoadSocDrv, "SOC_DRV");
    addBin(FwIntfDrv, kBlLoadIntfDrv, "INTF_DRV");
    addBin(FwDbgDrv, kBlLoadDbgDrv, "DBG_DRV");
    addBin(FwRasDrv, kBlLoadRasDrv, "RAS_DRV");
    addBin(FwSos, kBlLoadSosDrv, "SOS");
    return n;
}

// --- Mailbox / MM-window / VRAM constants (this card's evidence) -----------
inline constexpr uint32_t kMp0BaseDwords = 0x16000;   // hw_id 255 (MP0) base[0]
inline constexpr uint32_t kC2pmsg35Dword = 0x063;     // bootloader command/status
inline constexpr uint32_t kC2pmsg36Dword = 0x064;     // bootloader address (MC >> 20)
inline constexpr uint32_t kC2pmsg58Dword = 0x07a;     // sOS version after boot
inline constexpr uint32_t kC2pmsg81Dword = 0x091;     // sOS sign of life

inline constexpr uint32_t kMmhubBaseDwords = 0x1A000;  // hw_id 34 (MMHUB 3.0.2) base[0]
inline constexpr uint32_t kFbLocationBaseDword = 0x8EC; // MMHUB_FB_LOCATION_BASE
inline constexpr uint32_t kFbLocationTopDword = 0x8ED;  // MMHUB_FB_LOCATION_TOP

// SOC15 MMIO window (NBIF block, base 0): index/data pair inside register BAR5.
inline constexpr uint32_t kMmIndexByte = 0x00;
inline constexpr uint32_t kMmDataByte = 0x04;
inline constexpr uint32_t kMmIndexHiByte = 0x18;

// VRAM size in MB (mmRCC_CONFIG_MEMSIZE, NBIF base 0).
inline constexpr uint32_t kRccConfigMemsizeByte = 0xDE3u * 4u;

inline constexpr uint32_t kBootloaderReady = 0x80000000u;

inline constexpr uint64_t kStagingVramOffset = 0x40000000ull;  // 1 GiB into VRAM
inline constexpr uint32_t kStagingBytes = 1u << 20;            // 1 MiB, 1 MiB aligned
inline constexpr uint32_t kStagingAlign = 1u << 20;

// IP Discovery: DISCOVERY_TMR_OFFSET below the top of VRAM, 10 KiB long.
inline constexpr uint32_t kDiscoveryTopMargin = 64 * 1024;
inline constexpr uint32_t kDiscoveryReadBytes = 10 * 1024;
inline constexpr uint32_t kDiscoverySignature = 0x28211407u;

inline constexpr uint32_t kSosPostCmdDelayMs = 20;    // mdelay(20) in psp_v13_0.c
inline constexpr uint32_t kBootloaderWaitMs = 2000;   // Linux: 10 x psp_wait_for
inline constexpr uint32_t kSosAliveWaitMs = 1000;

// Mailbox byte offsets, same addressing as Navi33PspMailbox.hpp.
inline constexpr uint64_t mp0ByteOffset(uint32_t dword) noexcept
{
    return (static_cast<uint64_t>(kMp0BaseDwords) + dword) * 4ull;
}

inline constexpr uint64_t mmhubByteOffset(uint32_t dword) noexcept
{
    return (static_cast<uint64_t>(kMmhubBaseDwords) + dword) * 4ull;
}

// Status decodes (psp_v13_0.c): bit 31 = ready; on error, bits[7:0] carry the
// status for PSP 13.0.6 and newer (13.0.7 included).
inline constexpr bool bootloaderReady(uint32_t v) noexcept
{
    return (v & kBootloaderReady) != 0;
}

inline constexpr uint32_t bootloaderError(uint32_t v) noexcept
{
    return v & 0xFFu;
}

inline constexpr bool sosAlive(uint32_t c2pmsg81) noexcept
{
    return c2pmsg81 != 0;
}

// MM_INDEX/MM_DATA window encoding (RedNavi-verified sequence on Navi 32).
inline constexpr uint32_t mmIndexLow(uint64_t vramByte) noexcept
{
    return static_cast<uint32_t>(vramByte) | 0x80000000u;
}

inline constexpr uint32_t mmIndexHigh(uint64_t vramByte) noexcept
{
    return static_cast<uint32_t>(vramByte >> 31);
}

}  // namespace cobalt64::navi33::psp

#endif
