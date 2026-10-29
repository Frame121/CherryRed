# Compat layer (LinuxKPI-style)

Goal: let code ported from amdgpu (SMU 13, later DC, GFX, MES) build in the kext with as few edits as possible, instead of re-writing every register access by hand like the probe does. It is written from scratch under BSD-3; FreeBSD's LinuxKPI and drm-kmod were read only for ideas.

State (2026-10-28): the layer builds into `RedNavi.kext` and its pure parts have host tests (`make test`). **It has not been loaded on a real machine yet.** The first consumer is the SMU message stage (`RedNavi/smu13.cpp`, boot-arg `rednavi_smu=1`), which is the first thing to run on hardware.

## Layout

| Path | What |
|---|---|
| `compat/include/linux/*.h` | Linux headers: `types`, `errno`, `bits`, `kernel` (container_of, min/max, ERR_PTR, READ_ONCE), `printk`, `device`, `list`, `atomic` (+ kref), `spinlock`, `mutex`, `completion`, `delay`, `jiffies`, `ktime`, `slab`, `io`, `byteorder`, `firmware`, `workqueue`, `dma-mapping` |
| `compat/include/amd/rn_device.h` | Minimal stand-in for `struct amdgpu_device`: `RREG32`, `WREG32`, `RREG32_SOC15`, `SOC15_REG_OFFSET`, `REG_SET_FIELD` |
| `compat/include/rn/hooks.h` | The few functions the pure headers need from their environment (log, alloc, sleep, clock, yield) |
| `compat/src/rn_hooks.cpp` | Kernel implementation of the hooks (`IOLogv`, `IOMalloc`, `IOSleep`, `clock_get_uptime`, `panic`) |
| `compat/src/rn_slab.cpp` | `kmalloc` family with a size header, so `kfree` and `krealloc` need no size |
| `compat/src/rn_firmware.cpp` | `request_firmware` from a registry of blobs (`rn_firmware_register("amdgpu/smu_13_0_10.bin", ...)`); nothing is read from disk |
| `compat/src/rn_workqueue.cpp` | `work_struct`, `delayed_work`, `schedule_work` ... on `thread_call` |
| `compat/src/rn_dma.cpp` | `dma_alloc_coherent` on a physically contiguous `IOBufferMemoryDescriptor` |
| `compat/src/rn_reg.cpp` | Register access: direct inside the BAR, `MM_INDEX`/`MM_DATA` past its end |
| `tests/host/` | Host tests and the host version of the hooks |

## Known limits

- Spinlocks do not mask interrupts (nothing runs in interrupt context yet). Mutexes are not recursive and have no owner; a contended one yields with `IOSleep(1)`.
- One jiffy is one millisecond (`HZ` 1000).
- Work items own a `thread_call`; call `rn_work_destroy` before freeing one that was queued. All workqueues are the same shared pool.
- `dma_alloc_coherent` assumes the address from `getPhysicalAddress()` is what the GPU must use, i.e. no IOMMU. Verify on a machine with VT-d on.
- GFP flags are accepted but only `__GFP_ZERO` does anything.
- Not done yet: wait queues, `dma_fence`, IDR/xarray, `ww_mutex`, PCI helpers beyond what the probe does, interrupts, DRM structures.

## Adding a header

Put it under `compat/include/linux/` only if Linux code includes it by that name. Keep it free of IOKit so it runs in the host tests, and route anything kernel-specific through `rn/hooks.h`. Add a check to `tests/host/compat_test.cpp`.

## SMU stage

`rn_smu13_send` follows `smu_msg_v1_send_msg` from Linux: wait for the previous response, clear `C2PMSG_90`, write the argument to `C2PMSG_82`, the message to `C2PMSG_66`, poll `C2PMSG_90` (1 = OK). `rednavi_smu=1` sends `GetSmuVersion`, `GetDriverIfVersion` and `TestMessage` after the PSP and display bring-up and publishes `smu-info` (and `smu-info-early`, registers only, read before any write) on the probe node. Message numbers come from `pmfw_if/smu_v13_0_0_ppsmc.h`; SMU 13.0.10 uses those tables. Messages that change clocks or features are not sent.

**Result (2026-10-29): the SMU answers once its firmware has been loaded through the PSP.** Without that step none of the three messages gets a response, with the registers looking healthy (`C2PMSG_90` = 1, `MP1_FIRMWARE_FLAGS` bit 0 set). In `psp_load_non_psp_fw`, Linux first calls `psp_load_smu_fw` whenever the PSP supports autoload (MP0 13.0.10 does), which sends `GFX_CMD_ID_LOAD_IP_FW` with type `GFX_FW_TYPE_SMU` (18) and the ucode array of `smu_13_0_10.bin` (the 261632 bytes at `ucode_array_offset_bytes`); an earlier reading of `psp_hw_start` made it look skipped, which was wrong. The probe does this with boot-arg `rednavi_smu_load=1` (`loadSmuFw`, right after `SETUP_TMR`, header from `tools/gen_smu_fw_header.py`, git-ignored). The sOS accepts it (status 0) and the SMU then answers: `GetSmuVersion` returns 0x00505c00 (the version of the file we loaded; the IFWI one was 0x00505600), `GetDriverIfVersion` 0x40 and `TestMessage` 0x12345679, the same values Linux gets. The "Linux comparison run" section below documents how the difference was found; the GC/IMU/RLC hypothesis there turned out not to be needed for this.

### Linux comparison run

`tools/linux_smu_mailbox.py` reads the MP1 mailbox on Linux and, with `send`, sends the same three messages with the same protocol. Two runs on the card:

1. Normal boot (the driver has initialised the SMU): `sudo python3 linux_smu_mailbox.py send smu_mailbox_normal.txt`.
2. Boot with the kernel parameter `modprobe.blacklist=amdgpu` so no driver touches the card, then read BAR5 directly, like RedNavi does: `sudo python3 linux_smu_mailbox.py send smu_mailbox_nodriver.txt bar5`. (`amdgpu.dpm=0` gives a black screen on this card and is not usable.)

If the SMU answers in run 2, the difference is in what RedNavi does around the mailbox; if it does not but answers in run 1, it needs something the Linux driver does during its power-management init. Results go to `docs/logs/linux-reference/`.

**Result of the Linux runs (2026-10-29)** (`docs/logs/linux-reference/smu_mailbox_normal.txt`, `smu_mailbox_nodriver.txt`):

| | Normal boot (driver ran) | `modprobe.blacklist=amdgpu`, BAR5 only |
|---|---|---|
| `MP0_C2PMSG_81` (sOS alive) | `0x030b09a0` | 0 |
| `MP0_C2PMSG_35` | 0 | `0x80000000` (bootloader waiting) |
| `MP1_FIRMWARE_FLAGS` | 1 | 1 |
| `MP1_C2PMSG_90` before | 1 | 1 |
| `GetSmuVersion` | answered in 0.3 ms: `0x00505600` | no response in 1 s |
| `GetDriverIfVersion`, `TestMessage` | `0x40`, `0x12345679` (OK) | not sent after the first timeout |

So the protocol and the offsets in `rn_smu13_send` are right: the same sequence of writes gets an answer on Linux once the driver has run. Without the driver (sOS not running) the SMU does not answer, which is also what RedNavi saw. RedNavi's late attempt has the sOS running, the ring, LOAD_TOC and SETUP_TMR done, and still gets no answer, so something else in Linux's init order unlocks the mailbox. In the driver-run state `MP1_C2PMSG_71` = 5 and `_74` = 0x14 are set (zero in the no-driver state). Candidates, untested: the non-PSP firmware loads Linux does before the SMU init (IMU, RLC, SDMA, MES, DMUB, TOC), or a PSP step after SETUP_TMR.

**RedNavi's state at the first message (2026-10-29)**, same registers from the probe (`smu-info`): `MP0_C2PMSG_81` = 0x0231bf63 (sOS alive) and `MP0_C2PMSG_35` = 0, like the Linux driver-run column; but `MP1_C2PMSG_71` = 0 and `_74` = 0 (Linux with driver: 5 and 0x14), `MP1_C2PMSG_66` = 0, `_82` = 0, `_90` = 1, `MP1_FIRMWARE_FLAGS` = 1. So the PSP side matches what Linux has after its PSP init, and the SMU side looks like the no-driver state.

Working hypothesis, not tested: in Linux the SMU is first queried (`smu_hw_init`, after `amdgpu_device_fw_loading`) when the PSP has already loaded the GC, SDMA, MES, IMU and RLC firmware into the TMR and started the RLC autoload (`GFX_CMD_ID_AUTOLOAD_RLC`). The SMU firmware may wait for the IMU/RLC handshake before it opens the mailbox. Checking this means loading `gc_11_0_3_{pfp,me,mec,rlc,imu}.bin` and friends through `LOAD_IP_FW`, which is also the first step of M4 (GFX11 bring-up). The display does not depend on it; the SMU would only be needed for clock management.

## M4 step 1: the graphics core boots (2026-10-29)

With `rednavi_smu_load=1 rednavi_gc_load=1 rednavi_smu=1 rednavi_smu_enable=1` the GC block comes up, in this order (the order Linux uses, `psp_load_non_psp_fw` then `smu_smc_hw_setup`):

1. `LOAD_IP_FW` of `smu_13_0_10.bin` (type 18).
2. `LOAD_IP_FW` of 26 images in `AMDGPU_UCODE_ID` order, with the types and image offsets Linux uses (`tools/gen_gc_fw_header.py`, local header `RedNavi/gc_fw.h`): SDMA threads 0 and 1, CP RS64 PFP/ME/MEC and their stacks, MES scheduler and KIQ with their data, IMU I/D, the RLC restore lists, IRAM, DRAM, P, V and G. The sOS accepts all of them (status 0).
3. `GFX_CMD_ID_AUTOLOAD_RLC` (0x21) after `RLC_G`. Accepted, and `GFX_IMU_SCRATCH_0` shows the IMU firmware version 0xb204700, so the IMU runs, but the RLC does not finish booting: `RLC_RLCS_BOOTLOAD_STATUS` stays 0 for 2 s.
4. SMU messages `GetSmuVersion` etc., then `EnableAllSmuFeatures` (0x6). `SetAllowedFeaturesMaskLow/High` are rejected with CmdRejectedPrereq (SCPM, the PSP owns the allowed mask). Right after the enable the RLC boots: `BOOTLOAD_STATUS` 0xc000001f (BOOTLOAD_COMPLETE, bit 31), `CP_STAT` 0, `RLC_CNTL` 1, `RLC_GPM_STAT` 0xb40016, `GRBM_STATUS` 0x302c. Running SMU features: 0x0003ebbc_71ffffff.

So the RLC autoload waits for the SMU features to be enabled. Next for acceleration: memory access for the engines (the VRAM is reachable by physical MC address through the system aperture the VBIOS left; GART for system memory comes later), then an SDMA ring and a first command (write a value to VRAM, check it from the CPU), then the graphics ring (MES/KIQ).
