#pragma once
#include <cstdint>
using UInt8 = uint8_t;
using SInt32 = int32_t;
using IOByteCount = unsigned long;
#define OSDeclareDefaultStructors(T)
#define OSDefineMetaClassAndStructors(T, B)
#define OSDynamicCast(T, p) dynamic_cast<T *>(p)
class IOService {
public:
 virtual ~IOService() = default;
 virtual IOService* probe(IOService*, SInt32*) { return this; }
 virtual bool start(IOService*) { ++starts; return startResult; }
 virtual void stop(IOService*) {}
 void registerService() { ++registrations; }
 int starts=0, registrations=0;
 bool startResult=true;
};
