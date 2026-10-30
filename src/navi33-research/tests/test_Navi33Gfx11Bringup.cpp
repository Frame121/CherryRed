#include "Navi33Gfx11Bringup.hpp"
#include <cstdio>
using namespace cobalt64::research;
namespace {
int failed = 0, total = 0;
void check(bool v, const char* n) {
    ++total; if (!v) { ++failed; std::fprintf(stderr, "FAIL: %s\n", n); }
}
void testInitSequence() {
    Gfx11InitEvidence e{};
    Gfx11InitState s{};
    auto next = [&](Gfx11InitStage stage) { s = advanceGfx11Init(s, stage, e); };
    next(Gfx11InitStage::IdentityVerified);
    check(s.error == Gfx11InitError::MissingIdentityEvidence, "identity evidence gates init");
    e.exactPciIdentity = e.vbiosMatched = true;
    next(Gfx11InitStage::IdentityVerified);
    check(s.stage == Gfx11InitStage::IdentityVerified, "identity stage advances");
    e.firmwareManifestValidated = true; next(Gfx11InitStage::FirmwareVerified);
    e.pspAuthenticated = true; next(Gfx11InitStage::PspReady);
    e.smuResponding = true; next(Gfx11InitStage::SmuReady);
    e.gcIpVerified = e.mesFirmwareVerified = true; next(Gfx11InitStage::GcReady);
    e.sdmaIpVerified = true; next(Gfx11InitStage::SdmaReady);
    e.memoryTopologyVerified = e.gpuvmPolicyVerified = true; next(Gfx11InitStage::MemoryReady);
    e.computeQueuePolicyVerified = true; next(Gfx11InitStage::QueueReady);
    e.hardwareComputeVerified = true; next(Gfx11InitStage::ComputeReady);
    check(s.stage == Gfx11InitStage::ComputeReady && s.transitions == 9,
          "full evidenced state machine reaches compute-ready model");
    auto backwards = advanceGfx11Init(s, Gfx11InitStage::PspReady, e);
    check(backwards.error == Gfx11InitError::InvalidTransition, "init rejects backwards transition");
}
void testMemory() {
    const GpuMemoryRegion region{0x100000, 0x10000, 4096};
    check(validateGpuMemoryRegion(region).valid, "valid aligned memory region");
    check(!validateGpuMemoryRegion({0x100001, 0x10000, 4096}).valid, "unaligned region rejected");
    check(!validateGpuMemoryRegion({0xfffffffffffff000ULL, 0x2000, 4096}).valid,
          "region overflow rejected");
    const GpuMemoryMapping map{0x400000, 0x101000, 0x2000, 2, true, true};
    check(validateGpuMemoryMapping(region, map, 16).valid, "valid VMID mapping");
    check(validateGpuMemoryMapping(region, {0x400000,0x101000,0x2000,16,true,true},16).error ==
          GpuMemoryError::InvalidVmid, "out of range VMID rejected");
    check(validateGpuMemoryMapping(region, {0x400000,0x101000,0x2000,2,false,false},16).error ==
          GpuMemoryError::PermissionsDenied, "no-permission mapping rejected");
    check(validateGpuMemoryMapping(region, {0x400000,0x10f000,0x2000,2,true,true},16).error ==
          GpuMemoryError::OutsideRegion, "out of region mapping rejected");
    check(validateGpuMemoryMapping(region, {0xfffffffffffff000ULL,0x101000,0x2000,2,true,true},16).error ==
          GpuMemoryError::AddressOverflow, "GPUVA overflow rejected");
}
void testQueue() {
    const Gfx11QueueConfig c{0x800000, 4096, 4, 256, 0, 0, 1};
    const Gfx11QueueCursor p{0, 0, 4, 3};
    check(validateGfx11Queue(c,p).valid, "valid GFX11 pipe/queue geometry");
    check(!validateGfx11Queue({0x800001,4096,4,256,0,0,1},p).valid,
          "misaligned ring rejected");
    check(validateGfx11QueueSubmit(c,p,10,5).valid, "monotonic queue submission accepted");
    check(validateGfx11QueueSubmit(c,p,10,4).error == Gfx11QueueError::SequenceRegression,
          "replayed sequence rejected");
    check(validateGfx11QueueSubmit(c,p,1024,5).error == Gfx11QueueError::RingFull,
          "full ring rejected");
    check(validateGfx11QueueCompletion(p,4).valid, "valid fence completion accepted");
    check(!validateGfx11QueueCompletion(p,5).valid, "completion ahead of submit rejected");
}
void testMetalBridge() {
    MetalBridgeEvidence e{};
    MetalBridgeState s{};
    auto tryStage = [&](MetalBridgeStage stage) { s = advanceMetalBridge(s, stage, e); };
    tryStage(MetalBridgeStage::KernelServiceReady);
    check(s.error == MetalBridgeError::MissingKernelService, "kernel service gates bridge");
    e.kernelServicePublished = true; tryStage(MetalBridgeStage::KernelServiceReady);
    e.userClientValidated = true; tryStage(MetalBridgeStage::UserClientReady);
    e.ioAcceleratorRegistered = true; tryStage(MetalBridgeStage::AcceleratorReady);
    e.hardwareComputeVerified = true; tryStage(MetalBridgeStage::MetalDeviceAdvertised);
    check(s.error == MetalBridgeError::MissingFenceSupport,
          "Metal device not advertised without verified fences");
    e.fenceAndInterruptsVerified = true; tryStage(MetalBridgeStage::MetalDeviceAdvertised);
    check(s.stage == MetalBridgeStage::MetalDeviceAdvertised,
          "bridge can advertise only after hardware compute and fence evidence");
}
}
int main() {
    testInitSequence(); testMemory(); testQueue(); testMetalBridge();
    if (failed) { std::fprintf(stderr, "%d/%d failed\n", failed,total); return 1; }
    std::printf("Navi33 GFX11 bring-up foundations: %d passed, 0 failed\n", total);
    return 0;
}
