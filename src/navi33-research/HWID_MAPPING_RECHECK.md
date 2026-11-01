# RX 7600 IP Discovery HWID mapping re-check

Date: 2026-10-30

## Result

The capture contains numeric HWIDs **282** and **286**. Their names remain **unresolved** in CherryRed. Do not label them by guessing from their version numbers, record order, or neighboring IDs.

A re-check of the current upstream Linux AMD HWID header shows named constants for MP0 255, DMU 271, DIO 272, DAZ 274, ATU 294 and AIGC 295, but no defined constants for 282 or 286. The corresponding current upstream discovery name map likewise has no entries identifying 282 or 286.

This is a statement about the current source tables reviewed on 2026-10-30, not a claim that AMD has no private/internal name for either HWID. If a later public source or vendor document supplies a mapping, record its version/source and add a test before assigning a label.

## CherryRed policy

- Keep the captured records as raw numeric HWIDs.
- Preserve their instance, IP version, variant and sub-revision fields.
- Never use an unresolved HWID to select an IP block, firmware filename, register map, power/clock sequence or initialization routine.
- A profile may validate known required records while preserving unknown records, but it must not infer hardware functionality from unknown entries.

## Upstream references reviewed

- Current Linux `soc15_hw_ip.h`: https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/master/drivers/gpu/drm/amd/include/soc15_hw_ip.h
- Current Linux `amdgpu_discovery.c` HWID name table: https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c
