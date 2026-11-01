//
//  Navi33UserClient.cpp — Navi 33 User Client implementation.
//
#include "Navi33UserClient.hpp"
#include "Navi33Accelerator.hpp"

#include <IOKit/IOLib.h>
#include <string.h>

#define N33UC_LOG(fmt, ...) IOLog("Navi33UserClient: " fmt "\n", ##__VA_ARGS__)

using cobalt64::navi33::kN33nUcType;
using cobalt64::navi33::kN33nAbiVersion;
using cobalt64::navi33::N33nInfo;
using cobalt64::research::client::ClientError;
using cobalt64::research::client::ClientPolicy;

OSDefineMetaClassAndStructors(Navi33UserClient, IOUserClient)

static ClientPolicy defaultPolicy() {
    ClientPolicy p;
    p.abiVersion = kN33nAbiVersion;
    p.maxBufferBytes = 256ull * 1024ull * 1024ull; // 256 MiB per single BO
    p.maxClientBytes = 6ull * 1024ull * 1024ull * 1024ull; // 6 GiB per client VRAM budget
    p.minAlignment = 4096u;
    p.maxAlignment = 2u * 1024u * 1024u; // 2 MiB large page alignment
    p.maxOpenHandles = 4096u;
    return p;
}

bool Navi33UserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
    (void)securityID;
    if (!super::init())
        return false;

    if (type != 0 && type != kN33nUcType) {
        N33UC_LOG("init refused: unrecognized connection type 0x%x", type);
        return false;
    }

    task_ = owningTask;
    budget_ = cobalt64::research::client::ClientBudget(defaultPolicy());
    lastSeqno_ = 0;
    return true;
}

bool Navi33UserClient::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    N33UC_LOG("client session opened (ABI v%u)", kN33nAbiVersion);
    return true;
}

void Navi33UserClient::stop(IOService *provider)
{
    N33UC_LOG("client session closing (released %llu bytes)",
              (unsigned long long)budget_.allocatedBytes());
    super::stop(provider);
}

IOReturn Navi33UserClient::clientClose()
{
    terminate();
    return kIOReturnSuccess;
}

IOReturn Navi33UserClient::methodHello(uint64_t clientAbi, uint64_t *outKernelAbi, uint64_t *outVmid)
{
    if (clientAbi != kN33nAbiVersion)
        return kIOReturnUnsupported;
    if (outKernelAbi) *outKernelAbi = kN33nAbiVersion;
    if (outVmid) *outVmid = 0; // VMID 0 for direct engine access
    return kIOReturnSuccess;
}

IOReturn Navi33UserClient::methodQueryInfo(N33nInfo *outInfo)
{
    if (!outInfo)
        return kIOReturnBadArgument;

    memset(outInfo, 0, sizeof(*outInfo));
    outInfo->vendorId = 0x1002;
    outInfo->deviceId = 0x7480;
    outInfo->vramTotalMb = 8192;
    outInfo->vramVisibleMb = 8192; // Resizable BAR enabled
    outInfo->gfxTarget = 1102;     // gfx1102
    outInfo->abiVersion = kN33nAbiVersion;
    outInfo->clientVramBudget = budget_.policy().maxClientBytes;
    outInfo->clientVramAllocated = budget_.allocatedBytes();
    return kIOReturnSuccess;
}

IOReturn Navi33UserClient::methodBoCreate(uint64_t byteCount, uint32_t alignment, uint32_t *outHandle)
{
    if (!outHandle)
        return kIOReturnBadArgument;

    const ClientError err = budget_.allocate(byteCount, alignment, outHandle);
    if (err == ClientError::None)
        return kIOReturnSuccess;
    if (err == ClientError::ClientBudgetExhausted || err == ClientError::HandleTableFull)
        return kIOReturnNoMemory;
    return kIOReturnBadArgument;
}

IOReturn Navi33UserClient::methodBoFree(uint32_t handle)
{
    const ClientError err = budget_.release(handle);
    if (err == ClientError::None)
        return kIOReturnSuccess;
    return kIOReturnNotFound;
}

IOReturn Navi33UserClient::methodSubmit(uint64_t *outSeqno)
{
    ++lastSeqno_;
    if (outSeqno)
        *outSeqno = lastSeqno_;
    return kIOReturnSuccess;
}

IOReturn Navi33UserClient::methodWaitSeq(uint64_t seqno, uint64_t timeoutNs, uint64_t *outRetired)
{
    (void)timeoutNs;
    if (outRetired)
        *outRetired = lastSeqno_;
    if (seqno <= lastSeqno_)
        return kIOReturnSuccess;
    return kIOReturnTimeout;
}

IOReturn Navi33UserClient::externalMethod(uint32_t selector,
                                         IOExternalMethodArguments *arguments,
                                         IOExternalMethodDispatch *dispatch,
                                         OSObject *target,
                                         void *reference)
{
    (void)dispatch; (void)target; (void)reference;
    if (!arguments)
        return kIOReturnBadArgument;

    switch (selector) {
        case cobalt64::navi33::N33N_SEL_HELLO: {
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 2)
                return kIOReturnBadArgument;
            return methodHello(arguments->scalarInput[0],
                               &arguments->scalarOutput[0],
                               &arguments->scalarOutput[1]);
        }
        case cobalt64::navi33::N33N_SEL_QUERYINFO: {
            if (!arguments->structureOutput ||
                arguments->structureOutputSize < sizeof(N33nInfo))
                return kIOReturnBadArgument;
            arguments->structureOutputSize = sizeof(N33nInfo);
            return methodQueryInfo(reinterpret_cast<N33nInfo *>(arguments->structureOutput));
        }
        case cobalt64::navi33::N33N_SEL_BOCREATE: {
            if (arguments->scalarInputCount < 2 || arguments->scalarOutputCount < 1)
                return kIOReturnBadArgument;
            uint32_t handle = 0;
            const IOReturn ret = methodBoCreate(arguments->scalarInput[0],
                                                static_cast<uint32_t>(arguments->scalarInput[1]),
                                                &handle);
            if (ret == kIOReturnSuccess)
                arguments->scalarOutput[0] = handle;
            return ret;
        }
        case cobalt64::navi33::N33N_SEL_BOFREE: {
            if (arguments->scalarInputCount < 1)
                return kIOReturnBadArgument;
            return methodBoFree(static_cast<uint32_t>(arguments->scalarInput[0]));
        }
        case cobalt64::navi33::N33N_SEL_SUBMIT: {
            if (arguments->scalarOutputCount < 1)
                return kIOReturnBadArgument;
            return methodSubmit(&arguments->scalarOutput[0]);
        }
        case cobalt64::navi33::N33N_SEL_WAITSEQ: {
            if (arguments->scalarInputCount < 2 || arguments->scalarOutputCount < 1)
                return kIOReturnBadArgument;
            return methodWaitSeq(arguments->scalarInput[0],
                                 arguments->scalarInput[1],
                                 &arguments->scalarOutput[0]);
        }
        default:
            return kIOReturnUnsupported;
    }
}
