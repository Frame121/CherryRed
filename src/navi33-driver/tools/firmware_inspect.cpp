#include "Firmware.hpp"
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: navi33_firmware_inspect FILE\n");
        return 2;
    }
    FILE* file = std::fopen(argv[1], "rb");
    if (!file) { std::perror("open firmware"); return 2; }
    // Bounded host-only intake; large/unsupported containers need a separate
    // reviewed path. No firmware is redistributed, uploaded, or loaded.
    constexpr size_t limit = 64 * 1024 * 1024;
    std::vector<uint8_t> bytes;
    uint8_t block[4096];
    for (;;) {
        const size_t read = std::fread(block, 1, sizeof(block), file);
        if (bytes.size() > limit - read) {
            std::fputs("firmware exceeds 64 MiB intake limit\n", stderr);
            std::fclose(file);
            return 2;
        }
        bytes.insert(bytes.end(), block, block + read);
        if (read != sizeof(block)) break;
    }
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    if (failed) { std::fputs("firmware read failed\n", stderr); return 2; }
    cherryred::navi33::FirmwareInfo info;
    const auto error = cherryred::navi33::inspectFirmware(bytes.data(), bytes.size(), info);
    if (error != cherryred::navi33::FirmwareError::None) {
        std::fprintf(stderr, "rejected: %s\n", cherryred::navi33::firmwareErrorName(error));
        return 1;
    }
    std::printf("Common envelope: bounded\nFile bytes: %u\nHeader: %u.%u (%u bytes)\n"
                "Declared IP: %u.%u\nMicrocode version: %u\nPayload: offset=%u bytes=%u\n"
                "Declared CRC32: %08x (not verified)\n"
                "IP-specific subimages, signature, provenance, board compatibility: unverified\n"
                "GPU initialization/display/Metal: unavailable\n",
                static_cast<unsigned>(info.fileSize), static_cast<unsigned>(info.headerMajor),
                static_cast<unsigned>(info.headerMinor), static_cast<unsigned>(info.headerSize),
                static_cast<unsigned>(info.ipMajor), static_cast<unsigned>(info.ipMinor),
                static_cast<unsigned>(info.ucodeVersion), static_cast<unsigned>(info.payloadOffset),
                static_cast<unsigned>(info.payloadSize), static_cast<unsigned>(info.declaredCrc32));
    return 0;
}
