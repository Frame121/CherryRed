# CherryRed / RX 7600 research checkpoint — 2026-10-23

## Repository and safety boundary

- Repository path: `D:\\ALL\\Framelpo ALL Project\\Hackintosh AMD Driver\\Cobalt64`.
- Branch: `main`; HEAD: `970257f` (`Rebrand main as CherryRed and rewrite README`).
- Existing research changes are intentionally uncommitted and preserved. Nothing was staged, committed, reset, or cleaned in this session.
- Windows inventory still identifies AMD Radeon RX 7600 `1002:7480`, subsystem `148C:2421`, revision `CF`, driver `32.0.31041.1004`.
- No board-attributed IP Discovery binary, exact IP versions, or firmware request/load evidence is available. Do not invent records or enable Navi33 hardware dispatch.

## Verification executed in this session

| Check | Observed result |
|---|---|
| Python IP Discovery parser | 30 passed, 0 failed |
| Windows driver scanner tests | 6 passed, 1 skipped (symlink creation unavailable on this Windows environment) |
| Debug IP Discovery bounds tests | Passed: 8 fixture cases + 512 malformed inputs |
| Debug bounds security tests | Passed: 10 groups, synthetic data only |
| Debug Navi33 driver-core/lifecycle tests | 32 passed, 0 failed |
| Debug Navi33 research tests | 35 passed, 0 failed |
| Release IP Discovery bounds tests | Passed: 8 fixture cases + 512 malformed inputs |
| Release Navi33 driver-core/lifecycle tests | 32 passed, 0 failed |
| Release Navi33 research tests | 35 passed, 0 failed |
| `git diff --check` | Passed; Git emitted only an LF-to-CRLF working-copy warning for `tools/ipdisc/WORKLOG.md` |

The ASan executable was not rerun in this session; its previously recorded startup failure must not be described as a passing ASan run. CTest was not rerun because CMake/CTest are not available through the current PATH; existing test executables were invoked directly.

## Evidence review

- `tools/ipdisc/evidence/windows_probe.json` documents Windows PnP/CIM metadata only.
- `tools/ipdisc/evidence/windows_driver_scan.json` records four aligned IPDS signature offsets in the installed `amdkmdag.sys`, but no valid strict-parser binary candidate.
- A public Linux hardware report supports generic Linux `amdgpu` support for PCI `1002:7480`; that is not a capture from this physical board and cannot substitute for its IP Discovery table. See https://linuxgaminghw.com/components/amd-navi-33-radeon-rx-7600-7600-xt-7600m-xt-7600s-7700s-pro-w7600
- Linux upstream source describes generic version-driven discovery and dispatch, not the target board's actual IP versions. The kernel's raw `ip_discovery` path is Linux-specific; WSL's `/dev/dxg` is not native amdgpu binding to this GPU.

## Next safe action

Continue source review, strict parser fuzz/parity testing, and documentation. To make progress on the hardware map, obtain a provenance-documented IP Discovery artifact from the actual RX 7600 via an already-authorized native Linux capture or user-supplied artifact. Do not write custom BAR/MMIO probing, install a dump tool, change firmware, add `1002:7480` to Navi48, or implement hardware dispatch without the required evidence and separate authorization.

## Current limitation

This is portable research/parser groundwork, not a working RX 7600 macOS driver. macOS build, kernel ABI, GPU initialization, firmware loading, display output, acceleration, and Metal remain unverified.
