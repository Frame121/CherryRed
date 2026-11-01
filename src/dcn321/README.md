# dcn321 — DCN 3.2.1 display register layer (scaffold)

The RX 7600 (Navi 33) uses **DCN 3.2.1** (per the project's own captured evidence —
`RX7600-Linux-Evidence/dcn321-dependency-map.md`). This module is the display register layer
for that, mirroring the existing `src/dcn41/` (DCN 4.1) module.

## Current state (honest)

- **Generator is ready and host-tested.** `tools/dcn321/gen_regs.py` mirrors
  `tools/dcn41/gen_regs.py`: it parses Linux's DCN 3.2.1 `dcn_3_2_1_offset.h` +
  `dcn_3_2_1_sh_mask.h`, emits per-instance offsets and field masks/shifts straight from the
  headers, and **refuses** on a missing instance, disagreeing `BASE_IDX`, or a non-instance-0
  mask/shift that differs. `src/dcn321/tests/test_gen_regs.py` proves this against a synthetic
  fixture (host-verifiable, no real headers needed).
- **The real register header IS generated** — `dcn321_regs.h` is present in this tree. The two
  DCN 3.2.1 `asic_reg` headers were materialized on demand from the `blob:none` evidence clone
  (an on-demand fetch of just those two blobs at commit `64a90897d`: `dcn_3_2_1_offset.h`
  1.49 MB, `dcn_3_2_1_sh_mask.h` 6.07 MB), the register names in `spec.json` were confirmed
  against them, and the generator emitted the header. The generator's fail-closed checks
  passed: all four OTG/HUBPREQ instances are present and consistent, and every register uses
  `BASE_IDX 2`. The header carries a provenance line (commit + blob SHAs).
- **What is still NOT done:** the offsets are real (not fabricated), but the header is **not
  yet validated against the physical card** — there is no Mac + RX 7600 yet. On Mac-day the
  bring-up must read the DCN segment base from the live BAR5 and confirm
  `seg[BASE_IDX] + offset` is inside the window before touching any register (see the runbook).

  To regenerate (any machine with the two headers):

  ```
  python3 tools/dcn321/gen_regs.py \
    --offset <linux>/drivers/gpu/drm/amd/include/asic_reg/dcn/dcn_3_2_1_offset.h \
    --shmask <linux>/drivers/gpu/drm/amd/include/asic_reg/dcn/dcn_3_2_1_sh_mask.h \
    --spec tools/dcn321/spec.json --out src/dcn321/dcn321_regs.h \
    --provenance "archlinux/linux commit 64a90897d; offset blob 20deafe5…, sh_mask blob c2292913…"
  python3 tools/dcn321/gen_regs.py ... --check   # CI: fail if the header is stale
  ```

- `tools/dcn321/spec.json` is the register set to extract (a modest, DCN-3.x-stable set: OTG
  control/lock/position, HUBPREQ flip + surface address, DMCUB control + in/outbox). It is data,
  not code, and is **confirmed against the real DCN 3.2.1 headers** (every register and field in
  it is present in `dcn_3_2_1_offset.h` / `dcn_3_2_1_sh_mask.h`). Extend it as the display
  bring-up needs more registers.

## What is NOT here

No write allowlist, no display bring-up logic, and no claim the display works on macOS. The
generated header is read-only reference data (offsets + field masks/shifts); it does not by
itself touch hardware. The display bring-up (clock → link/encoder → hub pipeline → output
timing → DML bandwidth → DMCUB firmware `dcn_3_2_1_dmcub.bin` → scanout) is future,
evidence-gated work.

## Host test (no macOS, no real headers)

```
python -B src/dcn321/tests/test_gen_regs.py
```
