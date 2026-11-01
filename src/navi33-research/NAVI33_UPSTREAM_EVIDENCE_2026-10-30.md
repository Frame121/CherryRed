# Navi 33 upstream evidence notes

Reviewed: 2026-10-30
Target: Radeon RX 7600, PCI 1002:7480, subsystem 148c:2421, reported Navi 33 / GFX11

This is a research record, not a hardware support claim. Firmware filenames below are candidates from upstream firmware inventories; they are NOT an approved RX 7600 firmware manifest until the Linux amdgpu ASIC/IP selection path is traced for the exact ASIC and board revision.

## Confirmed target facts

- The project inventory identifies the target as PCI vendor/device 1002:7480 and subsystem 148c:2421.
- ROCm's public architecture mapping associates device 0x7480 with gfx1102 / Navi 33. This corroborates the ISA family, not every IP block revision on this board.
- Linux amdgpu is organized by individually versioned IP blocks. AMD's kernel documentation notes that two ASICs can share an IP version while differing elsewhere, and that SoC-specific reset/register access remains separate.
- For Navi and newer, GC and SDMA are attached through GCHUB; PSP firmware validation and TMR protection are part of the platform security model. A userspace metadata check cannot replace PSP authentication or hardware bring-up.

## Firmware inventory evidence — candidates only

Upstream linux-firmware WHENCE lists these GFX11-related families:
- GC: gc_11_0_1_* and gc_11_0_2_* (including IMU, ME, MEC, MES/MES1, PFP, RLC variants)
- Display microcontroller: dcn_3_2_1_dmcub.bin
- PSP: psp_13_0_4_ta.bin and psp_13_0_4_toc.bin (the listing also contains other PSP 13.0.x families)
- SDMA: sdma_6_0_1.bin
- VCN: vcn_4_0_2.bin

A historical linux-firmware submission added GC 11.0.1, PSP 13.0.4 TA/TOC, SDMA 6.0.1 and VCN 4.0.2 together. This establishes that the blobs exist in the redistributable firmware ecosystem; it does NOT establish that all of them are selected by RX 7600's ASIC path. Do not infer an RX 7600 manifest from filename similarity.

A third-party mac-amdgpu project documents GFX11 firmware families as gc_11_*, psp_13_*, smu_13_* and sdma_6_*. Its own target mapping explicitly identifies RX 7900-class gfx1100 as GC 11.0.0 / PSP 13.0.0 / SMU 13.0.0 / SDMA 6.0.0. That mapping must not be copied to Navi 33 gfx1102. The project's own documentation states its firmware is sourced verbatim from linux-firmware and points to WHENCE and LICENSE.amdgpu for provenance/licensing.

### Required before a real firmware loader

1. Trace the exact RX 7600 ASIC selection in upstream Linux (PCI ID, revision, board/subsystem, amdgpu IP-block selection, PSP/SMU/GC/SDMA IP versions and firmware request names).
2. Obtain a pinned linux-firmware commit and its WHENCE entry for each selected blob.
3. Record exact file SHA-256, size, version and license text; do not treat a caller-provided digest or license-approved flag as proof.
4. Establish PSP load order, TOC format, firmware header ABI, memory placement, authentication and failure/recovery behavior from matching upstream source.
5. Keep all firmware operations disabled in CherryRed until the above are independently reviewed.

## IP Discovery and ABI

Linux amdgpu's amdgpu_discovery.c contains IP discovery parsing and common register definitions; generic ip_discovery.bin is a firmware input in some configurations, but the actual target's IP list/revisions must be captured from the target card or traced through the exact ASIC initialization code. Generic GFX11 labels do not determine the board's PSP, SMU, SDMA, DCN, VCN or JPEG revisions.

No RX 7600-specific IP Discovery binary, binary dump, complete IP revision table, PSP ABI, SMU message ABI, or firmware manifest has yet been validated in this repository. The prior host-side IP Discovery parser tests validate malformed-input bounds only; they do not prove the target card's discovery table.

## Six-layer implementation boundary

| Layer | Evidence-backed status | Safe next deliverable |
|---|---|---|
| 1. Navi33 IP Discovery / firmware ABI | Family and candidate firmware families identified; exact target IP revisions and firmware selection still unverified | Trace upstream ASIC selection; add pinned manifest with provenance and negative tests |
| 2. PCI transport / Navi33 backend | Existing DriverKit probe matches the target and queries BAR metadata only | Define separate Navi33 backend interfaces and typed BAR capability contract; keep MMIO disabled |
| 3. Firmware loader / init state machine | Host-side descriptor validation exists; no firmware parsing/authentication/loading | Model explicit PSP/SMU/GC/SDMA phases, prerequisites, timeout/error/rollback states; no device I/O |
| 4. VRAM/GART / queue submission | Host arithmetic models exist only | Add evidence-backed address-space and ring contracts; no page-table writes, doorbells or packet submission |
| 5. IOAccelerator | No RX 7600 accelerator service/user-client integration | Document required macOS interfaces and entitlement/ABI constraints; do not expose a fake accelerator |
| 6. Metal / display | No Metal device, AIR compiler path, framebuffer, AGDC or display integration | Treat as blocked until a validated kernel backend and command submission exist |

## Architectural consequences

- Keep 1002:7480 on a dedicated Navi 33 / GFX11 route. Never route it into src/navi48-bringup (Navi 48 / GFX12).
- PCI match and BAR metadata are discovery only. They are not permission to map or write MMIO.
- PSP firmware verification/TMR, SMU power/clock management, GC/SDMA scheduling, memory translation, reset and fault handling are coupled dependencies. A partial command queue is not a working GPU.
- Metal integration is downstream of a real kernel accelerator and userspace shader/runtime stack. metal2vulkan or a Metal-to-Vulkan translation layer cannot supply the missing AMD PCI driver, firmware, VRAM/GART, GPU scheduler, or display engine.
- Keep the existing -radvesa boot setting and EFI untouched. Do not change SIP/AMFI, install/activate an unsigned dext, load firmware, write registers, or reboot as part of this research-only stage.

## Sources

- Linux kernel amdgpu discovery implementation: https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c
- Linux kernel amdgpu driver architecture: https://github.com/torvalds/linux/blob/master/Documentation/gpu/amdgpu/driver-core.rst
- linux-firmware WHENCE inventory: https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware/+/6569484e6b57bc1c688b383ba6719bf2e25574c4/WHENCE
- linux-firmware historical GFX11 firmware addition: https://lists.ubuntu.com/archives/kernel-team/2023-February/137279.html
- linux-firmware license criteria: https://kernel.googlesource.com/pub/scm/linux/kernel/git/firmware/linux-firmware/+/cc116cc451f80e2e86847a67bcf0a8a26a96b4c0/LICENSE-CRITERIA.md
- mac-amdgpu firmware provenance and family notes: https://github.com/lemonade-sdk/mac-amdgpu/blob/main/firmware/README.md
- ROCm gfx1102 device mapping: https://github.com/ROCm/rocminfo/blob/amd-staging_deprecated/rocminfo/rocm_agent_enumerator

## Current conclusion

The research has advanced enough to establish a GFX11/Navi 33 firmware family hypothesis and a strong architecture boundary, but not enough to safely implement a real RX 7600 firmware loader or hardware backend. The next engineering step is source tracing of the exact Navi 33 ASIC/IP selection and a pinned, board-attributed firmware manifest. All executable work in this stage remains host-only.


## metal2vulkan compatibility audit (steelbrain/metal2vulkan)

Repository README and API documentation describe an alpha Rust workspace with three published crates: a Metal AIR/metallib front end, a Metal-shaped translator that emits SPIR-V and pipeline/binding plans, and a runtime that implements Metal-like objects over a Vulkan device. The runtime requires a Vulkan 1.2 device and a Vulkan loader. The repository states that unsupported inputs are refused rather than silently translated incorrectly, and that its API/CLI/JSON/SPIR-V output may change during 0.x.

Practical consequence for CherryRed:
- It can be a future user-space Metal compatibility layer only after a functioning Vulkan device/driver exists.
- It does not implement PCI discovery, Navi 33 register programming, PSP/SMU firmware loading, VRAM/GART, GPU scheduling, reset/fault recovery, or macOS display/WindowServer integration.
- It cannot create a Vulkan 1.2 device when macOS has no RX 7600 kernel driver and no Vulkan ICD capable of driving that GPU.
- A software Vulkan implementation might satisfy some API calls on the CPU, but would not provide RX 7600 hardware acceleration.
- Its alpha status, Rust 1.98+ requirement and GPL-2.0-or-later OR LGPL-3.0-or-later licensing need to be considered before integration.

Assessment: potentially useful downstream once CherryRed exposes a real Vulkan-capable RX 7600 device, but not a shortcut around the missing Navi 33 kernel/firmware/display stack. Do not integrate it as a driver or claim that it enables Metal on the current macOS installation.

Source: https://github.com/steelbrain/metal2vulkan


## Physical IP Discovery intake update — 2026-10-30

The statement above describing the absence of a target IP Discovery binary is
superseded by the later same-day recovery and strict offline intake recorded
in `tools/ipdisc/evidence/RX7600_IP_DISCOVERY_ANALYSIS_2026-10-30.md`.
The preserved 10,240-byte capture has SHA-256
`4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`, parses
strictly into 39 records across one die, and directly reports GC 11.0.2.
The accompanying source manifest/logs record the matching RX 7600 PCI and
subsystem IDs under Linux AMDGPU. Provenance remains UNVERIFIED because those
records are not cryptographically authenticated. This updates the hardware
inventory evidence only; it does not supply a VBIOS, authenticate firmware
requirements, establish macOS support, or create an IOAccelerator/Metal device.
