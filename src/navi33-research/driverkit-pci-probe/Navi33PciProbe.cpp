#include <os/log.h>
#include <DriverKit/IOUserServer.h>
#include <PCIDriverKit/IOPCIDevice.h>

#include "Navi33PciProbe.h"

kern_return_t
IMPL(Navi33PciProbe, Start)
{
    kern_return_t result = Start(provider, SUPERDISPATCH);
    if (result != kIOReturnSuccess) {
        return result;
    }

    // This is deliberately a read-only survey. It records assigned PCI BAR
    // metadata only; it does not map apertures, touch MMIO, reset the device,
    // load firmware, or initialize the GPU.
    auto *pci = (IOPCIDevice *)provider;
    result = pci->Open(this, 0);
    if (result != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT,
               "CherryRed Navi33 survey: IOPCIDevice::Open failed (0x%x).",
               result);
        return result;
    }

    uint8_t validBars = 0;
    for (uint8_t bar = 0; bar < 6; ++bar) {
        uint8_t memoryIndex = 0;
        uint8_t barType = 0;
        uint64_t barSize = 0;
        kern_return_t barResult =
            pci->GetBARInfo(bar, &memoryIndex, &barSize, &barType);
        if (barResult == kIOReturnSuccess && barSize != 0) {
            ++validBars;
            os_log(OS_LOG_DEFAULT,
                   "CherryRed Navi33 survey: BAR%u index=%u type=0x%x size=%llu bytes.",
                   bar, memoryIndex, barType, barSize);
        }
    }

    pci->Close(this, 0);
    os_log(OS_LOG_DEFAULT,
           "CherryRed Navi33 read-only PCI survey completed; valid BARs=%u. No GPU initialization or acceleration performed.",
           validBars);
    return kIOReturnSuccess;
}

kern_return_t
IMPL(Navi33PciProbe, Stop)
{
    return Stop(provider, SUPERDISPATCH);
}
