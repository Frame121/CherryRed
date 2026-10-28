# Roadmap

Current goal: **M3, a native framebuffer** on the RX 7800 XT. M4 to M6 are open for contributors.

| Milestone | Scope | Test |
|---|---|---|
| M0 | Repo, CI on a macOS runner, build scripts | CI green |
| M1 | Probe kext: PCI info, BARs, IP discovery dump (read-only). **Done 2026-10-27**, plus read-only PSP/SMU, MMHUB, DMCUB and VBIOS reads | `ioreg` shows the IP versions and registers |
| M2 | LinuxKPI-style compatibility layer for XNU; PSP 13 and SMU 13 bring-up; DMUB firmware load. **Done in the probe (2026-10-28): PSP chain, secure OS, ring, TOC, TMR, DMCUB load and mailbox.** Remaining: SMU 13 (first messages written, untested) and the compat layer (started, see `COMPAT.md`) | Firmware loaded, log of PSP/SMU state |
| M3 | Port of amdgpu DC for DCN 3.2 and an `IOFramebuffer` subclass. **2026-10-28: a test pattern, an image scanned out from VRAM, then the macOS desktop through `RedNaviFB` (an `IOFramebuffer`), on a DisplayPort monitor, one fixed 1080p60 mode**; mode setting from the EDID, more connectors and a real DC port remain | A native mode on one connector, desktop visible |
| M4 | GFX11, MES, SDMA 6, GPU VM, IH ring, user client (buffers, VA, submit, sync) | Command submission works |
| M5 | RADV on macOS via a libdrm_amdgpu shim | Offscreen Vulkan compute and triangle |
| M6 | Metal plugin on RADV (reusing the LGPL translator) and an accelerator kext for WindowServer | Metal app renders |

Notes:
- The path from the running DMCUB to a first picture is laid out step by step in `DC_PLAN.md`.
- PSP initialises before display and graphics; the display firmware (DMUB) is loaded through it.
- Each milestone should leave a test log format that users can paste into an issue.
- See `ARCHITECTURE.md` for what was learned from NullMoth and `COMPAT.md` for the compatibility layer.
