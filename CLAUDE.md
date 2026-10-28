# RedNavi: notes for coding agents and contributors

Native macOS display driver for the AMD Radeon RX 7800 XT (Navi 32, RDNA 3, PCI `1002:747E`). Read `README.md` for the state, then `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`, `docs/TEST_MACHINES.md` and the bring-up notes in `docs/`.

## Layout

- `RedNavi/`: the probe kext (`kern_probe.cpp`, matches the GPU; every stage that writes to the card is opt-in through a `rednavi_*` boot-arg), `smu13.cpp`, the generated `dcn_reglist.h` and `dcn_replay.h`.
- `RedNaviFB/`: the `IOFramebuffer` kext. It is separate because IOGraphicsFamily is in the system kernel collection, which OpenCore cannot link against; see `docs/DP_BRINGUP.md` for installing it.
- `compat/`: LinuxKPI-style compatibility layer (`docs/COMPAT.md`). `tests/host/`: host tests, run with `make test`.
- `tools/`: Python helpers (IP discovery decoder, VBIOS and EDID parsers, firmware header generators, Linux register-dump scripts).
- `docs/logs/`: raw dumps from real runs, with vendor firmware and monitor serial numbers left out.

## Build

```
sh scripts/bootstrap.sh     # fetches MacKernelSDK
make DEBUG=1                # or: make
make test                   # host tests for the compat layer
```

`make` builds `RedNavi.kext` and `RedNaviFB.kext` under `build/<Config>/`. Firmware headers (`RedNavi/psp_*.h`, `RedNavi/dmub_*.h`) are generated locally from linux-firmware with `tools/gen_*_fw_header.py` and are git-ignored; without them the corresponding stages are compiled out.

## Conventions

- Code, comments, docs and commit messages are in English, in plain prose.
- Licence is BSD-3-Clause. Do not copy code from projects with other or restrictive licences (NullMoth's kexts and plugin are PolyForm Noncommercial; also do not copy from NootedRed or LegacyRed). Write from scratch; FreeBSD's drm-kmod and the Linux amdgpu driver are references for ideas and register definitions.
- Do not commit vendor firmware, the VBIOS image or raw EDIDs (they carry serial numbers). The probe publishes them as ioreg properties; cut them out of logs before committing.
- Say what is verified on hardware and what is not. Mark anything read from the Linux sources but never run as unverified.

## Hard-won facts

- OpenCore refuses to inject a kext whose `OSBundleLibraries` is incomplete or names a library outside the boot kernel collection. The only trace is `Prelinked injection ... - Invalid Parameter` in the OpenCore log.
- `IOLog` output may not show up in `log show`; read the properties on the `RedNaviProbe` node (`ioreg -rn RedNaviProbe -l -x`).
- Reading all registers of the DCN list (`tools/dcn32_reglist.txt`) hangs the machine (clock- or power-gated blocks). Read only registers of live blocks.
- `IODisplay::readFramebufferEDID` calls `getAttributeForConnection` with a NULL value pointer.
- The verbose boot console lags the kernel: the last visible line can be stale.

## Safety

The probe writes to a real GPU from the kernel and can hang the machine. Test on a system whose display does not depend on this card, keep SSH enabled, and keep a USB copy of the OpenCore EFI without the kexts.
