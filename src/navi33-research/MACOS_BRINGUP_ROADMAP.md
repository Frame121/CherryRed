# RX 7600 macOS bring-up roadmap

Date: 2026-10-23
Scope: Radeon RX 7600 / Navi 33 PCI ID 1002:7480
Status: research and host-side validation only; no working macOS driver yet.

## Current verified baseline

- A real RX 7600 was inspected on Linux and its IP Discovery artifact was captured.
- Portable Navi33 research tests pass on the Linux host.
- The read-only Navi33 survey source exists, but has not been compiled or loaded on macOS.
- The current host is Linux, not macOS. Apple SDK and MacKernelSDK are not present in this checkout.
- No code in the Navi33 research track initializes GPU registers, loads GPU firmware, creates a display engine, submits command buffers, or provides Metal acceleration.

## Milestone 0 — repeatable environment preflight

Run from the repository root on the intended macOS validation host:

```sh
sh src/navi33-research/tools/macos_preflight.sh
```

This is read-only. It reports toolchain prerequisites and deliberately does not install software or change security settings. A passing preflight is not a kext build.

## Milestone 1 — compile-only diagnostic

Use the portable-core procedure in `MACOS_VALIDATION_HANDOFF.md` to compile/link the user-space lifecycle tests with Apple Clang. Record macOS version, Xcode/CLT version, SDK path, architecture, compiler command, test output, and binary inspection output. This milestone is not kernel ABI validation.

## Milestone 2 — kernel extension compatibility gate

Before creating a Navi33 kext target:
1. Establish a legally usable, version-pinned MacKernelSDK or supported public SDK arrangement.
2. Prove the exact headers, kernel C++ ABI, and link libraries match the intended macOS version and architecture.
3. Remove unsupported standard-library/virtual-dispatch assumptions from any code intended for the kernel unless the chosen SDK explicitly supports them.
4. Build a minimal no-hardware kext target and inspect its symbols and metadata.
5. Keep the existing Navi48 target separate; never change its match IDs to include Navi33.

Do not treat a successful user-space executable or a Navi48 build as proof that Navi33 kernel code works.

## Milestone 3 — read-only PCI survey

Only after the kernel build gate passes, review the survey code against the exact SDK, then test on a recoverable macOS installation with a known-good boot entry. The survey must remain opt-in and read-only. Collect IOKit logs and compare PCI identity, BAR metadata, PCIe link, and ReBAR data against the existing Linux evidence. If the platform API semantics cannot be proven safe, do not load it.

## Milestone 4 — evidence-backed initialization design

No register writes are authorized by the current project state. Before implementing any hardware initialization, close all of these evidence gates:
- Resolve every IP Discovery HWID and version using source matching the installed Linux kernel or other authoritative source.
- Document exact IP block dependencies and initialization order for Navi 33, not Navi 48.
- Trace each required firmware blob to the exact upstream declaration and request callsite; document provenance and license constraints.
- Review the reset, power-management, VRAM/GART, interrupt, command processor, SDMA, display/DCN, and teardown dependencies.
- Define failure rollback and watchdog/recovery behavior.
- Add offline tests for each parser and state transition before hardware access.

If any dependency is unknown, keep initialization disabled. Linux firmware presence is not proof that firmware can be redistributed or used by a macOS driver.

## Milestone 5 — staged hardware validation

On dedicated test hardware with recoverable boot media, validate one subsystem at a time:
1. Device match and safe service lifecycle.
2. Resource discovery without register writes.
3. Firmware delivery and PSP/SMU startup only after documented prerequisites.
4. VRAM/GMC/GART and interrupt handling.
5. Command submission and synchronization.
6. Display-engine initialization and scanout.
7. Acceleration API integration and finally Metal workloads.

Each stage needs logs, reproducible steps, failure rollback, and a clear PASS/FAIL/SKIP result. Do not enable the next stage based only on compile success.

## Current blocker

The current host has no macOS toolchain, and the project has no verified Navi33 kernel backend. Therefore the next external prerequisite is access to a suitable macOS validation host/toolchain. Meanwhile, Linux-side source tracing and offline tests can proceed, but they cannot establish that macOS boots with accelerated RX 7600 graphics.

## Completion criteria

The phrase “works on macOS” is reserved for reproducible target-hardware evidence: macOS boots reliably, display output works, GPU initialization and memory management survive stress, acceleration is demonstrated, and Metal workloads pass. OpenCore booting with the card present, a kext compiling, or a PCI match alone do not meet this bar.
