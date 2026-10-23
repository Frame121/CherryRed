#include "Navi33Probe.hpp"
#include <IOKit/pci/IOPCIDevice.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
bool argumentPresent = true;
std::uint32_t argumentValue = 1;
std::string messages;

void require(bool condition) {
    if (!condition) {
        std::fputs("Navi33Probe check failed\n", stderr);
        std::abort();
    }
}

class Device final : public IOPCIDevice {
public:
    std::uint32_t identity = 0x74801002;
    std::uint32_t classRevision = 0x030000cf;
    std::uint32_t subsystem = 0x2421148c;
    std::vector<unsigned> reads;

    std::uint32_t configRead32(unsigned offset) override {
        reads.push_back(offset);
        switch (offset) {
            case 0x00: return identity;
            case 0x08: return classRevision;
            case 0x2c: return subsystem;
            default: std::abort();
        }
    }
};

void probeAndDecline(IOService* provider) {
    Navi33Probe probe;
    SInt32 score = 731;
    require(probe.probe(provider, &score) == nullptr);
    require(score == 731);
}
}  // namespace

int PE_parse_boot_argn(const char* name, void* value, int length) {
    require(std::strcmp(name, "navi33probe") == 0);
    require(length == static_cast<int>(sizeof(argumentValue)));
    if (!argumentPresent) return 0;
    std::memcpy(value, &argumentValue, sizeof(argumentValue));
    return 1;
}

void IOLog(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    const int count = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    require(count >= 0 && static_cast<unsigned>(count) < sizeof(buffer));
    messages += buffer;
}

int main() {
    // No opt-in: no configuration reads, including when argument is present
    // with a value other than exactly 1.
    Device disabled;
    argumentPresent = false;
    probeAndDecline(&disabled);
    argumentPresent = true;
    for (auto value : {0u, 2u, 0xffffffffu}) {
        argumentValue = value;
        probeAndDecline(&disabled);
    }
    require(disabled.reads.empty() && messages.empty());
    argumentValue = 1;

    IOService otherProvider;
    probeAndDecline(nullptr);
    probeAndDecline(&otherProvider);
    require(messages.empty());

    // Reject other AMD families, the audio function, non-AMD vendors, and
    // conventional absent-device responses even if start/probe is called directly.
    for (auto identity : {0x75501002u, 0x75511002u, 0xab301002u,
                          0x748010deu, 0xffffffffu, 0u}) {
        Device rejected;
        rejected.identity = identity;
        probeAndDecline(&rejected);
        require(rejected.reads == std::vector<unsigned>{0x00});
        require(messages.empty());
    }

    Device nonDisplay;
    nonDisplay.classRevision = 0x040300cf;
    probeAndDecline(&nonDisplay);
    require(nonDisplay.reads == (std::vector<unsigned>{0x00, 0x08}));
    require(messages.empty());

    Device target;
    probeAndDecline(&target);
    require(target.reads == (std::vector<unsigned>{0x00, 0x08, 0x2c}));
    require(messages.find("PCI 1002:7480 subsystem 148c:2421 revision cf class 030000") !=
            std::string::npos);
    require(messages.find("identity only, IP/firmware unknown") != std::string::npos);
    require(messages.find("hardware initialization/display/Metal unavailable; declining attachment") !=
            std::string::npos);

    // Subsystem/revision are observations, not a hard-coded board identity.
    messages.clear();
    Device anotherBoard;
    anotherBoard.subsystem = 0x56781234;
    anotherBoard.classRevision = 0x03800001;
    Navi33Probe probe;
    require(probe.probe(&anotherBoard, nullptr) == nullptr);
    require(messages.find("subsystem 1234:5678 revision 01 class 038000") !=
            std::string::npos);

    // A bypass of matching must never enter IOService::start (which returns
    // true in this shim), read configuration, or produce a success claim.
    const auto priorMessages = messages;
    Device bypass;
    require(!probe.start(&bypass));
    require(!probe.start(&otherProvider));
    require(!probe.start(nullptr));
    require(bypass.reads.empty() && messages == priorMessages);
    return 0;
}
