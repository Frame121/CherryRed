# RX 7600 (Navi 33) firmware dependency map

Date: 2026-10-30

## Scope and evidence boundary

This map reconciles the 14 firmware names in `RX7600-Linux-Evidence/navi33-firmware-manifest.json` against the locally retained Arch Linux 7.2.11 source snapshot at:

`RX7600-Linux-Evidence/source-investigation/arch-linux-v7.2.11/drivers/gpu/drm/amd/`

The capture and firmware manifest identify the card as RX 7600, PCI ID `1002:7480`, GPU target `gfx1102`, and report GC 11.0.2, SDMA 6.0.2, UVD/VCN 4.0.4, DMU/DCN 3.2.1, MP0/MP1 13.0.7. The source clone commit is recorded in the manifest, but it is not established that this exact source tree produced the installed Arch kernel binary. This is source correlation, not binary provenance.

`MODULE_FIRMWARE` means a kernel module declares a dependency/name. It is not by itself proof the blob was requested or successfully loaded. The callsite notes below identify the relevant request path in this source snapshot.

## Per-IP firmware route

| IP / firmware | Source reference and request path | Notes / unresolved work |
|---|---|---|
| PSP 13.0.7: `psp_13_0_7_sos.bin`, `psp_13_0_7_ta.bin` | `amdgpu_discovery.c:2295-2312` adds `psp_v13_0_ip_block` for MP0 13.0.7. `amdgpu_psp.c:257-263` selects PSP v13.0 functions. `psp_v13_0.c:120-134` routes MP0 13.0.7 to SoS + TA initialization. `amdgpu_psp.c:4117-4136` requests SoS and `4406+` requests TA. | Required firmware file names are source-supported. PSP boot itself depends on bootloader state, TMR/PSP ring, host memory setup, and PSP command protocol; filenames do not provide a safe macOS load sequence. |
| SMU 13.0.7: `smu_13_0_7.bin` | `amdgpu_discovery.c:2369-2383` selects `smu_v13_0_ip_block` for MP1 13.0.7. `pm/swsmu/smu13/smu_v13_0.c:93-127` decodes the MP1 prefix, requests `amdgpu/%s.bin`, parses the firmware header, and registers the firmware in the PSP payload table when PSP firmware loading is selected. | The SMU firmware is not standalone proof of working power management. The SMU message mailbox, clocks, dependencies and timeout behavior require target-specific implementation and testing. |
| GC 11.0.2: PFP, ME, MEC, RLC | `amdgpu_discovery.c:2520-2534` selects `gfx_v11_0_ip_block` for GC 11.0.2. `amdgpu/gfx_v11_0.c:737-816` decodes the GC prefix and requests PFP, ME, RLC (non-SR-IOV path), and MEC. Firmware declarations are at `gfx_v11_0.c:100-103`. | This source path includes several firmware header formats, CP mode selection (including RS64), RLC initialization and different load modes. Do not transplant register sequences or assume every firmware header variant is interchangeable. |
| GC 11.0.2: IMU | `amdgpu/imu_v11_0.c:49-72` decodes GC version and requests `%s_imu.bin`; declaration at line 37. `gfx_v11_0.c:827-832` calls IMU microcode initialization after the primary GFX firmware requests. | The Linux source contains GC 11.0.2-specific IMU RLC golden data and register writes (`imu_v11_0.c:361-387`). Those writes are not ported. They need exact register-base, power, reset and rollback review before any native driver can use them. |
| GC 11.0.2: MES scheduler | `amdgpu_discovery.c:2732-2750` selects `mes_v11_0_ip_block` and enables MES for GC 11.0.2. `amdgpu_mes.c:700-734` chooses `%s_mes_2.bin` for the scheduler pipe, `%s_mes1.bin` for the other pipe, and tries `%s_mes.bin` as a scheduler fallback. Declarations are in `mes_v11_0.c:42-44`. | MES is a scheduler subsystem, not the same thing as the classic MEC firmware. Do not load all three blobs blindly; the Linux code selects by pipe and has a specific fallback. MES queue/ring protocol remains unimplemented here. |
| SDMA 6.0.2: `sdma_6_0_2.bin` | `sdma_v6_0.c:1312` invokes `amdgpu_sdma_init_microcode`. `amdgpu/amdgpu_sdma.c:203-235` decodes SDMA0 version, requests the firmware, parses the common header and creates instance context. Declaration is `sdma_v6_0.c:51`. | The card reports SDMA0 and SDMA1. The Linux code can duplicate the firmware for instances; this does not imply two different firmware images. Actual engine start, ring, fence and IRQ paths are hardware operations and remain disabled in Cobalt64. |
| VCN 4.0.4: `vcn_4_0_4.bin` | `amdgpu/amdgpu_vcn.c:102-130` decodes UVD HWIP and requests `amdgpu/%s.bin`; `amdgpu_vcn.c:61` defines the v4.0.4 name and line 90 declares it. | VCN firmware version is sourced from the captured UVD HWID 12 record. The request path is common; VCN engine initialization in `vcn_v4_0_3.c` / `vcn_v4_0_5.c` must not be presumed identical to the 4.0.4 target. |
| DCN 3.2.1 DMCUB: `dcn_3_2_1_dmcub.bin` | `display/amdgpu_dm/amdgpu_dm.c:6200-6269` selects the DMCUB filename for DCE/DCN IP 3.2.1 and requests it. The constant and module declaration are at lines 138-139. | DMCUB firmware acquisition does not bring up scanout by itself. DisplayCore resource construction, DML, clocks, link encoders, PHY, HPD and commit sequencing remain a separate large port. |

## How to interpret the 14-file set

- The eight GC files cover IMU, PFP, ME, MEC, RLC and the two MES execution pipes plus the scheduler fallback. They are not one flat “GFX firmware bundle” that can safely be loaded in arbitrary order.
- PSP SoS and TA are requested under the MP0 13.0.7 PSP function path. PSP is a prerequisite boundary for the kernel's secure firmware load route.
- SMU 13.0.7 is requested using MP1 hardware version decoding. Do not derive the filename solely from the GPU marketing name.
- SDMA, VCN and DMCUB use separate IP-specific request paths and lifecycle code. Their presence on disk does not establish that their engines are safe to start.
- RAP and secure-display TA names are recorded as optional in the manifest; they are not included in the 14 required blobs.

## Implementation decisions for CherryRed

1. Keep this firmware list as provenance evidence. Do not copy Linux kernel implementation code wholesale into the macOS driver; the retained Linux source is GPL-licensed and platform/ABI-specific. Independently implement from reviewed hardware documentation, Apple interfaces and permissible source references.
2. Add a future host-only firmware-header validator per firmware family before any load code exists. A file's hash, size and name are not a substitute for parsing and validating its internal header and payload bounds.
3. PSP/SMU are the first critical blocked lifecycle: the PSP function dispatch is confirmed in Linux source, but the correct macOS PSP ring/TMR, boot handshake, memory placement, timeout and failure cleanup are not implemented.
4. Do not enable GC/IMU/MES/SDMA/VCN/DCN hardware writes until BAR mapping, IP Discovery to register-map resolution, reset/clock/power prerequisites, firmware loader semantics and rollback are independently verified for Navi 33.
5. No command submission, GPUVM/GART enable, display scanout, acceleration, Vulkan or Metal is enabled by this work.

## Validation performed

- The manifest test checks the exact required 14-file set and the evidence IP versions.
- The firmware blob test checks the local files against the hash-pinned manifest/provenance.
- The new source-trace test checks every manifest source reference exists in the retained Arch source snapshot and that each named blob is referenced in the specified source file.
- These are offline host-side consistency checks only. They do not test firmware execution on the RX 7600 or macOS.


## Firmware common-header bounds test (2026-10-30)

Added `tests/test_navi33_firmware_header_bounds.py` and CTest target `navi33_firmware_header_bounds`. It reads the 14 local RX 7600 blobs without modifying them and validates the generic AMD `common_firmware_header` fields against the family/version matrix corroborated in the retained Linux `amdgpu_ucode.h` definitions:

| Family / file group | Header version | Header bytes | IP major.minor |
|---|---:|---:|---:|
| GC IMU | 1.0 | 48 | 11.0 |
| GC PFP / ME / MEC | 2.0 | 60 | 11.0 |
| GC RLC | 2.3 | 204 | 11.0 |
| GC MES / MES1 / MES_2 | 1.0 | 72 | 11.0 |
| SDMA | 2.0 | 64 | 6.0 |
| VCN | 1.0 | 32 | 4.0 |
| DCN DMCUB | 1.0 | 40 | 3.0 |
| PSP SoS / TA | 2.0 | 36 | 13.0 |
| SMU | 2.1 | 44 | 13.0 |

The test checks the full-file size, expected family header version/length, IP major/minor, aligned payload offset, and payload range bounds. It has negative tests for a truncated common header, mismatched total length, oversized header, payload range overrun and wrong IP family.

Important limits: it does **not** recompute the firmware CRC field, validate every family-specific sub-descriptor or signature, establish authenticity, test firmware loading, or certify macOS compatibility. The RLC and SMU files contain additional sections beyond the generic payload range; the validator therefore checks only the generic header's declared range and does not claim full-format validation.


## Family-specific firmware descriptor validation (2026-10-30)

The firmware header validator has been extended beyond the common header. It now checks the subranges defined by the exact family structures present in this 14-file RX 7600 set:

| Family | Structure fields checked |
|---|---|
| GFX PFP / ME / MEC v2.0 | code offset/size and data offset/size; both ranges bounded and contiguous for these captured blobs |
| IMU v1.0 | IRAM and DRAM sizes; relative offsets; segments bounded within the file |
| MES v1.0 | code and data offsets/sizes; both ranges bounded and contiguous |
| SDMA v2.0 | context/control code ranges and jump-table metadata bounds |
| RLC v2.3 | every present register-list, save/restore, IRAM, DRAM, RLP and RLCV size/offset pair |
| PSP packed v2.0 | bin count, descriptor-array extent, unique firmware types, per-descriptor payload bounds relative to the common ucode array |
| SMU v2.1 | PPTABLE count, entry-array extent, unique non-zero payload extents in file bounds |
| DMCUB v1.0 | instruction/constant region length agrees with the common ucode payload; BSS/data size is bounded |
| VCN v1.0 | common header and payload bounds (the captured v4.0.4 format has no additional family descriptor in this source structure) |

The tests now contain 17 cases, including corruption of PSP descriptor offsets, SMU PPTABLE offsets/count, IMU DRAM offset, GFX data offset, MES data offset, RLC section size, SDMA control size and DMCUB length.

These checks are based on AMD family structure definitions and the retained Linux loader's interpretation of the relevant fields. They still do not validate signatures, cryptographic authenticity, the firmware CRC algorithm, every semantic firmware field or device-side acceptance. In particular, no blob is uploaded or executed.
