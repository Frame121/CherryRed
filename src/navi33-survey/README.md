# navi33-survey

Read-only PCI survey kext for the AMD Radeon RX 7600 (Navi 33, RDNA 3, `1002:7480`).

**Status: experimental diagnostic service.** Host regression tests and real
MacKernelSDK header compilation are available; macOS CI builds and signs the
kext. Loading on physical hardware remains unverified. This is not an accelerated
graphics driver. Unlike Navi33Probe, this service attaches and registers after
its survey, preserving the original survey lifecycle.

## What it does
- Probe and start both require display class 0x03.
- Matches only `1002:7480`, and only when the boot-arg `navi33survey=1` is set.
- Logs (via `IOLog`): PCI ids/revision/subsystem, BAR physical addresses and sizes,
  PCIe link speed/width, Resizable BAR capability and current size.

## What it does NOT do
- No register BAR mapping, no MMIO reads or writes, no VRAM access, no firmware, no display.
- It does not initialise the GPU and gives no acceleration.

## Build / test
Host tests (IOKit doubles; no hardware or Apple ABI validation):
```sh
cmake -S src/navi33-survey -B build/navi33-survey
cmake --build build/navi33-survey
ctest --test-dir build/navi33-survey --output-on-failure
```

On macOS with Xcode Command Line Tools and MacKernelSDK pinned to
`3f750085caa17ec3a7880f11c11bf4f48cd6a164`:
```sh
make -C src/navi33-survey MKSDK=/absolute/path/to/MacKernelSDK
```
The output is `src/navi33-survey/build/Navi33Survey.kext`, ad-hoc signed.
GitHub Actions publishes `Navi33Survey-experimental` with a ZIP retaining
executable permissions. A successful build does not establish hardware compatibility.

The PCIe capability pointer uses the SDK's UInt8 API. ReBAR lookup uses the
negated extended capability ID required by IOPCIFamily. All entries (up to six)
are bounded by 4 KiB config space and the next capability, with BAR index,
duplicate, count, unreadable value and supported-size checks. Size encodings
beyond the 28-bit capability bitmap are reported as unsupported. BAR addresses
come only from existing IOKit descriptors; no sizing writes or mapping occur.

Physical testing requires an x86_64 macOS host with an RX 7600.
Load via OpenCore, boot with `navi33survey=1`, then read the log:
`log show --last boot | grep Navi33Survey` (or `sudo dmesg | grep Navi33Survey`).

## Next
Use real data from `RX7600-collect/collect_rx7600.sh` (Linux Live USB) to confirm IP versions
before adding any register access.
