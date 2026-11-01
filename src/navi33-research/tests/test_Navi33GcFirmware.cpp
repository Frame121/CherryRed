// Host validation of the GC/SMU firmware parsers against the REAL
// hash-pinned Navi 33 blobs (src/navi33-bringup/firmware/, gitignored; the
// CI workflow fetches them from the pinned linux-firmware commit). The
// expected numbers below were cross-checked by an independent parse of the
// same blobs; a parser regression or a changed blob set shows up here.
//
// Usage: test_Navi33GcFirmware <firmware-dir>   (skips if the dir is absent)
#define _CRT_SECURE_NO_WARNINGS
#include "Navi33GcLoad.hpp"
#include "Navi33SmuLoad.hpp"

#include <stdio.h>
#include <string.h>

namespace {

int failures = 0;

void check(bool condition, const char *label) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", label);
        ++failures;
    }
}

bool readFile(const char *path, unsigned char **out, uint32_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > (64L << 20)) {
        fclose(f);
        return false;
    }
    unsigned char *data = new unsigned char[size];
    if (fread(data, 1, static_cast<size_t>(size), f) != static_cast<size_t>(size)) {
        delete[] data;
        fclose(f);
        return false;
    }
    fclose(f);
    *out = data;
    *len = static_cast<uint32_t>(size);
    return true;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: test_Navi33GcFirmware <firmware-dir>\n");
        return 2;
    }
    const char *dir = argv[1];
    // Deliberate skip (exit 0): the blobs are gitignored; a host without
    // them simply cannot run this check.
    char probe[512];
    snprintf(probe, sizeof(probe), "%s/smu_13_0_7.bin", dir);
    FILE *f = fopen(probe, "rb");
    if (!f) {
        printf("GcFirmware tests: SKIP (firmware blobs not retained on this host)\n");
        return 0;
    }
    fclose(f);

    using namespace cobalt64::navi33::psp;

    GcBlobRef blobs[kGcBlobCount] = {};
    unsigned char *raw[kGcBlobCount] = {};
    const char *names[kGcBlobCount] = {
        "gc_11_0_2_pfp.bin", "gc_11_0_2_me.bin", "gc_11_0_2_mec.bin",
        "gc_11_0_2_rlc.bin", "gc_11_0_2_imu.bin", "gc_11_0_2_mes_2.bin",
        "gc_11_0_2_mes1.bin", "sdma_6_0_2.bin"};
    for (uint32_t i = 0; i < kGcBlobCount; ++i) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        uint32_t len = 0;
        check(readFile(path, &raw[i], &len), names[i]);
        blobs[i] = GcBlobRef{raw[i], len};
    }

    if (failures == 0) {
        // SMU ucode: header parses, version and size match the pinned blobs.
        SmuUcode smu;
        unsigned char *smuRaw = nullptr;
        uint32_t smuLen = 0;
        char path[512];
        snprintf(path, sizeof(path), "%s/smu_13_0_7.bin", dir);
        check(readFile(path, &smuRaw, &smuLen), "smu_13_0_7.bin");
        check(parseSmuUcode(smuRaw, smuLen, smu), "smu header parses");
        check(smu.version == 0x526100u, "smu ucode version 0x526100");
        check(smu.sizeBytes == 261632u, "smu ucode size 261632");
        delete[] smuRaw;

        // GC image plan: 26 images, RLC_G last, sizes match the independent
        // parse of the same blobs.
        GcImage images[kMaxGcImages];
        const uint32_t count = buildGcImagePlan(blobs, images);
        check(count == 26u, "26 GC/SDMA images");
        if (count == 26u) {
            check(images[count - 1].fwType == kGfxFwTypeRlcG,
                  "RLC_G is the last image (autoload trigger)");
            check(strcmp(images[count - 1].name, "RLC_G") == 0, "RLC_G name");
            check(images[count - 1].sizeBytes == 25088u, "RLC_G size 25088");
            check(images[0].fwType == kGfxFwTypeSdmaTh0 &&
                      images[0].sizeBytes == 17408u,
                  "SDMA_UCODE_TH0 first, 17408 bytes");
            check(images[2].fwType == kGfxFwTypeRs64Pfp &&
                      images[2].sizeBytes == 98144u,
                  "CP_RS64_PFP 98144 bytes");
            // [2] PFP, [3..4] its stacks, [5] ME, [6..7] its stacks, [8] MEC.
            check(images[8].fwType == kGfxFwTypeRs64Mec &&
                      images[8].sizeBytes == 267008u,
                  "CP_RS64_MEC 267008 bytes");
            uint32_t totalBytes = 0;
            for (uint32_t i = 0; i < count; ++i)
                totalBytes += images[i].sizeBytes;
            check(totalBytes == 1333328u, "total image payload 1333328 bytes");
        }

        // Fail-closed: a corrupted RLC header version must void the plan.
        GcBlobRef broken[kGcBlobCount];
        for (uint32_t i = 0; i < kGcBlobCount; ++i)
            broken[i] = blobs[i];
        unsigned char *rlcCopy = new unsigned char[blobs[kGcBlobRlc].len];
        memcpy(rlcCopy, blobs[kGcBlobRlc].data, blobs[kGcBlobRlc].len);
        rlcCopy[8] = 9;  // bogus header major
        broken[kGcBlobRlc] = GcBlobRef{rlcCopy, blobs[kGcBlobRlc].len};
        GcImage brokenImages[kMaxGcImages];
        check(buildGcImagePlan(broken, brokenImages) == 0u,
              "corrupted RLC header refuses the whole plan (fail-closed)");
        delete[] rlcCopy;
    }

    for (uint32_t i = 0; i < kGcBlobCount; ++i)
        delete[] raw[i];

    if (failures != 0) {
        fprintf(stderr, "GcFirmware tests: %d failure(s)\n", failures);
        return 1;
    }
    puts("GcFirmware tests: PASS (real pinned blobs: SMU ucode + 26-image plan + "
         "fail-closed refusal)");
    return 0;
}
