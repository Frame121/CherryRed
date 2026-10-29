# RedNavi

A native macOS display driver for AMD **Navi 32 (RDNA 3)** GPUs, starting with the **Radeon RX 7800 XT** (`1002:747E`).

> **For developers only.** This is a research prototype, not a driver you can install and use. It needs OpenCore, SIP turned off and boot-args, it writes to the GPU from the kernel, and it can hang or crash your machine. Run it only on a system whose display does not depend on this card, with a backup EFI. Nothing here is accelerated: no Metal, no 3D.

Goal: a real framebuffer on the card, driven by our own kext (DCN 3.2 display engine), with no GOP, VESA or firmware framebuffer in the way. Acceleration (Metal) comes after, and is where we need help.

## Status

**Pre-alpha, 2026-10-28:** the macOS Sequoia desktop is shown on a real RX 7800 XT, over DisplayPort, through our own kext. The kext loads the card's PSP firmware chain and our own DMCUB display firmware, trains the DisplayPort link, lights the pipe in a mode read from the monitor's EDID (tested from 1024x768 to 1920x1080 at 144 Hz) and scans out a linear framebuffer in VRAM that WindowServer draws into; the resolution and refresh rate can be changed from System Settings while it runs. The picture does not come from the firmware's GOP/VESA framebuffer: without the kext the card's output stays dark.

What this is **not**: there is no acceleration and no Metal. WindowServer renders on the CPU (expect fans spinning during animations). Modes come from the EDID, but only on one DisplayPort connector, and the display-mode bandwidth values are borrowed from a 1080p60 capture, so unusual timings may underflow. macOS also lists the firmware's boot framebuffer as a second, nameless display. There is no cursor plane, clock or power management (SMU), hot-plug, audio or HDMI, and only one DisplayPort connector has been tested. It needs OpenCore, and the framebuffer kext needs SIP off (details below). It can hang the machine. There is no release.

| Milestone | What | State |
|---|---|---|
| M0 | Repo, CI (macOS runner), build scripts | Done, CI green (Debug and Release) |
| M1 | Probe kext: matches `1002:747E`, maps the register BAR, reads the IP discovery table | Done: ran on a real RX 7800 XT, versions recorded in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| M1.5 | Probe stage 2: read-only PSP and SMU mailbox registers | Done: PSP bootloader ready, sOS not running, SMU alive |
| M2a | PSP bring-up and DMCUB load, as opt-in probe stages (PSP firmware chain, secure OS, command ring, `LOAD_TOC`, `SETUP_TMR`, DMCUB via the PSP) | Done on a real RX 7800 XT: the secure OS answers commands, and our DMCUB firmware runs and answers mailbox commands (see [docs/PSP_BRINGUP.md](docs/PSP_BRINGUP.md), [docs/DMUB_BRINGUP.md](docs/DMUB_BRINGUP.md)) |
| M2b | SMU 13 bring-up, and the LinuxKPI-style compatibility layer that replaces the probe's hand-written register code | Started: the compat layer exists with host tests ([docs/COMPAT.md](docs/COMPAT.md)); the SMU answers messages after its firmware is loaded through the PSP. Clock and power management on top of that is not written yet |
| M3 | Native framebuffer: DCN 3.2 display engine + `IOFramebuffer` | Works on one DisplayPort connector: link training, a pipe state replayed from a Linux capture with the mode-dependent registers recomputed (OTG, DP stream, DTO, plane sizes, DISPCLK), the monitor's EDID modes, and live mode changes through `RedNaviFB` (see [docs/DC_PLAN.md](docs/DC_PLAN.md), [docs/DP_BRINGUP.md](docs/DP_BRINGUP.md)). Missing: more connectors, a real DC port instead of a replay, exact bandwidth (DML) values |
| M4 | GFX11, MES, SDMA 6, GPU VM, IH ring, user client | Started: the PSP loads the graphics, SDMA, MES, IMU and RLC firmware and the RLC boots (GC alive, 2026-10-29); SDMA0 executes a first command from a ring in VRAM (2026-10-29); no graphics ring, doorbells or interrupts yet ([docs/COMPAT.md](docs/COMPAT.md)) |
| M5 | RADV (Mesa) running on macOS through a libdrm_amdgpu shim | Not started |
| M6 | Metal plugin on top of RADV + accelerator kext for WindowServer | Not started |

M4 and M5 give you compute and Vulkan on the card but do not change how the desktop feels; a smooth, GPU-composited desktop needs M6.

## Why not Apple's drivers (like CarrizoRed does)?

macOS has no RDNA 3 support at all and Apple's Metal shader compiler has no GFX11 back end, so there is nothing to impersonate. The approach that has been shown to work for an unsupported GPU is the one used by [NullMoth](https://github.com/nullmoth/nvidia-macos-driver) for NVIDIA: host the vendor's open kernel driver inside a kext, run the vendor's Vulkan driver in userspace, translate Metal to Vulkan on top.

For AMD that means: **amdgpu in a kext, RADV (ACO) in userspace, Metal -> SPIR-V translation on top.** See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Hardware this is developed on

ASUS ROG STRIX Z490-G GAMING, i7-10700K, **Intel UHD 630 as the primary display**, Sapphire RX 7800 XT as the development target (PCIe 4.0 x16). See [docs/TEST_MACHINES.md](docs/TEST_MACHINES.md).

Navi 32 boards (RX 7700 XT / 7800 XT) are in scope; reports from other 7000-series owners are welcome now that the probe works.

## Building

Needs macOS with the Xcode command-line tools, or just push to GitHub: `.github/workflows/build.yml` builds Debug and Release on a macOS runner and uploads the kext.

```
sh scripts/bootstrap.sh
make DEBUG=1
```

## Trying it

Load `RedNavi.kext` on a machine where the 7800 XT is **not** the display you depend on. By default the probe only reads:

```
ioreg -rn RedNaviProbe -l -x > probe.txt
python3 tools/decode_discovery.py probe.txt
```

It publishes the IP discovery table (`ip-discovery`), PSP/SMU registers (`mailbox-regs`), MMHUB and DMCUB registers and the VBIOS read from the PCI ROM as properties. `IOLog` output may not show up in `log show` (cause unknown), so read the properties.

The stages that **write to the GPU** (PSP load, ring, TMR, DMCUB load, DisplayPort training, the display replay) are off unless enabled with `rednavi_*` boot-args, and need firmware headers generated locally from linux-firmware (`tools/gen_psp_fw_header.py`, `tools/gen_dmub_fw_header.py`); firmware is not redistributed here. The boot-args are listed in [docs/PSP_BRINGUP.md](docs/PSP_BRINGUP.md), [docs/DMUB_BRINGUP.md](docs/DMUB_BRINGUP.md) and [docs/DP_BRINGUP.md](docs/DP_BRINGUP.md). They can hang the machine: use a system whose display does not depend on this card, with a USB copy of your EFI.

### The framebuffer

With `rednavi_fb=1` (and `rednavi_dcn_replay=14`) the probe publishes where the scanout surface is, and a second kext, `RedNaviFB.kext`, hands it to WindowServer. It is a separate kext because it links against IOGraphicsFamily, which lives in the system kernel collection, so OpenCore cannot inject it. Install it through the auxiliary kernel collection (SIP off, `codesign -f -s -`, `kmutil load -p`), as described in [docs/DP_BRINGUP.md](docs/DP_BRINGUP.md). If macOS also shows a second, nameless display, that is the stock firmware boot framebuffer, not a second output. Boot-arg `-wegnoigpu` (WhateverGreen) switches the Intel iGPU off so the card is the only display.

Please post decoded output in an issue, together with your board and BIOS settings (Above 4G Decoding, Resizable BAR).

## Contributing

We want contributors. Good first areas, each of which will get an issue:

- LinuxKPI-style compatibility layer for XNU (memory, locks, workqueues, dma-fence, firmware loading)
- PSP 13 / SMU 13 bring-up
- DCN 3.2 modeset from the EDID and more connectors (HDMI, second DisplayPort)
- SMU 13 clock and power management
- Cursor plane, vertical blank interrupts, hot-plug
- GFX11 / SDMA bring-up (M4)
- Decoding the IP discovery dump (`tools/`)

Please do not copy code from projects with restrictive licences (NullMoth's kexts and plugin are PolyForm Noncommercial). The Metal translator in that project is LGPL-3.0 and may be reused as a separate component.

## License

BSD-3-Clause for the code in this repository. Register definitions and behaviour taken from the Linux amdgpu driver are MIT, see [NOTICE](NOTICE).

GPU firmware is not included. It comes from linux-firmware under `LICENSE.amdgpu`, which allows redistribution in binary form only, with AMD's notice reproduced, and forbids reverse engineering, decompilation and disassembly. That is why firmware is never committed here and each user generates local headers from files they fetch themselves.
