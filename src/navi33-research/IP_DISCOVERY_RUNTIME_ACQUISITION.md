# RX 7600 IP Discovery runtime acquisition — macOS port requirements

Date: 2026-10-30

## What the captured binary does and does not prove

The current 10 KiB evidence artifact contains a valid 1,280-byte AMD Discovery binary v1.3 with IP Discovery table v3. This is enough to test the offline parser and the expected Navi 33 IP profile.

It does not establish where a running driver should fetch the discovery binary from after macOS starts, whether the TMR is at the default VRAM location on every board/boot, or whether a saved capture remains valid after firmware/BIOS changes.

## Linux source flow observed in the retained kernel source

The following describes the Linux source snapshot only. It is not a sequence to copy into CherryRed:

1. Non-SR-IOV path polls the MP0 C2P message register for the IFWI-ready high bit, with a bounded wait. The source comments note that IFWI readiness can take up to about two seconds on some discrete GPUs.
2. It reads the VRAM size register. A valid non-zero value selects a default TMR region at the VRAM end using `DISCOVERY_TMR_OFFSET`; a zero value selects a system-memory path.
3. On discrete cards it checks for the discovery binary signature at the default TMR location. If absent, it can take TMR size and offset from driver scratch registers 2, 0 and 1. If there is no VRAM, the source has an ACPI TMR-query fallback.
4. After selecting TMR location/size, Linux allocates a kernel buffer and reads the binary from VRAM or system memory through its AMD device-memory access layer.
5. It verifies the binary signature, binary checksum, table directory, IP Discovery signature, table size/checksum and other available tables before parsing per-die records.

Source anchors in the retained Arch Linux 7.2.11 source:
- `amdgpu/amdgpu_discovery.c:265-352` — IFWI wait, VRAM/TMR selection, scratch-register and ACPI fallback.
- `amdgpu/amdgpu_discovery.c:373-394` — memory transport and VRAM/system-memory copy.
- `amdgpu/amdgpu_discovery.c:438-461` — byte-sum checksum calculation.
- `amdgpu/amdgpu_discovery.c:583-727` — table verification and binary checksum flow.

## macOS implementation gates

Before any live fetch is written, CherryRed needs independently verified answers for all of these:

- Exact Navi 33 register definitions and access mechanism for IFWI-ready status, VRAM size, TMR scratch size/address and any fallback path. Register bases must be resolved from a trusted, version-matched source, not guessed from the Discovery base-address array.
- IOKit device-memory mapping and cache/ordering requirements for the relevant PCI BAR and any VRAM window. The Linux `amdgpu_device_vram_access` call is not an IOKit API and must not be mechanically translated.
- TMR ownership/reservation rules and firmware/PSP readiness semantics. Reading a scratch register before the device is ready is not evidence that the returned pointer is usable.
- A bounded allocation/copy path with checked address arithmetic, maximum size, timeout and unwind behaviour.
- Agreement between runtime binary identity, PCI ID `1002:7480`, board/subsystem identity, IP version profile and the current firmware/BIOS environment.
- A kernel-context test plan that can safely abort on any mismatch without resetting or touching engine state.

## Current CherryRed decision

No live register reads, BAR mapping, VRAM window mapping, TMR access or MMIO writes have been added. The current driver remains at PCI identity/config-space survey and publishes `RegisterBARMapped=false` and `HardwareWrites=false`. The offline parser is kept in the host research library only.

A later read-only runtime probe should be implemented as a separate reviewed step after the macOS SDK and target-specific register maps are available. Do not use the captured base addresses as direct host addresses or offsets.
