# Navi 33 DriverKit PCI Probe

This is a build-only Xcode/DriverKit prototype for the RX 7600 board identity
captured on the connected Mac: primary PCI match `1002:7480`, subsystem match
`148c:2421`.

The DriverKit personality is restricted to those PCI IDs. The current
build-only prototype opens the matching provider and reports the assigned BAR
index, type, and size using `GetBARInfo`, then closes the provider. This is
read-only PCI resource inventory: it does not map BAR apertures, read or write
GPU MMIO registers, modify PCI configuration, reset the device, submit commands,
load firmware, or initialize the GPU. Opening the provider has not been tested
on hardware because this bundle is never installed or activated.

## Build check

From the repository root on an Intel Mac with Xcode and the DriverKit SDK:

```sh
src/navi33-research/driverkit-pci-probe/build_compile_check.sh
```

The script runs Apple's IIG compiler and Clang for x86_64 DriverKit 25.5,
compiles both source units, links an unsigned `.dext` bundle, and validates the
property list and target platform. Build products go under the ignored `build/`
directory. It does not create a host app, sign, install, or activate the bundle.

A usable PCI DriverKit extension also needs an app host, the approved
`com.apple.developer.driverkit` and
`com.apple.developer.driverkit.transport.pci` entitlements, code signing, and
user activation. PCI attachment alone would not provide Radeon firmware
startup, GPU memory/queue/command handling, an IOAccelerator service, display
integration, or a Metal device. This prototype is not graphics acceleration.
