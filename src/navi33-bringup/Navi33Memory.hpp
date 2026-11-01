#ifndef COBALT64_NAVI33_MEMORY_HPP
#define COBALT64_NAVI33_MEMORY_HPP

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

// RunNavi33MemoryStage — Stage 5.4 / C2:
//   GFXHUB v3.0 GART aperture setup in VRAM, VMID 0 page table,
//   system aperture covering VRAM, L1 TLB and L2 cache enable,
//   and CPU memory write/read persistence verification.
//   On success, publishes "Navi33Bringup,MemoryVerified" = true.
void RunNavi33MemoryStage(IOPCIDevice *pci, IOService *publisher);

#endif // COBALT64_NAVI33_MEMORY_HPP
