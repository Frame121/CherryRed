# RX 7600 board firmware evidence intake — 2026-10-30

## What was added

The Cobalt64 evidence package on the mounted user volume
(`Cobalt64/RX7600-Linux-Evidence/`) holds board-attributed records that had not
yet been brought into this repository. The following byte-preserving copies and
one derived record were added (originals untouched):

| File | Origin | SHA-256 (source) |
|---|---|---|
| `evidence/rx7600-navi33-firmware-manifest-cobalt64.json` | `RX7600-Linux-Evidence/navi33-firmware-manifest.json` | `5e1f7879f6f39e6e432e51bf97904f926e8622ad7e99edbce183bc97d85e69bd` |
| `evidence/rx7600-vbios-identity-2026-10-23.json` | derived record; hash-bound to `rx7600-initial.txt` | `db85fb970eb4bf69e31943e910c50b06625f0f087439c7f1c6d7f37228c9818a` (source journal) |

The original volume copy of `amdgpu_discovery.bin` was re-hashed this session
and remains byte-identical to
`evidence/rx7600-amdgpu-discovery-2026-10-23.bin`
(`4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`).

## VBIOS identity (identity strings, not a ROM dump)

From the board's own Linux boot journal (quoted verbatim by the amdgpu driver):

```
amdgpu 0000:03:00.0: [drm] ATOM BIOS: 113-EXT86626-001, build: 00062039,
ver: 022.003.001.027.000001, 2023/04/25
```

Corroborated runtime facts on the same journal line block: 8176M GDDR6
128-bit VRAM, SMU initialized successfully, Display Core v3.2.384 on DCN 3.2.1,
DMUB firmware version `0x07003300` loaded via PSP, VCN firmware ENC 1.24 / DEC 9
/ revision 34, GPU topology SE 2 / SH 2 / CU 8 / 32 active CUs.

This closes part of the previously open evidence gate ("board-attributed IP
Discovery + VBIOS record"): the board now has both a hashed IP Discovery
artifact and a VBIOS identity record. **Still missing:** the VBIOS byte payload
itself. The identity strings are not a ROM image and must not be treated as one.

## Board firmware manifest cross-check

New tool: `tools/ipdisc/verify_board_firmware_manifest.py`
(plus `test_board_firmware_manifest.py`, 17 synthetic tests).

The verifier checks internal consistency between the three evidence files —
it does not authenticate anything:

- every firmware filename prefix must match the IP version recorded for its
  family in the parsed IP Discovery artifact (GC 11.0.2 → `gc_11_0_2_*`,
  PSP 13.0.7 → `psp_13_0_7_*`, SMU 13.0.7 → `smu_13_0_7.bin`,
  SDMA 6.0.2 → `sdma_6_0_2.bin`, VCN 4.0.4 → `vcn_4_0_4.bin`,
  DCN 3.2.1 → `dcn_3_2_1_dmcub.bin`);
- required firmware roles must be covered (GC: pfp/me/mec/rlc/imu/mes/mes1/mes2,
  PSP: sos/ta, SMU, SDMA, VCN, DCN/dmub);
- HWIDs 282 and 286 must stay explicitly catalogued as unresolved with UNKNOWN
  labels — no guessed names;
- the VBIOS identity record must carry the exact board PCI identity
  `1002:7480 / 148c:2421`, `authentication: NONE`, and a well-formed SHA-256
  binding;
- with `--source-log`, the recorded journal SHA-256 must match the actual bytes.

Real-evidence run on 2026-10-30: PASS — 14 firmware entries consistent with the
parsed artifact; VBIOS `113-EXT86626-001`; source journal hash-bound.

## Provenance and limits

- The manifest and journal are user-maintained records from the board's own
  Archcraft/amdgpu session. They are **not cryptographically authenticated**;
  provenance remains UNVERIFIED pending independent review.
- The manifest's firmware list is Linux-side evidence of what the upstream
  driver declares and what was present on the Linux system. It is **not** proof
  that any blob was requested/loaded, is macOS-compatible, or is legally
  redistributable. No firmware blob has been copied into this repository.
- No VBIOS byte dump exists yet. Acquiring one requires either an authorized
  Linux session (`rom` sysfs/debugfs) or a macOS ROM read, both of which need
  separate authorization; neither was attempted.
- No hardware access, kext load, firmware load, MMIO/VRAM access, EFI/SIP/AMFI
  change, or reboot was performed by this intake.
