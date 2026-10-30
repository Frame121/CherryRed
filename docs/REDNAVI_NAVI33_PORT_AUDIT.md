# RedNavi → Navi 33 Port Audit

Date: 2026-10-29
Target: Radeon RX 7600, Navi 33, PCI 1002:7480, subsystem 148c:2421
Reference: https://github.com/Helemen7/RedNavi

## Executive decision

RedNavi is a valuable RDNA 3 macOS research reference, but it is not a drop-in RX 7600 driver. Its reported working display prototype targets RX 7800 XT / Navi 32 (1002:747E) on macOS Sequoia, with a fixed 1920x1080@60 DisplayPort framebuffer. It explicitly has no 3D acceleration or Metal. Do not copy its register-write sequences into the Navi 33 target without a source/version audit and recoverable macOS test hardware.

## Verified reference status (README and source reviewed 2026-10-29)

- M1: PCI/BAR/IP discovery probe tested on a real RX 7800 XT.
- M2a: PSP chain, secure OS command ring, TOC/TMR and DMCUB bring-up reported working on that card.
- M2b: LinuxKPI-style compatibility layer builds and host tests pass; SMU messages have not yet succeeded on hardware (the project's COMPAT.md records a no-response result on 2026-10-29).
- M3: DCN 3.2.0 register-state replay and a separate IOFramebuffer reportedly produce one fixed-mode DisplayPort scanout.
- M4 GFX11/MES/SDMA/GPUVM/interrupts/user client: not started.
- M5 RADV/Vulkan userspace: not started.
- M6 Metal plugin + accelerator integration: not started.
- README warns that this is a pre-alpha research prototype and hardware writes can hang/crash the machine.

References:
- README: https://github.com/Helemen7/RedNavi/blob/main/README.md
- Architecture: https://github.com/Helemen7/RedNavi/blob/main/docs/ARCHITECTURE.md
- Compatibility layer: https://github.com/Helemen7/RedNavi/blob/main/docs/COMPAT.md
- PSP: https://github.com/Helemen7/RedNavi/blob/main/docs/PSP_BRINGUP.md
- DisplayPort: https://github.com/Helemen7/RedNavi/blob/main/docs/DP_BRINGUP.md
- Framebuffer: https://github.com/Helemen7/RedNavi/blob/main/RedNaviFB/kern_fb.cpp

## Hardware/IP mismatch that must remain explicit

| Item | RedNavi reference card | Project target RX 7600 |
|---|---|---|
| ASIC | Navi 32 / RX 7800 XT | Navi 33 / RX 7600 |
| PCI ID | 1002:747E | 1002:7480 (board evidence subsystem 148c:2421) |
| GC | 11.0.3 / gfx1101 | 11.0.2 / gfx1102 |
| SDMA | 6.0.3 | 6.0.2 |
| PSP / SMU | 13.0.10 | 13.0.7 |
| DCN | 3.2.0 | 3.2.1 |
| VCN | 4.0.x variants in reference | 4.0.4 |
| Status | one fixed-mode scanout reported | no target macOS hardware validation yet |

Source of RX 7600 values: this repository's RX7600-Linux-Evidence/firmware-dependency-audit.md, navi33-firmware-manifest.json, and dcn321-dependency-map.md. Treat captured board evidence as authoritative.

## File-by-file port assessment

| RedNavi component | Recommendation for Navi 33 | Gate / reason |
|---|---|---|
| RedNavi/kern_probe.cpp | Use as a reading guide; port small, isolated logic only after API and IP-version review | Contains hardware-specific BAR/register offsets, PSP/SMU stages and opt-in writes; do not wholesale copy |
| compat/include/linux and compat/src | Audit licensing and APIs, then adapt selected abstractions rather than copying the whole layer | Some parts are untested on hardware; DMA physical-address assumptions and incomplete interrupt/fence primitives are explicit limitations |
| RedNavi/smu13.cpp | Research reference only | Its own 2026-10-29 COMPAT.md reports that SMU messages do not respond on the reference card yet |
| RedNaviFB/kern_fb.cpp | Architectural reference for a future IOFramebuffer milestone | Assumes a valid scanout surface and fixed mode; does not implement mode setting, cursor, gamma or vblank |
| DCN 3.2.0 replay and DP training | Do not replay unchanged | Target is DCN 3.2.1; register offsets, transmitter routing, AUX path, timing and firmware must be reconciled against RX 7600/Linux captures |
| PSP firmware protocol | Compare call order and response checks against Linux source for PSP 13.0.7 | Firmware component names/versions and bootloader protocol must match the target |
| RADV / Metal architecture | Keep as a long-term architecture reference only | No RedNavi GFX11, RADV or Metal milestone is implemented yet |

## Existing target project state

- src/navi33-bringup/Navi33Bringup.cpp is deliberately fail-closed: exact boot-arg opt-in, exact PCI/subsystem identity, PCI/BAR/PCIe/ReBAR discovery and explicit markers that no firmware is loaded and no hardware writes are performed.
- The target firmware directory has 14 Navi 33 blobs, and the manifest/evidence tests exist.
- src/dcn321/dcn321_regs.h has been generated from the DCN 3.2.1 source headers.
- The bring-up roadmap says the host-side research/scaffold has not been compiled/validated as a working kext on the target macOS/RX 7600 hardware. A connected Windows host is not a substitute for macOS kernel ABI or GPU validation.

## Safe next implementation sequence

1. Keep the existing Navi 33 kext read-only. Do not add register writes as part of this audit.
2. Create a per-function port matrix mapping RedNavi's probe/PSP/DMCUB/DP/framebuffer operations to the target Linux source callsites and Navi 33 IP versions.
3. Add host-side tests for the matrix/manifest consistency and any portable parsing logic; never claim these prove hardware compatibility.
4. Before hardware writes: pin the Linux reference commit, verify PSP 13.0.7 call order and required buffer/VRAM ownership, audit firmware licensing/provenance, define timeouts and rollback, and test on a recoverable macOS machine with a separate primary display.
5. Prove safe resource discovery, then PSP/firmware startup, then memory/GART/interrupts and command submission, then DCN 3.2.1 scanout, then acceleration/userspace. Keep Metal last.

## Honest completion accounting

This audit improves the research plan and identifies a more relevant RDNA 3 reference. It does not add GPU initialization, load firmware on macOS, compile a working kext on macOS, light the RX 7600 display, or implement Metal. Report research/scaffold progress separately from end-to-end driver progress; never use a single percentage without naming what it measures.