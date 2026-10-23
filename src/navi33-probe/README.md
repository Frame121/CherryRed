# Navi 33 read-only macOS PCI diagnostic

`Navi33Probe.kext` is the first isolated macOS-facing diagnostic for the
RX 7600 family research track. **It is not a graphics driver with working
display or Metal acceleration.** Its macOS build and hardware behavior have
not yet been validated. Host tests do not establish kext loadability.

## Contract

- Disabled unless the integer boot argument `navi33probe=1` is present.
- The personality matches only `1002:7480`. Runtime checks require the same
  identity and a display-controller base class (`03`). No Navi 31, Navi 32,
  Navi 48, or audio-function match is provided.
- Reads only conventional PCI configuration dwords at offsets `00`, `08`,
  and `2c`: vendor/device, class/revision, and subsystem vendor/device.
- Logs raw identity; does not infer an RX 7600 versus RX 7600 XT model,
  board vendor name, VRAM size, IP versions, firmware, or compatibility.
  The existing physical-card evidence covers an RX 7600 with `1002:7480`;
  RX 7600 XT hardware remains unverified. Other PCI IDs are rejected.
- Always returns `nullptr` from `probe`, allowing IOKit to continue matching
  other drivers. `start` always returns false, including direct calls.
  No successful service attachment or registry capability is published.
- No MMIO, BAR/ROM mapping, PCI command writes, device enable, firmware
  loading, interrupts, framebuffer, user client, or GPU commands.

This target links neither the portable `std::`-based research model nor
Navi48 bring-up code. The original research core remains an offline model;
its `hardwareInitializationAllowed` invariant is unchanged. This diagnostic
does not authenticate evidence or implement a production hardware backend.

## Build on an x86_64 macOS validation host

Use Xcode Command Line Tools and external
[MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) at
`3f750085caa17ec3a7880f11c11bf4f48cd6a164` (the pin used by CI).
The build links its `libkmod.a`. No firmware is required for this target.
From the repository root:

```sh
make -C src/navi33-probe MKSDK=/absolute/path/to/MacKernelSDK
```

Output: `src/navi33-probe/build/Navi33Probe.kext`, with an ad-hoc signature.
The Makefile targets the macOS 11 x86_64 kernel ABI. This is a build target,
not a claim of verified compatibility with macOS 11 or later versions.
No installation or system-security changes are performed by the build.

On an authorized macOS test host, first validate the bundle, kernel symbols,
and matching behavior using that host's kext validation process. Loading
must be tested separately; an ad-hoc signature does not guarantee permission
to load. With the diagnostic loaded and `navi33probe=1` set through the test
host's existing boot configuration, matching should emit a kernel log:

```text
CherryRed Navi33Probe: PCI 1002:7480 subsystem <vendor>:<device> revision <rev> class <class>; identity only, IP/firmware unknown, hardware initialization/display/Metal unavailable; declining attachment
```

Collect logs using the test host's kernel-log facilities; for example:

```sh
log show --last boot --style compact --predicate 'eventMessage CONTAINS "CherryRed Navi33Probe:"'
```

A missing log is inconclusive: it can indicate a missing boot argument,
failure to load, unavailable matching, or log retention. This service should
not appear as an attached `Navi33Probe` driver in IORegistry, since matching
is intentionally declined. Repeated matching can emit repeated observations.

## Host regression tests

Requires CMake 3.16+, a C++17 compiler, and Python 3. From the repository root:

```sh
cmake -S src/navi33-probe -B build/navi33-probe
cmake --build build/navi33-probe --config Debug
ctest --test-dir build/navi33-probe -C Debug --output-on-failure
```

The tests compile the actual diagnostic C++ source against a narrow IOKit
test double. They verify disabled behavior, wrong provider/vendor/device/class
rejection, raw reporting, unchanged match score, refusal to attach, and direct
start refusal. The test double exposes no hardware-write APIs. A separate
test checks plist match scope and agreement with kmod identity/version.
The host shim uses C++ RTTI for its dynamic cast; it does not reproduce
Apple's metaclass machinery, kmod glue, kernel linking, or signing. These
checks do not replace a build against real Apple kernel headers.

## Remaining work for a graphics driver

The [Navi 33 evidence requirements](../../tools/ipdisc/RX7600_NAVI33_EVIDENCE.md)
still apply. Next gates are a real-card IP Discovery artifact with provenance,
verified per-IP register and firmware mapping, macOS compile/link validation,
then staged memory/interrupt/firmware initialization on actual hardware.
Display bring-up and Metal integration require separate implementation and
hardware validation after those gates. A PCI observation clears none of them.
