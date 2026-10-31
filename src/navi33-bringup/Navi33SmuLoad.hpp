#ifndef COBALT64_NAVI33_SMU_LOAD_HPP
#define COBALT64_NAVI33_SMU_LOAD_HPP

#include <stdint.h>

#include "Navi33PspLoad.hpp"

// Pure, host-testable pieces of the SMU stage (M2b): hand the SMU 13.0.7
// firmware to the secure OS through LOAD_IP_FW, then talk to the SMU over the
// MP1 mailbox. No IOKit includes.
//
// Provenance:
//  - GFX_FW_TYPE_SMU = 18 (psp_gfx_if.h) and the LOAD_IP_FW argument shape
//    (fw MC lo/hi, size, type) are the working RedNavi implementation,
//    verified on a real RX 7800 XT (Navi 32); the Navi 33 behaviour stays
//    UNVERIFIED until this stage runs on the card.
//  - The SMU 13 message protocol (previous message must complete: C2PMSG_90
//    nonzero; then C2PMSG_90 = 0, C2PMSG_82 = arg, C2PMSG_66 = msg; response
//    1 = PPSMC_Result_OK, 0xFF = PPSMC_Result_Failed, result in C2PMSG_82)
//    is smu_v13_0_send_msg / _wait_for_response in the Linux kernel (MIT),
//    and RedNavi's rn_smu13_send verified on Navi 32. Message IDs from
//    smu_v13_0.h: TestMessage = 1, GetSmuVersion = 2, GetDriverIfVersion = 3.
//  - The SMU ucode payload handed to the PSP is the common-firmware-header
//    ucode array (ucode_size_bytes at +20, ucode_array_offset_bytes at +24),
//    as amdgpu_ucode_init_single_fw builds it for AMDGPU_UCODE_ID_SMC.

namespace cobalt64::navi33::psp {

inline constexpr uint32_t kGfxFwTypeSmu = 18u;  // GFX_FW_TYPE_SMU
inline constexpr uint32_t kCmdLoadIpFw = 0x06u; // GFX_CMD_ID_LOAD_IP_FW

// The MP1 mailbox dwords (kMp1C2pmsg66Dword/82Dword/90Dword) and the SMU
// response decode (smuResponded/smuResponseStatus/smuResponseOk) live in
// Navi33PspMailbox.hpp — one shared definition for every stage.

inline constexpr uint32_t kPpsmcMsgTestMessage = 0x1u;
inline constexpr uint32_t kPpsmcMsgGetSmuVersion = 0x2u;
inline constexpr uint32_t kPpsmcMsgGetDriverIfVersion = 0x3u;

struct SmuUcode {
    uint32_t version = 0;     // ucode_version
    uint32_t offsetBytes = 0; // ucode_array_offset_bytes
    uint32_t sizeBytes = 0;   // ucode_size_bytes
};

// Bounds-checked extraction of the SMU ucode payload from smu_13_0_7.bin.
inline bool parseSmuUcode(const uint8_t *data, uint32_t dataLen, SmuUcode &out)
{
    out = SmuUcode();
    if (!data || dataLen < 32)
        return false;
    const uint32_t declaredSize = readU32Le(data + 0);
    const uint16_t major = readU16Le(data + 8);
    const uint16_t minor = readU16Le(data + 10);
    const uint32_t version = readU32Le(data + 16);
    const uint32_t ucodeSize = readU32Le(data + 20);
    const uint32_t arrayOff = readU32Le(data + 24);
    if (declaredSize != dataLen)
        return false;
    if (major == 0 && minor == 0)
        return false;
    if (ucodeSize == 0 || arrayOff > declaredSize ||
        ucodeSize > declaredSize - arrayOff)
        return false;
    out = SmuUcode{version, arrayOff, ucodeSize};
    return true;
}

}  // namespace cobalt64::navi33::psp

#endif
