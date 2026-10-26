# RedNavi

A native macOS display driver for AMD **Navi 32 (RDNA 3)** GPUs, starting with the **Radeon RX 7800 XT** (`1002:747E`).

Goal: a real framebuffer on the card, driven by our own kext (DCN 3.2 display engine), with no GOP, VESA or firmware framebuffer in the way. Acceleration (Metal) comes after, and is where we need help.

## Status

**Pre-alpha, nothing works yet.** There is no release. What exists today:

| Milestone | What | State |
|---|---|---|
| M0 | Repo, CI (macOS runner), build scripts | Written, not yet built by CI |
| M1 | Probe kext: matches `1002:747E`, maps the register BAR, logs PCI info and the IP discovery table | Written, never run on hardware |
| M2 | Compatibility layer (LinuxKPI-style), PSP 13 + SMU 13 + DMUB firmware bring-up | Not started |
| M3 | Native framebuffer: port of amdgpu DC (DCN 3.2) + `IOFramebuffer` | Not started |
| M4 | GFX11, MES, SDMA 6, GPU VM, IH ring, user client | Not started |
| M5 | RADV (Mesa) running on macOS through a libdrm_amdgpu shim | Not started |
| M6 | Metal plugin on top of RADV + accelerator kext for WindowServer | Not started |

## Why not Apple's drivers (like CarrizoRed does)?

macOS has no RDNA 3 support at all and Apple's Metal shader compiler has no GFX11 back end, so there is nothing to impersonate. The approach that has been shown to work for an unsupported GPU is the one used by [NullMoth](https://github.com/nullmoth/nvidia-macos-driver) for NVIDIA: host the vendor's open kernel driver inside a kext, run the vendor's Vulkan driver in userspace, translate Metal to Vulkan on top.

For AMD that means: **amdgpu in a kext, RADV (ACO) in userspace, Metal -> SPIR-V translation on top.** See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Hardware this is developed on

ASUS ROG STRIX Z490-G GAMING, i7-10700K, **Intel UHD 630 as the primary display**, Sapphire RX 7800 XT as the development target (PCIe 4.0 x16). See [docs/TEST_MACHINES.md](docs/TEST_MACHINES.md).

Navi 32 boards (RX 7700 XT / 7800 XT) are in scope; reports from other 7000-series owners are welcome once the probe works.

## Building

Needs macOS with the Xcode command-line tools, or just push to GitHub: `.github/workflows/build.yml` builds Debug and Release on a macOS runner and uploads the kext.

```
sh scripts/bootstrap.sh
make DEBUG=1
```

## Trying the probe (read-only)

Load `RedNavi.kext` on a machine where the 7800 XT is **not** the display you depend on. Then:

```
log show --last boot --predicate 'eventMessage CONTAINS "RedNavi"'
```

Please post the output in an issue, together with your board and BIOS settings (Above 4G Decoding, Resizable BAR).

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
