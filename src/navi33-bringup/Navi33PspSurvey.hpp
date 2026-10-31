#ifndef COBALT64_NAVI33_PSP_SURVEY_HPP
#define COBALT64_NAVI33_PSP_SURVEY_HPP

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

// Read-only PSP/SMU mailbox survey for the RX 7600 bring-up kext, enabled by
// its own boot argument (navi33psp=1, on top of navi33bringup=1). It maps the
// MMIO register BAR (BAR5) and READS PSP/SMU mailbox registers, publishing
// everything under "Navi33Bringup,PspStage,*" / "Navi33Bringup,PspMailboxRegs".
//
// Zero writes of any kind: the PCI command register is read, never written —
// if PCI memory decode is off the survey aborts instead of enabling it, and
// the zero-write contract of the scaffold still holds.
void SurveyPspMailbox(IOPCIDevice *pci, IOService *publisher);

#endif
