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

**First hardware result (2026-10-29): the SMU does not answer.** The compat layer loads and runs in the kernel (all new kernel symbols resolve), register access through `struct rn_device` works, and the SMU looks alive: `C2PMSG_90` reads 1, `MP1_FIRMWARE_FLAGS` (SMN `0x3b10024`, read through the NBIO RSMU index/data pair) reads `0x1`, i.e. `INTERRUPTS_ENABLED`, which is what Linux's `smu_v13_0_check_fw_status` checks. But none of the three messages gets a response within 100 ms: `C2PMSG_66` and `_82` keep the values written and `C2PMSG_90` stays 0. The result is the same before and after our PSP, DMCUB and display stages, so those did not break it. Linux does not load SMU firmware through the PSP on this ASIC (`autoload_supported`), so the IFWI firmware is expected to answer. What is not known yet: whether Linux does something between PSP start and the first message that we do not (the Linux reference dmesg shows the PSP TMR at 20.50 s and the SMU version query at 20.74 s), or whether the mailbox needs a step this read of `smu_v13_0.c` missed. A register capture of the MP1 mailbox around the first message on Linux would settle it.

### Linux comparison run

`tools/linux_smu_mailbox.py` reads the MP1 mailbox on Linux and, with `send`, sends the same three messages with the same protocol. Two runs on the card:

1. Normal boot (the driver has initialised the SMU): `sudo python3 linux_smu_mailbox.py send smu_mailbox_normal.txt`.
2. Boot with the kernel parameter `modprobe.blacklist=amdgpu` so no driver touches the card, then read BAR5 directly, like RedNavi does: `sudo python3 linux_smu_mailbox.py send smu_mailbox_nodriver.txt bar5`. (`amdgpu.dpm=0` gives a black screen on this card and is not usable.)

If the SMU answers in run 2, the difference is in what RedNavi does around the mailbox; if it does not but answers in run 1, it needs something the Linux driver does during its power-management init. Results go to `docs/logs/linux-reference/`.
