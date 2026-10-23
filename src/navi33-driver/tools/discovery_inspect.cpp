#include "Discovery.hpp"
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("Usage: navi33_discovery_inspect FILE\n", stderr);
        return 2;
    }
    FILE* file = std::fopen(argv[1], "rb");
    if (!file) { std::perror("open discovery"); return 2; }
    constexpr size_t limit = 1024 * 1024;
    std::vector<uint8_t> bytes(limit + 1);
    const size_t size = std::fread(bytes.data(), 1, bytes.size(), file);
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    if (failed || size > limit) {
        std::fputs("discovery read failed or input exceeds 1 MiB\n", stderr);
        return 2;
    }
    cherryred::navi33::Discovery discovery;
    const auto error = discovery.initialize(bytes.data(), size);
    if (error != cherryred::navi33::DiscoveryError::None) {
        std::fprintf(stderr, "rejected: %s\n", cherryred::navi33::discoveryErrorName(error));
        return 1;
    }
    std::printf("{\"validation_scope\":\"STRUCTURE_ONLY\",\"hardware_initialization_allowed\":false,"
                "\"binary_offset\":0,\"table_version\":%u,\"die_id\":%u,\"ips\":[",
                static_cast<unsigned>(discovery.tableVersion()),
                static_cast<unsigned>(discovery.dieId()));
    for (size_t i = 0; i < discovery.count(); ++i) {
        cherryred::navi33::DiscoveryRecord record;
        if (!discovery.at(i, record)) return 1;
        std::printf("%s{\"hw_id\":%u,\"instance\":%u,\"version\":\"%u.%u.%u\","
                    "\"attributes\":%u,\"num_bases\":%u,\"bases\":[",
                    i ? "," : "", static_cast<unsigned>(record.hwId),
                    static_cast<unsigned>(record.instance), static_cast<unsigned>(record.major),
                    static_cast<unsigned>(record.minor), static_cast<unsigned>(record.revision),
                    static_cast<unsigned>(record.attributes), static_cast<unsigned>(record.baseCount));
        for (size_t j = 0; j < record.baseCount; ++j)
            std::printf("%s%u", j ? "," : "", static_cast<unsigned>(record.bases[j]));
        std::fputs("]}", stdout);
    }
    std::puts("]}");
    return 0;
}
