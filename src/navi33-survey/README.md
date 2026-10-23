# navi33-survey

Read-only PCI survey kext for the AMD Radeon RX 7600 (Navi 33, RDNA 3, `1002:7480`).

**Status: UNTESTED. Never compiled, never loaded.** It is a first scaffold, not a driver.

## What it does
- Matches only `1002:7480`, and only when the boot-arg `navi33survey=1` is set.
- Logs (via `IOLog`): PCI ids/revision/subsystem, BAR physical addresses and sizes,
  PCIe link speed/width, Resizable BAR capability and current size.

## What it does NOT do
- No register BAR mapping, no MMIO reads or writes, no VRAM access, no firmware, no display.
- It does not initialise the GPU and gives no acceleration.

## Build / test (not done yet)
Needs a macOS host with MacKernelSDK and an x86_64 Hackintosh with an RX 7600.
Load via OpenCore, boot with `navi33survey=1`, then read the log:
`log show --last boot | grep Navi33Survey` (or `sudo dmesg | grep Navi33Survey`).

## Next
Use real data from `RX7600-collect/collect_rx7600.sh` (Linux Live USB) to confirm IP versions
before adding any register access.
