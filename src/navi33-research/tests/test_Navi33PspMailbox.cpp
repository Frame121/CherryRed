// Host unit tests for the pure PSP/SMU mailbox survey logic
// (src/navi33-bringup/Navi33PspMailbox.hpp). No IOKit, no hardware: the
// offsets, bounds checks and decode semantics are pinned here so the kext
// cannot drift from the captured-evidence provenance.
#include "Navi33PspMailbox.hpp"

#include <stdio.h>

namespace {

int failures = 0;

void check(bool condition, const char *label) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace cobalt64::navi33::psp;

    // Bar selection and PCI command bits.
    check(kRegBarIndex == 5, "MMIO registers live in BAR5 on Navi 3x");
    check(kPciCommandMemorySpace == 0x2, "PCI command memory-space bit is bit 1");

    // Captured-evidence bases: MP0 (hw_id 255) and MP1 (hw_id 1) share
    // base[0] = 0x16000 dwords in this card's IP Discovery table.
    check(kMp0BaseDwords == 0x16000 && kMp1BaseDwords == 0x16000,
          "captured MP0/MP1 base[0] must stay 0x16000 dwords");

    // Byte-offset math: (base + dword) * 4.
    check(mailboxByteOffset(0x16000, 0x061) == 0x58184ull,
          "MP0_C2PMSG_33 byte offset");
    check(mailboxByteOffset(0x16000, 0x091) == 0x58244ull,
          "MP0_C2PMSG_81 byte offset");
    check(mailboxByteOffset(0x16000, 0x29a) == 0x58a68ull,
          "MP1_C2PMSG_90 byte offset");

    // Bounds: a 1 MiB BAR5 holds every mailbox of the pinned table; smaller
    // windows or the unmapped tail must fail closed.
    check(dwordInBar(0x58184ull, 0x100000ull), "C2PMSG_33 fits a 1 MiB BAR5");
    check(dwordInBar(0x58a68ull, 0x100000ull), "C2PMSG_90 fits a 1 MiB BAR5");
    check(!dwordInBar(0x58184ull, 0x40000ull),
          "a 256 KiB window must reject the mailbox region");
    check(!dwordInBar(0x58184ull, 0), "a zero-length BAR rejects every read");
    check(!dwordInBar(0x4ull, 4ull), "offset 4 in a 4-byte BAR is out of range");
    check(!dwordInBar(0x90ff000ull, 0x100000ull),
          "the unmapped 5th base region (0x243FC00 dwords) must stay out of reach");

    // The whole pinned table must sit inside a 1 MiB register window, matching
    // what macOS reports for BAR5 on this card.
    for (unsigned i = 0; i < kMailboxSpecCount; ++i) {
        const uint64_t off =
            mailboxByteOffset(kMailboxSpecs[i].baseDwords, kMailboxSpecs[i].dword);
        check(dwordInBar(off, 0x100000ull), "table entry inside 1 MiB BAR5");
    }

    // Decode semantics.
    check(ifwiInitDone(0x80000000u) && !ifwiInitDone(0x00000001u),
          "IFWI-init decode is bit 31");
    check(pspBootloaderReady(0x80000000u) && !pspBootloaderReady(0x1234u),
          "bootloader-ready decode is bit 31");
    check(!secureOsAlive(0u) && secureOsAlive(1u),
          "sOS-alive decode is nonzero");
    check(!smuResponded(0u) && smuResponded(1u) && smuResponded(0xFFu),
          "SMU responded = nonzero (0 busy, 1 OK, 0xFF refused)");
    check(smuResponseOk(1u) && smuResponseOk(0x80000001u) && !smuResponseOk(0xFFu) &&
              !smuResponseOk(0u),
          "SMU response OK = low half equals 1");
    check(smuResponseStatus(0x8000FFFFu) == 0xFFFFu,
          "SMU status extracts the low half");

    // Table pins: the decodes index fixed positions.
    check(kMailboxSpecs[kSpecIfwiInit].dword == 0x061, "pinned IFWI entry");
    check(kMailboxSpecs[kSpecBootloader].dword == 0x063, "pinned bootloader entry");
    check(kMailboxSpecs[kSpecSosAlive].dword == 0x091, "pinned sOS entry");
    check(kMailboxSpecs[kSpecSmuResponse].dword == 0x29a, "pinned SMU entry");

    if (failures != 0) {
        fprintf(stderr, "PSP mailbox logic tests: %d failure(s)\n", failures);
        return 1;
    }

    puts("PSP mailbox logic tests: PASS (offsets, bounds, decodes, table pins)");
    return 0;
}
