#ifndef COBALT64_NAVI33_QUEUE_HPP
#define COBALT64_NAVI33_QUEUE_HPP

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

// RunNavi33QueueStage — Stage 5.5 / C3:
//   Command submission proof: resumes SDMA0 instance 0 on a ring in VRAM,
//   executes a linear write packet (0xDEADBEEF to target VRAM), tests GFX
//   ring command processor, and verifies writeback.
//   On success, publishes "Navi33Bringup,QueueVerified" = true.
void RunNavi33QueueStage(IOPCIDevice *pci, IOService *publisher);

#endif // COBALT64_NAVI33_QUEUE_HPP
