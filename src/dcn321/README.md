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
- **The real register header is NOT generated yet.** The DCN 3.2.1 `asic_reg` headers are not
  materialized in the `blob:none` evidence clone, so there is no `dcn321_regs.h` in this tree.
  Per the project's evidence-gated philosophy, **no DCN 3.2.1 register offset is fabricated** —
  the header is generated on Mac-day (or against a full checkout) from the real headers:

  ```
  python3 tools/dcn321/gen_regs.py \
    --offset <linux>/drivers/gpu/drm/amd/include/asic_reg/dcn/dcn_3_2_1_offset.h \
    --shmask <linux>/drivers/gpu/drm/amd/include/asic_reg/dcn/dcn_3_2_1_sh_mask.h \
    --spec tools/dcn321/spec.json --out src/dcn321/dcn321_regs.h
  python3 tools/dcn321/gen_regs.py ... --check   # CI: fail if the header is stale
  ```

- `tools/dcn321/spec.json` is the register set to extract (a modest, DCN-3.x-stable set: OTG
  control/lock/position, HUBPREQ flip + surface address, DMCUB control + in/outbox). It is data,
  not code, and is to be confirmed/extended against the real headers before display bring-up.

## What is NOT here

No register offsets, no write allowlist, no display bring-up logic, and no claim the display
works on macOS. The display bring-up (clock → link/encoder → hub pipeline → output timing → DML
bandwidth → DMCUB firmware `dcn_3_2_1_dmcub.bin` → scanout) is future, evidence-gated work.

## Host test (no macOS, no real headers)

```
python -B src/dcn321/tests/test_gen_regs.py
```
