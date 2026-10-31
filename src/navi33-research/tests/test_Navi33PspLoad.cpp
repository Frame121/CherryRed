//
// test_Navi33PspLoad.cpp — host unit tests for the pure logic of the PSP
// 13.0.7 firmware-load stage (Navi33PspLoad.hpp). No IOKit, no vendor
// firmware bytes: the fixture reproduces only the DOCUMENTED HEADER METADATA
// of psp_13_0_7_sos.bin (component types/versions/offsets/sizes read from the
// v2.0 header — the same factual values PSP_LOAD_STAGE.md records).
//
#include "Navi33PspLoad.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace cobalt64::navi33::psp;

namespace {

int failures = 0;

void check(bool condition, const char *label)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}

void putU16(std::vector<uint8_t> &b, size_t off, uint16_t v)
{
    b[off] = static_cast<uint8_t>(v & 0xFF);
    b[off + 1] = static_cast<uint8_t>(v >> 8);
}

void putU32(std::vector<uint8_t> &b, size_t off, uint32_t v)
{
    b[off] = static_cast<uint8_t>(v & 0xFF);
    b[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    b[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    b[off + 3] = static_cast<uint8_t>(v >> 24);
}

// The real psp_13_0_7_sos.bin component table (header metadata only):
//   SYS_DRV ver 0x330024 off 0      sz 141200
//   SOS     ver 0x330024 off 141200 sz 99184
//   TOC     ver 0x00000c off 240384 sz 2304
//   KDB     ver 0x04003300 off 242688 sz 4880
//   SPL     ver 0x010033c0 off 247568 sz 672
// Fixture uses the same SHAPE with small sizes so the test never carries
// firmware payload bytes: the table layout, order, and boundary arithmetic
// are what is exercised.
struct Fixture {
    static constexpr uint32_t kArrayOff = 256;
    static constexpr uint32_t kDeclaredSize = 768;   // arrayOff + ucodeSize
    static constexpr uint32_t kUcodeSize = 512;
    std::vector<uint8_t> bytes;

    Fixture()
    {
        bytes.assign(kDeclaredSize, 0);
        putU32(bytes, 0, kDeclaredSize);          // size_bytes
        putU32(bytes, 4, 36);                     // header_size_bytes
        putU16(bytes, 8, 2);                      // header version 2.0
        putU16(bytes, 10, 0);
        putU16(bytes, 12, 13);                    // IP version 13.0
        putU16(bytes, 14, 0);
        putU32(bytes, 16, 0x00330024);            // ucode_version
        putU32(bytes, 20, kUcodeSize);            // ucode_size_bytes
        putU32(bytes, 24, kArrayOff);             // ucode_array_offset_bytes
        putU32(bytes, 28, 0xDEADBEEF);            // crc (not checked)
        putU32(bytes, 32, 5);                     // psp_fw_bin_count

        struct Bin { uint32_t type, ver, off, sz; };
        const Bin bins[5] = {
            {2, 0x00330024, 0, 160},       // SYS_DRV
            {1, 0x00330024, 160, 200},     // SOS
            {4, 0x0000000C, 360, 72},      // TOC
            {3, 0x04003300, 432, 48},      // KDB
            {5, 0x010033C0, 480, 32},      // SPL
        };
        for (size_t i = 0; i < 5; ++i) {
            const size_t base = 36 + i * 16;
            putU32(bytes, base + 0, bins[i].type);
            putU32(bytes, base + 4, bins[i].ver);
            putU32(bytes, base + 8, bins[i].off);
            putU32(bytes, base + 12, bins[i].sz);
        }
    }
};

void testValidParse()
{
    Fixture f;
    SosImage img;
    check(parseSosHeaderV2(f.bytes.data(),
                           static_cast<uint32_t>(f.bytes.size()), img),
          "valid fixture parses");
    check(img.ucodeVersion == 0x00330024u, "ucode version");
    check(img.ucodeSize == 512u, "ucode size");
    check(img.arrayOffset == 256u, "array offset");
    check(img.binCount == 5u, "bin count");
    check(findBin(img, FwSos) == 1, "SOS bin index");
    check(findBin(img, FwSysDrv) == 0, "SYS_DRV bin index");
    check(findBin(img, FwToc) == 2, "TOC bin index");
    check(findBin(img, FwKdb) == 3, "KDB bin index");
    check(findBin(img, FwSpl) == 4, "SPL bin index");
    check(findBin(img, FwSocDrv) == -1, "SOC_DRV absent");
    check(findBin(img, FwIntfDrv) == -1, "INTF_DRV absent");
    check(findBin(img, FwDbgDrv) == -1, "DBG_DRV absent");
    check(findBin(img, FwRasDrv) == -1, "RAS_DRV absent");
}

void testLoadPlan()
{
    Fixture f;
    SosImage img;
    check(parseSosHeaderV2(f.bytes.data(),
                           static_cast<uint32_t>(f.bytes.size()), img),
          "plan fixture parses");

    LoadStep plan[kMaxLoadSteps];
    const uint32_t n = buildLoadPlan(img, plan);

    // KDB, SPL, SYS_DRV, SOS — SOC/INTF/DBG/RAS are absent and must be skipped.
    check(n == 4u, "plan has 4 steps (absent components skipped)");
    if (n != 4u)
        return;

    check(std::strcmp(plan[0].name, "KDB") == 0, "step 0 is KDB");
    check(plan[0].cmd == kBlLoadKeyDb, "KDB command");
    check(plan[0].fwType == FwKdb, "KDB type");
    check(plan[0].fileOffset == 256u + 432u, "KDB absolute file offset");
    check(plan[0].sizeBytes == 48u, "KDB size");
    check(plan[0].version == 0x04003300u, "KDB version");

    // SPL rides on the KDB image (psp_v13_0_bootloader_load_spl uses psp->kdb).
    check(std::strcmp(plan[1].name, "SPL") == 0, "step 1 is SPL");
    check(plan[1].cmd == kBlLoadTosSplTab, "SPL command");
    check(plan[1].fwType == FwSpl, "SPL type");
    check(plan[1].fileOffset == 256u + 432u, "SPL rides on the KDB payload");
    check(plan[1].sizeBytes == 48u, "SPL payload size = KDB size");
    check(plan[1].version == 0x010033C0u, "SPL version is the SPL bin's own");

    check(std::strcmp(plan[2].name, "SYS_DRV") == 0, "step 2 is SYS_DRV");
    check(plan[2].cmd == kBlLoadSysDrv, "SYS_DRV command");
    check(plan[2].fileOffset == 256u + 0u, "SYS_DRV file offset");
    check(plan[2].sizeBytes == 160u, "SYS_DRV size");

    check(std::strcmp(plan[3].name, "SOS") == 0, "step 3 is SOS (last)");
    check(plan[3].cmd == kBlLoadSosDrv, "SOS command");
    check(plan[3].fileOffset == 256u + 160u, "SOS file offset");
    check(plan[3].sizeBytes == 200u, "SOS size");
}

void testMalformedHeaders()
{
    Fixture f;
    SosImage img;

    std::vector<uint8_t> tiny(16, 0);
    check(!parseSosHeaderV2(tiny.data(), 16, img),
          "truncated header refuses");

    std::vector<uint8_t> wrongSize = f.bytes;
    putU32(wrongSize, 0, 512);
    check(!parseSosHeaderV2(wrongSize.data(),
                            static_cast<uint32_t>(wrongSize.size()), img),
          "declared size mismatch refuses");

    std::vector<uint8_t> v1 = f.bytes;
    putU16(v1, 8, 1);
    check(!parseSosHeaderV2(v1.data(), static_cast<uint32_t>(v1.size()), img),
          "non-2.0 header refuses");

    std::vector<uint8_t> zeroCount = f.bytes;
    putU32(zeroCount, 32, 0);
    check(!parseSosHeaderV2(zeroCount.data(),
                            static_cast<uint32_t>(zeroCount.size()), img),
          "zero bin count refuses");

    std::vector<uint8_t> hugeCount = f.bytes;
    putU32(hugeCount, 32, kMaxBins + 1);
    check(!parseSosHeaderV2(hugeCount.data(),
                            static_cast<uint32_t>(hugeCount.size()), img),
          "count above the cap refuses");

    std::vector<uint8_t> badSz = f.bytes;
    putU32(badSz, 36 + 1 * 16 + 12, 1200);
    check(!parseSosHeaderV2(badSz.data(),
                            static_cast<uint32_t>(badSz.size()), img),
          "payload escaping ucode_size refuses");

    std::vector<uint8_t> zeroSz = f.bytes;
    putU32(zeroSz, 36 + 2 * 16 + 12, 0);
    check(!parseSosHeaderV2(zeroSz.data(),
                            static_cast<uint32_t>(zeroSz.size()), img),
          "zero-size component refuses");

    std::vector<uint8_t> badType = f.bytes;
    putU32(badType, 36 + 2 * 16 + 0, 77);
    check(!parseSosHeaderV2(badType.data(),
                            static_cast<uint32_t>(badType.size()), img),
          "unknown component type refuses");

    std::vector<uint8_t> escape = f.bytes;
    putU32(escape, 36 + 4 * 16 + 8, 1000);
    check(!parseSosHeaderV2(escape.data(),
                            static_cast<uint32_t>(escape.size()), img),
          "file-range escape refuses");
}

void testPinnedOffsets()
{
    // The mailbox byte offsets the stage writes must stay pinned to the
    // mp_13_0_2_offset.h values (MP0 base 0x16000 dwords).
    check(mp0ByteOffset(kC2pmsg35Dword) == 0x5818Cu, "C2PMSG_35 byte offset");
    check(mp0ByteOffset(kC2pmsg36Dword) == 0x58190u, "C2PMSG_36 byte offset");
    check(mp0ByteOffset(kC2pmsg58Dword) == 0x581E8u, "C2PMSG_58 byte offset");
    check(mp0ByteOffset(kC2pmsg81Dword) == 0x58244u, "C2PMSG_81 byte offset");

    // MMHUB 3.0.2 base[0] = 0x1A000 dwords (this card's IP discovery).
    check(mmhubByteOffset(kFbLocationBaseDword) == 0x6A3B0u,
          "FB_LOCATION_BASE byte offset");
    check(mmhubByteOffset(kFbLocationTopDword) == 0x6A3B4u,
          "FB_LOCATION_TOP byte offset");

    // MM_INDEX window (NBIF base 0).
    check(kMmIndexByte == 0x00u, "MM_INDEX byte offset");
    check(kMmDataByte == 0x04u, "MM_DATA byte offset");
    check(kMmIndexHiByte == 0x18u, "MM_INDEX_HI byte offset");

    // Staging: 1 GiB into VRAM, 1 MiB, 1 MiB aligned.
    check(kStagingVramOffset == 0x40000000ull, "staging VRAM offset");
    check(kStagingBytes == (1u << 20), "staging size");
    check(kStagingAlign == (1u << 20), "staging alignment");
}

void testWindowEncodingAndDecodes()
{
    // MM_INDEX/MM_DATA window encoding (RedNavi-verified sequence).
    check(mmIndexLow(0x00000000u) == 0x80000000u, "index low, zero");
    check(mmIndexLow(0x12345678u) == 0x92345678u, "index low, bit31 merge");
    check(mmIndexHigh(0x00000000u) == 0u, "index high, zero");
    check(mmIndexHigh(0x7FFFFFFFu) == 0u, "index high, <2GiB");
    check(mmIndexHigh(0x80000000u) == 1u, "index high, 2GiB");
    check(mmIndexHigh(0x180000000ull) == 3u, "index high, 6GiB");

    // Status decodes (psp_v13_0.c).
    check(bootloaderReady(0x80000000u), "ready bit set");
    check(!bootloaderReady(0x00000000u), "ready bit clear");
    // bit31 set = the bootloader RESPONDED; error status rides in bits[7:0]
    // and is decoded separately by bootloaderError().
    check(bootloaderReady(0x8000000Au), "response bit set with error bits");
    check(bootloaderError(0x8000000Au) == 0x0Au, "error bits decode");
    check(bootloaderError(0x80000000u) == 0u, "no error bits");
    check(!sosAlive(0u), "sOS dead");
    check(sosAlive(0x0136947Cu), "sOS alive");
}

}  // namespace

int main()
{
    testValidParse();
    testLoadPlan();
    testMalformedHeaders();
    testPinnedOffsets();
    testWindowEncodingAndDecodes();

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("Navi33PspLoad: all checks passed\n");
    return 0;
}
