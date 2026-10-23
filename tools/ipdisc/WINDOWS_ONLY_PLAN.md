# Cobalt64 — Windows-only development plan

Updated: 2026-10-22

## Goal

Keep research, C++ development, parser tests, artifact validation, and supported GPU capability checks on Windows. Do not require WSL or reboot to Linux for ordinary development.

## What Windows can do now

- Build and test the portable C++ parser with MSVC/CMake.
- Run Python IP Discovery parsing, provenance, hash, bounds, and regression tests.
- Query RX 7600 PCI identity, PnP location, and driver metadata through supported Windows APIs.
- Official AMD HIP SDK documentation lists RX 7600 as RDNA3 gfx1102 with runtime and HIP SDK support. The SDK is not installed here; this is generic software compatibility, not a measured IP Discovery record.
- Scan local driver binaries for aligned IPDS signatures and verify candidates against the strict parser without hardware access.
- Inspect user-provided, provenance-documented artifacts offline and record SHA-256. A VBIOS image must not be assumed to contain the live IP Discovery binary.

## Important limitation

The Linux kernel's amdgpu driver exposes raw IP Discovery data through its amdgpu_discovery debugfs file. Windows' ordinary user-mode device metadata APIs do not provide an equivalent documented raw IP Discovery interface. A custom kernel driver capable of accessing GPU BAR/MMIO or VRAM would be a separate, higher-risk driver project and is not the recommended next step.

## Safe next steps

1. Keep using the read-only Windows inventory: powershell -NoProfile -ExecutionPolicy Bypass -File .\\tools\\ipdisc\\windows_probe.ps1 -OutFile .\\tools\\ipdisc\\evidence\\windows_probe.json.
2. Scan local files only when a concrete candidate exists: python -B tools\\ipdisc\\scan_windows_driver.py PATH_TO_FILE --json tools\\ipdisc\\evidence\\scan.json.
3. If HIP SDK is needed, install it only after reviewing component choices; avoid replacing the working AMD display driver.
4. Keep Navi33 hardware dispatch disabled until real RX 7600 evidence establishes IP block versions and mappings.
5. Do not claim macOS support, GPU acceleration, or successful hardware initialization until tested in a real macOS environment.

## Progress estimate (2026-10-22)

These percentages track different scopes and are estimates, not a claim of working GPU support:

- **Windows-only research/tooling phase: 85%** — parser, C++ parity/bounds tests, read-only Windows inventory, static driver scan, provenance checks, and regression suite are implemented. Remaining work in this phase is further offline hardening and reviewing any independently supplied artifact.
- **End-to-end RX 7600 macOS driver goal: 10%** — portable research/parser groundwork exists, but there is no RX 7600 board-specific IP Discovery capture, no Navi33 hardware initialization, no macOS/Xcode+MacKernelSDK build, no loaded kext, and no acceleration/display/Metal proof. This is not 10% of a validated driver; it is a rough project-stage estimate.

## Latest Windows checkpoint

- RX 7600: PCI\\VEN_1002&DEV_7480&SUBSYS_2421148C&REV_CF; driver 32.0.31041.1004; PnP status OK; PCI bus 3/device 0/function 0.
- Inventory report: tools/ipdisc/evidence/windows_probe.json; SHA-256 218A5AFB7D9B935E18FB2183EC62980FFC904F3BF3BD7AD19B8AE31D2EA9AF19.
- Static scan of installed amdkmdag.sys (SHA-256 92a83d34adb17a8c419a153b62e94e2cf3c478e260571af6699574800af3f3df) found four aligned IPDS strings but zero valid enclosing IP Discovery binaries. Report: tools/ipdisc/evidence/windows_driver_scan.json; SHA-256 9FAC110DB356D6D3B7649F9CFD036B5E7750FCC4D01643105E80B4E21A036AC4.
- Python parser tests: 30/30 passed. Scanner tests: 6 passed, 1 skipped because symlink creation is unavailable. The scanner hashes input before and after scanning, rejects a changed content hash, and refuses to write its JSON report over the input file (including a path resolving to the same file). MSVC Debug/Release CTest: 4/4 each. `git diff --check` passed with existing line-ending warnings.
- HIP SDK / hipInfo.exe is not installed. No SDK or display driver was installed or changed.
- Existing worktree changes remain uncommitted. Do not reset or clean them.
