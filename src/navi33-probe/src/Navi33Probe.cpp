#include "Navi33Probe.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <pexpert/pexpert.h>

OSDefineMetaClassAndStructors(Navi33Probe, IOService)

IOService* Navi33Probe::probe(IOService* provider, SInt32* score) {
    (void)score;
    uint32_t enabled = 0;
    if (!PE_parse_boot_argn("navi33probe", &enabled, sizeof(enabled)) ||
        enabled != 1) {
        return nullptr;
    }

    auto* pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) return nullptr;

    const uint32_t vendorDevice = pci->configRead32(kIOPCIConfigVendorID);
    if ((vendorDevice & 0xffffu) != 0x1002u ||
        (vendorDevice >> 16) != 0x7480u) {
        return nullptr;
    }

    // Conventional PCI configuration reads only. No BAR/ROM mapping, command
    // register writes, memory/bus-master enable, firmware, or MMIO access.
    const uint32_t classRevision = pci->configRead32(kIOPCIConfigRevisionID);
    if ((classRevision >> 24) != 0x03u) return nullptr;  // display controller
    const uint32_t subsystem = pci->configRead32(kIOPCIConfigSubSystemVendorID);

    IOLog("CherryRed Navi33Probe: PCI %04x:%04x subsystem %04x:%04x "
          "revision %02x class %06x; identity only, IP/firmware unknown, "
          "hardware initialization/display/Metal unavailable; declining attachment\n",
          static_cast<unsigned>(vendorDevice & 0xffffu),
          static_cast<unsigned>(vendorDevice >> 16),
          static_cast<unsigned>(subsystem & 0xffffu),
          static_cast<unsigned>(subsystem >> 16),
          static_cast<unsigned>(classRevision & 0xffu),
          static_cast<unsigned>(classRevision >> 8));

    // Decline matching so this diagnostic cannot win ownership from a display
    // driver. A log is an observation, not a successful driver start.
    return nullptr;
}

bool Navi33Probe::start(IOService* provider) {
    (void)provider;
    // Also refuse a direct call that bypasses probe().
    return false;
}
