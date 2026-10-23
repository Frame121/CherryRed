#pragma once

#include <IOKit/IOService.h>

inline constexpr unsigned kIOPCIConfigVendorID = 0x00;
inline constexpr unsigned kIOPCIConfigRevisionID = 0x08;
inline constexpr unsigned kIOPCIConfigSubSystemVendorID = 0x2c;

// Intentionally no PCI write, BAR mapping, firmware, or device-enable APIs.
class IOPCIDevice : public IOService {
public:
    virtual std::uint32_t configRead32(unsigned offset) = 0;
};
