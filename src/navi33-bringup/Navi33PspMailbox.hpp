#ifndef COBALT64_NAVI33_PSP_MAILBOX_HPP
#define COBALT64_NAVI33_PSP_MAILBOX_HPP

#include <stdint.h>

#include "Navi33PspLoad.hpp"

// Pure, host-testable pieces of the PSP/SMU mailbox survey. No IOKit includes:
// this header compiles both in the kext and in the host unit tests.
//
// Provenance of every constant here:
//  - The MP0/MP1 base addresses are instance-0 base[0] of hardware IDs 255/1
//    in this card's captured IP Discovery table
//    (tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin,
//    SHA-256 4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89;
//    same values in RedNavi-collab-review/docs/logs/navi33/ip_discovery.txt).
//    Discovery bases are dword indices; the byte offset is (base + dword) * 4.
//  - The mailbox dword offsets come from the shared PSP/SMU 13 register file
//    (Linux mp_13_0_0_offset.h and smu_v13_0.c), the same file PSP 13.0.7 and
//    SMU 13.0.7 use; the decode bits mirror RedNavi's Navi 32 M1.5 stage that
//    was verified on a real RX 7800 XT. Reading them on Navi 33 is the point
//    of this stage: the VALUES are the evidence, and they stay UNVERIFIED
//    until this stage runs on the card.

namespace cobalt64::navi33::psp {

inline constexpr uint32_t kRegBarIndex = 5;   // BAR5: MMIO register window (1 MiB)
inline constexpr uint16_t kPciCommandMemorySpace = 0x2;  // PCI command bit 1

// kMp0BaseDwords lives in Navi33PspLoad.hpp (single shared definition).
inline constexpr uint32_t kMp1BaseDwords = 0x16000;  // hw_id 1   (MP1) base[0]

struct MailboxSpec {
    const char *name;
    uint32_t baseDwords;
    uint32_t dword;
    const char *note;
};

inline constexpr MailboxSpec kMailboxSpecs[] = {
    {"MP0_C2PMSG_33", kMp0BaseDwords, 0x061, "bit31 IFWI init done"},
    {"MP0_C2PMSG_35", kMp0BaseDwords, 0x063, "bit31 bootloader ready"},
    {"MP0_C2PMSG_64", kMp0BaseDwords, 0x080, "ring command/response"},
    {"MP0_C2PMSG_67", kMp0BaseDwords, 0x083, "ring write pointer"},
    {"MP0_C2PMSG_71", kMp0BaseDwords, 0x087, "ring size"},
    {"MP0_C2PMSG_81", kMp0BaseDwords, 0x091, "nonzero: secure OS alive"},
    {"MP1_C2PMSG_66", kMp1BaseDwords, 0x282, "SMU message"},
    {"MP1_C2PMSG_82", kMp1BaseDwords, 0x292, "SMU argument"},
    {"MP1_C2PMSG_90", kMp1BaseDwords, 0x29a, "SMU response, 1 = OK"},
};

inline constexpr unsigned kMailboxSpecCount =
    sizeof(kMailboxSpecs) / sizeof(kMailboxSpecs[0]);

// Indices into kMailboxSpecs the survey decodes from. Pinned with
// static_asserts below so reordering the table cannot silently break the
// decodes.
inline constexpr unsigned kSpecIfwiInit = 0;     // MP0_C2PMSG_33
inline constexpr unsigned kSpecBootloader = 1;   // MP0_C2PMSG_35
inline constexpr unsigned kSpecSosAlive = 5;     // MP0_C2PMSG_81
inline constexpr unsigned kSpecSmuResponse = 8;  // MP1_C2PMSG_90

static_assert(kMailboxSpecs[kSpecIfwiInit].dword == 0x061 &&
                  kMailboxSpecs[kSpecIfwiInit].baseDwords == kMp0BaseDwords,
              "IFWI-init spec must stay MP0_C2PMSG_33 at base[0]");
static_assert(kMailboxSpecs[kSpecBootloader].dword == 0x063 &&
                  kMailboxSpecs[kSpecBootloader].baseDwords == kMp0BaseDwords,
              "bootloader spec must stay MP0_C2PMSG_35 at base[0]");
static_assert(kMailboxSpecs[kSpecSosAlive].dword == 0x091 &&
                  kMailboxSpecs[kSpecSosAlive].baseDwords == kMp0BaseDwords,
              "sOS spec must stay MP0_C2PMSG_81 at base[0]");
static_assert(kMailboxSpecs[kSpecSmuResponse].dword == 0x29a &&
                  kMailboxSpecs[kSpecSmuResponse].baseDwords == kMp1BaseDwords,
              "SMU response spec must stay MP1_C2PMSG_90 at base[0]");

// Discovery bases are in dword units: byte offset = (base + dword) * 4.
inline constexpr uint64_t mailboxByteOffset(uint32_t baseDwords,
                                            uint32_t dword) noexcept {
    return (static_cast<uint64_t>(baseDwords) +
            static_cast<uint64_t>(dword)) * 4ull;
}

// A read is only attempted when the whole dword lies inside the mapped BAR.
inline constexpr bool dwordInBar(uint64_t byteOffset, uint64_t barLength) noexcept {
    return barLength >= 4ull && byteOffset <= barLength - 4ull;
}

inline constexpr bool ifwiInitDone(uint32_t c2pmsg33) noexcept {
    return (c2pmsg33 & 0x80000000u) != 0;
}

inline constexpr bool pspBootloaderReady(uint32_t c2pmsg35) noexcept {
    return (c2pmsg35 & 0x80000000u) != 0;
}

inline constexpr bool secureOsAlive(uint32_t c2pmsg81) noexcept {
    return c2pmsg81 != 0;
}

// SMU response semantics (smu_v13_0.c): 0 = still busy, 1 = OK,
// 0xFF = refused; only the low half carries the status.
inline constexpr bool smuResponded(uint32_t mp1C2pmsg90) noexcept {
    return mp1C2pmsg90 != 0;
}

inline constexpr uint32_t smuResponseStatus(uint32_t mp1C2pmsg90) noexcept {
    return mp1C2pmsg90 & 0xFFFFu;
}

inline constexpr bool smuResponseOk(uint32_t mp1C2pmsg90) noexcept {
    return (mp1C2pmsg90 & 0xFFFFu) == 1u;
}

}  // namespace cobalt64::navi33::psp

#endif
