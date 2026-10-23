# RX 7600 firmware and dependency audit

Date: 2026-10-23 (local capture host)
GPU: AMD Navi 33, PCI `1002:7480`, subsystem `148c:2421`
Host kernel: `7.2.11-arch1-1` (`linux 7.2.11.arch1-1`)
Firmware package: `linux-firmware-amdgpu 20260916-1`; meta package `linux-firmware 20260916-1`.

## Runtime evidence

The current boot journal shows `amdgpu` initialized successfully on `0000:03:00.0`; SMU initialization succeeded; Display Core initialized on DCN 3.2.1; GPU rings registered for GFX, compute, SDMA0/1, VCN and JPEG; HMM reports 8176 MB device memory. This is strong evidence the Linux driver loaded its required runtime components and brought up the GPU.

The journal reports `RAP: optional rap ta ucode is not available` and `SECUREDISPLAY: optional securedisplay ta ucode is not available`. They are explicitly optional; the GPU still initialized. Do not classify these as blocking firmware failures.

## Matching firmware payloads present on disk

The following compressed firmware files were found under `/usr/lib/firmware/amdgpu`:

- GC/GFX 11.0.2 family: `gc_11_0_2_imu.bin.zst`, `gc_11_0_2_me.bin.zst`, `gc_11_0_2_mec.bin.zst`, `gc_11_0_2_mes1.bin.zst`, `gc_11_0_2_mes_2.bin.zst`, `gc_11_0_2_mes.bin.zst`, `gc_11_0_2_pfp.bin.zst`, `gc_11_0_2_rlc.bin.zst`
- SDMA 6.0.2: `sdma_6_0_2.bin.zst`
- VCN 4.0.4: `vcn_4_0_4.bin.zst`
- Display microcontroller for DCN 3.2.1: `dcn_3_2_1_dmcub.bin.zst`
- PSP 13.0.7: `psp_13_0_7_sos.bin.zst`, `psp_13_0_7_ta.bin.zst`
- SMU 13.0.7: `smu_13_0_7.bin.zst`

This is an inventory of candidate files consistent with the observed IP versions, not proof that every listed payload was requested or loaded. Kernel journal runtime success is the separate evidence of functional initialization.

## Dependency interpretation

- Required runtime stack observed: PCI device `1002:7480` -> Linux `amdgpu` kernel driver -> AMD IP blocks/firmware -> DRM/KMS display core; HDA function `03:00.1` binds to the GPU display-audio component.
- Host-side packages observed: kernel `7.2.11.arch1-1`, `linux-firmware-amdgpu 20260916-1`.
- Optional missing payloads: RAP TA and secure-display TA, as reported by kernel; not fatal to this boot.
- This audit does not imply these Linux firmware blobs can be used in macOS. Their formats, initialization contracts, licensing/redistribution terms, and OS-specific interfaces must be reviewed independently before any reuse. Do not copy Linux firmware into a kext or bundle it as a macOS dependency based only on filename/version similarity.
- No firmware was flashed, replaced, deleted, or modified by this audit.

## Remaining work before a macOS dependency plan can be called complete

1. Extract exact firmware request names from the full boot log (`amdgpu` debug messages may not print every successful request).
2. Inspect source for the exact Arch kernel build/source commit and map each request to the source's `MODULE_FIRMWARE` / firmware-loading call sites.
3. Separately identify macOS-native equivalents or document that no usable equivalent exists. Linux firmware presence is not a substitute for a macOS driver interface.
