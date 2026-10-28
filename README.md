# RedNavi

A native macOS display driver for AMD **Navi 32 (RDNA 3)** GPUs, starting with the **Radeon RX 7800 XT** (`1002:747E`).

Goal: a real framebuffer on the card, driven by our own kext (DCN 3.2 display engine), with no GOP, VESA or firmware framebuffer in the way. Acceleration (Metal) comes after, and is where we need help.

## Status

**Pre-alpha, first pictures on 2026-10-28:** the kext drives a 1080p60 DisplayPort test pattern and then an image it wrote into VRAM and scans out through a display plane, on a real RX 7800 XT. There is no framebuffer for macOS yet. The probe kext brings up the card's PSP and our own DMCUB display firmware, but nothing drives a display yet. There is no release. What exists today:

| Milestone | What | State |
|---|---|---|
| M0 | Repo, CI (macOS runner), build scripts | Done, CI green (Debug and Release) |
| M1 | Probe kext: matches `1002:747E`, maps the register BAR, reads the IP discovery table | Done: ran on a real RX 7800 XT, versions recorded in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| M1.5 | Probe stage 2: read-only PSP and SMU mailbox registers | Done: PSP bootloader ready, sOS not running, SMU alive |
| M2a | PSP bring-up and DMCUB load, as opt-in probe stages (PSP firmware chain, secure OS, command ring, `LOAD_TOC`, `SETUP_TMR`, DMCUB via the PSP) | Done on a real RX 7800 XT: the secure OS answers commands, and our DMCUB firmware runs and answers a mailbox command (see [docs/PSP_BRINGUP.md](docs/PSP_BRINGUP.md), [docs/DMUB_BRINGUP.md](docs/DMUB_BRINGUP.md)) |
| M2b | SMU 13 bring-up, and the LinuxKPI-style compatibility layer that replaces the probe's hand-written register code | Not started |
| M3 | Native framebuffer: port of amdgpu DC (DCN 3.2) + `IOFramebuffer` | Not started |
| M4 | GFX11, MES, SDMA 6, GPU VM, IH ring, user client | Not started |
| M5 | RADV (Mesa) running on macOS through a libdrm_amdgpu shim | Not started |
| M6 | Metal plugin on top of RADV + accelerator kext for WindowServer | Not started |

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

## Trying the probe

Load `RedNavi.kext` on a machine where the 7800 XT is **not** the display you depend on. By default the probe only reads:

```
ioreg -rn RedNaviProbe -l -x > probe.txt
python3 tools/decode_discovery.py probe.txt
```

It publishes the IP discovery table (`ip-discovery`), PSP/SMU registers (`mailbox-regs`), MMHUB and DMCUB registers and the VBIOS read from the PCI ROM as properties. `IOLog` output may not show up in `log show` (cause unknown), so read the properties.

The stages that **write to the GPU** (PSP load, ring, TMR, DMCUB load) are off unless enabled with `rednavi_*` boot-args, and need firmware headers generated locally from linux-firmware (`tools/gen_psp_fw_header.py`, `tools/gen_dmub_fw_header.py`); firmware is not redistributed here. The boot-args are listed in [docs/PSP_BRINGUP.md](docs/PSP_BRINGUP.md) and [docs/DMUB_BRINGUP.md](docs/DMUB_BRINGUP.md). They can hang the machine: use a system whose display does not depend on this card, with a USB copy of your EFI.

Please post decoded output in an issue, together with your board and BIOS settings (Above 4G Decoding, Resizable BAR).

## Contributing

We want contributors. Good first areas, each of which will get an issue:

- LinuxKPI-style compatibility layer for XNU (memory, locks, workqueues, dma-fence, firmware loading)
- PSP 13 / SMU 13 bring-up
- DMUB firmware load and DCN 3.2 modeset
- `IOFramebuffer` glue
- Decoding the IP discovery dump (`tools/`)

Please do not copy code from projects with restrictive licences (NullMoth's kexts and plugin are PolyForm Noncommercial). The Metal translator in that project is LGPL-3.0 and may be reused as a separate component.

## License

BSD-3-Clause for the code in this repository. amdgpu register definitions and behaviour documented in Linux are MIT. GPU firmware is not included and is fetched from linux-firmware.
