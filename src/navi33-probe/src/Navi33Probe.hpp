#ifndef CHERRYRED_NAVI33_PROBE_HPP
#define CHERRYRED_NAVI33_PROBE_HPP

#include <IOKit/IOService.h>

// Diagnostic matching only. Never attaches to or initializes a GPU.
class Navi33Probe : public IOService {
    OSDeclareDefaultStructors(Navi33Probe)

public:
    IOService* probe(IOService* provider, SInt32* score) override;
    bool start(IOService* provider) override;
};

#endif
