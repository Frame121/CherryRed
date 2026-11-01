# RX 7600 userspace GPU validation

Date: 2026-10-23

## OpenGL — PASS
Command: `glxinfo -B`
- Direct rendering: Yes
- Vendor: AMD (`0x1002`)
- Device: AMD Radeon RX 7600 (`0x7480`)
- Renderer: `radeonsi, navi33, ACO`
- Mesa: `26.2.5` (package `1:26.2.5-1`)
- OpenGL core profile: 4.6
- OpenGL ES 3.2 reported
- Video memory reported: 8192 MB
- `ldd /usr/lib/dri/radeonsi_dri.so` reported no missing shared libraries.

This is positive evidence of a functioning Linux OpenGL hardware-accelerated userspace path, not just kernel detection.

## Vulkan — NOT VERIFIED
- `vulkaninfo` executable was not found.
- `/usr/share/vulkan/icd.d` contained no ICD files at time of check.
- `pacman -Q vulkan-radeon` did not return an installed package.

Therefore Vulkan userspace is currently not verified and appears not installed/configured. No packages were installed or changed as part of this audit.

## Scope
These tests validate Linux Mesa/DRM operation on this host. They do not establish macOS compatibility or Cobalt64 driver readiness.
