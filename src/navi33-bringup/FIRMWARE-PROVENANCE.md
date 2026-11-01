# Navi 33 (RX 7600) firmware provenance

The 14 firmware blobs the bring-up kext embeds live in `src/navi33-bringup/firmware/`
(gitignored — never committed). This file records **where they came from and their exact
hashes**, so the set is reproducible and auditable without the blobs themselves.

## Source

- Repository: `kernel-firmware/linux-firmware` (mirror of `firmware/linux-firmware`),
  `amdgpu/` directory.
- Commit fetched: **`d0039fd797d7776d89f00c22c45754914a96706e`** (HEAD at fetch time, 2026-10-29).
- Fetched via `tools/fetch-firmware-navi33.sh <checkout>` (sparse, `amdgpu/` only).
- Licence: AMD's redistribution licence (`LICENSE.amdgpu`, copied alongside the blobs).

> These are AMD's firmware images for the RX 7600 (Navi 33): GC 11.0.2 (gfx1102), SDMA 6.0.2,
> VCN 4.0.4, DCN 3.2.1 (dmcub), PSP 13.0.7, SMU 13.0.7 — the versions confirmed by the card's
> own captured evidence (`RX7600-Linux-Evidence/navi33-firmware-manifest.json`).

## sha256 pins (as fetched)

| Blob | Bytes | sha256 |
|---|---|---|
| `gc_11_0_2_imu.bin` | 132352 | `9befca62b0b0cfd252c3df4a9edca295526f4d43821cd99a6326454995a6ca2d` |
| `gc_11_0_2_me.bin` | 198624 | `86b48ea19b01a7b5529da3eb1d65f5848b39159f1313c05cddd18feb73dd9651` |
| `gc_11_0_2_mec.bin` | 272720 | `eabe3bcfd2dc3520cec7f2205b5666c2e2ef4b5aeda0ac6c02b456b3794d10d7` |
| `gc_11_0_2_mes.bin` | 286336 | `e41e0a0a950f55f1bf8fa260972e67e9e06ba1fdf06125dcea46b92dd1d57276` |
| `gc_11_0_2_mes1.bin` | 172256 | `ac0db48108090dd64b0f1dd6f9c6fe4e6912ea17432914ec1fc5aaba8be5e4dc` |
| `gc_11_0_2_mes_2.bin` | 202112 | `ba078ce093244ac5e086d5968e912cb70676212f3557b164691f62660047d4bb` |
| `gc_11_0_2_pfp.bin` | 107968 | `7fb5ae619395caafda0697310cce04a07e8583035dd517c660d43e45cdbe0dc2` |
| `gc_11_0_2_rlc.bin` | 178848 | `b43eb2fd0600f50a1a5796bc9983d6b39b5c20960234920f5e89cb362193e0b8` |
| `sdma_6_0_2.bin` | 34560 | `16c374344d2894da751f7028f9ec1f7520035fa9548d8c750d99a00a4afa86c7` |
| `vcn_4_0_4.bin` | 403856 | `5633a5212d41e1f3932c1e7e630b1f2c8c4d27bc53ce8149c3be75d1d832053c` |
| `dcn_3_2_1_dmcub.bin` | 232608 | `3648f36b7d0ffbb0fa29ddb61b5637d8fc837669093b5bb9098bdc69fed5e010` |
| `psp_13_0_7_sos.bin` | 248496 | `94db505fa6482f258c33a0a8d412050f6d843ab4ada368252e988f82f8a26fa8` |
| `psp_13_0_7_ta.bin` | 246784 | `0b7fc5a0bea6e3559df9791a57ae5a648ec6246679f8f8a6bc27defb2aa7a75e` |
| `smu_13_0_7.bin` | 287080 | `6046f6379f42b4d3618151eace413d8c076ea374944afb770a8871ad79fcec55` |

## Re-fetch

```
git clone --filter=blob:none --no-checkout https://gitlab.com/kernel-firmware/linux-firmware.git /tmp/linux-firmware
cd /tmp/linux-firmware && git sparse-checkout set amdgpu && git checkout <commit>
tools/fetch-firmware-navi33.sh /tmp/linux-firmware
```

Then verify each copied blob's sha256 against the table above. A mismatch means the source
moved (or the blob was altered) — re-fetch at the pinned commit.

## What this is and is not

- **Is:** the real, hash-pinned RX 7600 firmware, ready to embed when the kext is built on a
  macOS host. This removes "no firmware" from the Mac-day blocker list.
- **Is not:** a claim the firmware loads or works. Loading/validating it requires a Mac + the
  physical card (PSP/SMU/GFX init, see the runbook). The blobs are data; nothing here has been
  run against hardware.
