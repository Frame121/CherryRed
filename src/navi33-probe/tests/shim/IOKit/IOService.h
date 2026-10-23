#pragma once

#include <cstdint>

using SInt32 = std::int32_t;

// Host-only test double. These macros do not model Apple's metaclass ABI.
#define OSDeclareDefaultStructors(type) \
public: \
    type() = default; \
    ~type() override = default;
#define OSDefineMetaClassAndStructors(type, base)
#define OSDynamicCast(type, object) dynamic_cast<type*>(object)

class IOService {
public:
    virtual ~IOService() = default;
    virtual IOService* probe(IOService*, SInt32*) { return this; }
    virtual bool start(IOService*) { return true; }
};
