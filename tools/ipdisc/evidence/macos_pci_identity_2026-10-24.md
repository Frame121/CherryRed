# macOS RX 7600 PCI identity observation

**Evidence ID:** RX7600-PCI-001  
**Observed:** 2026-10-24 (Asia/Bangkok)  
**Target:** macOS 26.7.1, build 25G241, x86_64  
**Custodian:** Authorized connected Mac; observed through Remote Desktop Commander

## Evidence record

- **Claim:** The GFX0 PCI endpoint reported by this macOS boot has vendor/device 1002:7480, revision 0xcf, and subsystem vendor/device 148c:2421.
- **Artifact path:** tools/ipdisc/evidence/macos_pci_identity_2026-10-24.md (transcribed observation; raw full IORegistry dump was not retained).
- **Source repository:** LOCAL_MACOS_HOST (system report, not a source repository).
- **Source revision:** macOS 26.7.1 (25G241).
- **Source section:** IORegistry GFX0 properties and system_profiler SPDisplaysDataType.
- **Checksum:** NOT_COMPUTED (no raw dump file retained).
- **Applies to:** TEST_ENVIRONMENT:macOS 26.7.1 (25G241), x86_64; PCI endpoint 1002:7480 / subsystem 148c:2421.
- **Verification method:** ioreg -r -n GFX0 -l -w0; system_profiler SPDisplaysDataType; ioreg -r -c IOAccelerator -l; kmutil showloaded; public Metal enumeration with swift tools/diagnostics/macos_metal_probe.swift.
- **Verification status:** TARGET_TESTED for the narrow claim that these OS interfaces report these fields on this boot (2026-10-24). This is not a driver or acceleration test.

## Observed values

- IORegistry vendor-id bytes 02100000 decode to PCI vendor 0x1002.
- IORegistry device-id bytes 80740000 decode to PCI device 0x7480.
- IORegistry revision-id bytes cf000000 decode to revision 0xcf.
- IORegistry subsystem-vendor-id bytes 8c140000 decode to 0x148c.
- IORegistry subsystem-id bytes 21240000 decode to 0x2421.
- IORegistry compatible includes pci148c,2421 and pci1002,7480.
- system_profiler reports PCIe, x8 link width, 7 MB VRAM, AMD vendor 0x1002, device 0x7480, revision 0x00cf, and the display field “Kernel Extension Info: No Kext Loaded”.
- Separately, kmutil showloaded reports com.apple.kext.AMDSupport (7.0.1) loaded, and IORegistry shows an active AMDSupport service plus an IONDRVFramebuffer named .Display_boot.
- The filtered IOAccelerator query returned no services. Metal returned zero devices and no default device.

## Limits and next evidence required

The retail board make/model and VBIOS identity remain unverified. These PCI properties do not contain the board IP Discovery table and do not establish its IP block versions, firmware requirements, or macOS compatibility. The PCI endpoint is identified from the OS report, but the subsystem tuple alone has not been independently attributed to a retail card model.

No PCI BAR was mapped, no GPU register or VRAM was read, no firmware was accessed, and no driver was built or loaded during this observation. A provenance-documented IP Discovery artifact is still required before designing GPU initialization. This observation does not demonstrate hardware acceleration.
