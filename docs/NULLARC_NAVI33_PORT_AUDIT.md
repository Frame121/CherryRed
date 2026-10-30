# nullarc → RX 7600 / Navi 33 Port Audit

Date: 2026-10-29
Target: AMD Radeon RX 7600 (PCI 1002:7480, Navi 33, GFX11 / gfx1102)
Reference: https://github.com/luuumi-dev/nullarc

## Finding

nullarc demonstrates a working macOS Tahoe graphics stack on one tested AMD Radeon 780M (Phoenix/Hawk Point APU, gfx1103, GC 11.0.1, DCN 3.1.4). Its README reports WindowServer compositing and Metal-over-Vulkan on that reference machine. This is strong architectural evidence, but it is not RX 7600 support.

The upstream README explicitly says other 780M/760M machines are untested. The installation guide identifies Radeon 780M/760M devices (including PCI IDs 1002:15bf and 1002:1900); it does not list RX 7600 / 1002:7480 as a supported target.

## Portability assessment

| Component | nullarc reference | RX 7600 requirement | Port decision |
|---|---|---|---|
| GPU identity | Phoenix/Hawk Point APU, gfx1103 / GC 11.0.1 | Navi 33, gfx1102 / GC 11.0.2 | Do not reuse fixed identity values; derive and validate target IP discovery |
| Memory model | UMA; source comments describe 6 GiB carve-out and 256 MiB BAR0 aperture | Discrete 8 GiB VRAM with PCIe BAR/ReBAR geometry | Do not reuse UMA addresses, memory budgets, or BAR assumptions |
| Display | DCN 3.1.4, tested internal eDP panel | DCN 3.2.1; board-specific DP/HDMI routing | Treat display bring-up as a separate target-specific milestone |
| PSP / firmware | Phoenix firmware chain and tested APU initialization | PSP 13.0.7 and Navi 33 firmware family | Revalidate exact blobs, firmware interfaces, memory placement and command sequencing |
| SMU / power | APU-specific power-management path | SMU 13.0.7 with discrete-GPU board/power constraints | No copying of mailbox sequences without source trace and failure handling |
| GPUVM / queues | nullarc implements GPUVM and queue/submit path for its target | Must validate Navi 33 GMC/GFX/SDMA/MES behavior and address spaces | Reuse design concepts and host-side test patterns, not assumptions or hardcoded addresses |
| Metal / RADV | gfx1103 RADV Darwin port + Metal translation plugin | gfx1102 RADV support and Darwin winsys compatibility must be proved | Keep this downstream of stable GPU initialization and scanout |

## License gate

The upstream README states that nullarc-authored code such as `src/phxbringup` and `tools/phxctl` is MIT or GPL-2.0-only at the recipient's option. It separately states that `src/phxaccel`, `src/nmplugin`, and `tools/nvmtl-patches` derive from NullMoth and are PolyForm Noncommercial 1.0.0 (no commercial use). Mesa patches and shader translator have separate licenses and notices.

Before copying any implementation into CherryRed, preserve attribution and notices, identify the exact source file and applicable license, and avoid treating the whole repository as one uniformly licensed codebase. This audit does not copy upstream source into the project.

## Safe implementation sequence

1. Keep CherryRed's current Navi 33 probe opt-in, read-only, and fail-closed.
2. Add a source-to-target matrix backed by the Linux amdgpu IP-discovery evidence and exact firmware request callsites.
3. Convert any candidate portable parsing/state logic into isolated host tests; no MMIO writes in these tests.
4. Build a Navi 33-specific memory plan from captured RX 7600 PCI/BAR/VRAM evidence; never reuse the APU's UMA defaults.
5. Design and review PSP/SMU startup as a target-specific state machine with bounded waits, timeout/error propagation, and rollback before enabling hardware writes.
6. Establish a macOS host with the intended Tahoe SDK/MacKernelSDK and compile a minimal kext before hardware experiments.
7. Only after those gates pass, test one hardware stage at a time on recoverable boot media: PSP/firmware, memory/GART, queues, display scanout, then acceleration/Metal.

## Current status

- Upstream source was cloned locally for read-only inspection at the Windows host's temporary directory; no upstream files were copied into CherryRed.
- Confirmed upstream target: Radeon 780M / gfx1103, GC 11.0.1, DCN 3.1.4; reported tested system is Firebat U6C with macOS Tahoe 26.6.2.
- Confirmed target difference: RX 7600 is Navi 33 / gfx1102, PCI 1002:7480, with a discrete VRAM/BAR topology.
- No macOS build, RX 7600 hardware initialization, display output, GPU acceleration, or Metal test has been performed by this audit.
- No commit or push was made.

## Upstream references

- https://github.com/luuumi-dev/nullarc
- https://github.com/luuumi-dev/nullarc/blob/main/README.md
- https://github.com/luuumi-dev/nullarc/blob/main/docs/INSTALL.md
- https://github.com/luuumi-dev/nullarc/blob/main/src/phxbringup/PhxBringup.cpp
- https://github.com/luuumi-dev/nullarc/blob/main/src/amd/rdna3_ip_blocks.cpp
- https://github.com/luuumi-dev/nullarc/blob/main/LICENSE
- https://github.com/luuumi-dev/nullarc/blob/main/THIRD_PARTY_NOTICES.md
