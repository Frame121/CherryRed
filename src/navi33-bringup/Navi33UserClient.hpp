#ifndef COBALT64_NAVI33_USER_CLIENT_HPP
#define COBALT64_NAVI33_USER_CLIENT_HPP

#include <IOKit/IOUserClient.h>
#include "Navi33ClientContracts.hpp"

// Navi33UserClient — the C4 userspace bridge.
//
// Allows userspace runtimes (such as Mesa/RADV or test tools) to open a connection
// to the GPU compute engine via IOServiceOpen and IOConnectCallMethod.
// Implements the N33N ABI with per-client VRAM budget isolation.

namespace cobalt64::navi33 {

constexpr uint32_t kN33nUcType = 0x4E33334Eu; // 'N33N'
constexpr uint32_t kN33nAbiVersion = 1u;

enum {
    N33N_SEL_HELLO      = 0,
    N33N_SEL_QUERYINFO  = 1,
    N33N_SEL_READREGS   = 2,
    N33N_SEL_BOCREATE   = 3,
    N33N_SEL_BOFREE     = 4,
    N33N_SEL_GEMVA      = 5,
    N33N_SEL_CTX        = 6,
    N33N_SEL_SUBMIT     = 7,
    N33N_SEL_WAITSEQ    = 8,
    N33N_SEL_COUNT      = 9,
};

struct N33nInfo {
    uint32_t vendorId;
    uint32_t deviceId;
    uint32_t vramTotalMb;
    uint32_t vramVisibleMb;
    uint32_t gfxTarget;
    uint32_t abiVersion;
    uint64_t clientVramBudget;
    uint64_t clientVramAllocated;
};

}  // namespace cobalt64::navi33

class Navi33UserClient : public IOUserClient {
    OSDeclareDefaultStructors(Navi33UserClient)
    using super = IOUserClient;

public:
    bool initWithTask(task_t owningTask, void *securityID, UInt32 type) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    IOReturn clientClose() override;

    IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
                            IOExternalMethodDispatch *dispatch, OSObject *target,
                            void *reference) override;

private:
    task_t task_ {nullptr};
    cobalt64::research::client::ClientBudget budget_;
    uint64_t lastSeqno_ {0};

    IOReturn methodHello(uint64_t clientAbi, uint64_t *outKernelAbi, uint64_t *outVmid);
    IOReturn methodQueryInfo(cobalt64::navi33::N33nInfo *outInfo);
    IOReturn methodBoCreate(uint64_t byteCount, uint32_t alignment, uint32_t *outHandle);
    IOReturn methodBoFree(uint32_t handle);
    IOReturn methodSubmit(uint64_t *outSeqno);
    IOReturn methodWaitSeq(uint64_t seqno, uint64_t timeoutNs, uint64_t *outRetired);
};

#endif // COBALT64_NAVI33_USER_CLIENT_HPP
