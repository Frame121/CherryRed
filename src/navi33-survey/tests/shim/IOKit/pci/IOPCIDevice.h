#pragma once
#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <map>
#include <vector>
constexpr unsigned kIOPCIConfigVendorID=0, kIOPCIConfigDeviceID=2,
 kIOPCIConfigRevisionID=8, kIOPCIConfigSubSystemVendorID=0x2c,
 kIOPCIConfigSubSystemID=0x2e, kIOPCIConfigBaseAddress0=0x10;
class IOPCIDevice : public IOService {
public:
 std::map<unsigned,uint32_t> values{{0,0x1002},{2,0x7480},{0xb,3}};
 std::vector<unsigned> reads;
 IOByteCount rebar=0x200;
 UInt8 pcie=0x40;
 uint32_t header=0x10015;
 uint32_t configRead32(unsigned o) { reads.push_back(o); return values[o]; }
 uint16_t configRead16(unsigned o) { return configRead32(o); }
 uint8_t configRead8(unsigned o) { return configRead32(o); }
 uint32_t extendedConfigRead32(IOByteCount o) { return configRead32(o); }
 uint32_t findPCICapability(uint8_t, UInt8* o) { *o=pcie; return pcie?0x10:0; }
 uint32_t extendedFindPCICapability(uint32_t id, IOByteCount* o) {
   if(id!=0u-0x15u) return 0;
   *o=rebar; return header;
 }
 IOMemoryDescriptor* getDeviceMemoryWithRegister(UInt8) { return nullptr; }
};
