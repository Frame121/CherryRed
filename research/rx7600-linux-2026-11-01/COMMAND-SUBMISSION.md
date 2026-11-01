# GPU command-submission research status

**Capture date:** 2026-11-01
**Device:** AMD RX 7600 / Navi 33 on Linux `amdgpu`

## Available evidence

The current read-only snapshot contains GEM, GTT, VRAM, VM and fence information, plus ring inventory for GFX `gfx_0.0.0`, eight compute rings, SDMA0/1, VCN unified, JPEG decode, MES KIQ and MES.

Relevant kernel tracepoint schemas include `amdgpu_cs_ioctl`, `amdgpu_cs`, `amdgpu_sched_run_job`, `drm_sched_job_run`, `drm_sched_job_done`, `amdgpu_vm_update_ptes`, `amdgpu_bo_move` and `amdgpu_ib_pipe_sync`. A capture script and research note are present in the local evidence archive.

## Not yet established

**A live GPU command-submission trace has not yet been captured.** Current GEM/VM/fence and ring snapshots are point-in-time state, not a trace of application command buffers being submitted, scheduled, executed and retired.

This evidence therefore does not establish exact IB packet sequences, per-job scheduling/completion timing, a complete user-space-to-kernel submission path for a workload, or a macOS-compatible command submission ABI.

## Next safe research step

Capture a short, controlled workload trace using the identified tracepoint schemas, with process identifiers and sensitive metadata sanitized before sharing. Keep capture read-only: no GPU reset, MMIO writes, firmware writes, module reloads or fault injection.
