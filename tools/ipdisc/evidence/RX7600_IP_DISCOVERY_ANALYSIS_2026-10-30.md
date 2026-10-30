# RX 7600 AMD IP Discovery — verified offline intake

**Result:** raw IP Discovery binary found in the preserved Linux evidence package, copied byte-for-byte into this repository, and accepted by the strict offline parser.

## Artifact identity

- Original: `RX7600-Linux-Evidence/amdgpu_discovery.bin` on the external `GameAndFile` volume
- Repository copy: `tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin`
- Size: **10,240 bytes**
- SHA-256: `4b809b3279bd9fd164456f5fcdcb9743d2b4289acdd77f9ac2a2cd77b75e6c89`
- Byte-for-byte comparison: **PASS**
- Parser report: `rx7600-amdgpu-discovery-2026-10-23.bin.parsed.json`
- Provenance sidecar: `rx7600-amdgpu-discovery-2026-10-23.bin.provenance.json`

The accompanying evidence package's `MANIFEST.txt` records capture on 2026-10-23 using Archcraft Linux kernel `7.2.11-arch1-1`, PCI BDF `0000:03:00.0`, GPU `1002:7480`, subsystem `148c:2421`, and driver `amdgpu`. The original capture command recorded there reads `/sys/kernel/debug/dri/1/amdgpu_discovery`. `rx7600-initial.txt` independently records the same PCI identity, amdgpu binding, and Linux IP block initialization messages.

**Provenance status remains UNVERIFIED:** the manifest and logs are user-maintained evidence from the same package, not cryptographically authenticated capture attestations. The artifact is classified as a physical-capture candidate, not independently authenticated board evidence.

## Strict parser result

- Strict mode: **PASS**
- Binary offset: `0`
- Die offset: `140`
- IP records: **39**
- Unique numeric HWIDs: **30**
- Input file size: 10,240 bytes
- Header-declared binary region: **1,280 bytes** (trailing bytes are not assumed to belong to the declared binary)

The previous evidence audit records:
- Binary signature: `0x28211407`
- Binary header version: `1.3`
- Binary checksum: stored/calculated `0x5784` — PASS
- IP Discovery table: `IPDS`, version `3`, offset `60`, size `808`
- IP Discovery table checksum: stored/calculated `0x49a2` — PASS
- One die; all 39 variable-length records pass strict bounds validation.

## Directly decoded IP records of interest

| Numeric HWID | Parser label | Instance(s) | Reported version |
|---:|---|---:|---|
| 11 | GC | 0 | **11.0.2** |
| 34 | MMHUB | 0 | 3.0.2 |
| 41 | HDP | 0 | 6.0.1 |
| 42 | SDMA0 | 0 | 6.0.2 |
| 150 | UMC | 0–3 | 8.11.0 |
| 255 | MP0 | 0 | 13.0.7 |
| 1 | MP1 | 0 | 13.0.7 |
| 108 | NBIF | 0 | 4.3.1 |
| 40 | OSSSYS | 0 | 6.0.2 |
| 271 | DMU | 0 | 3.2.1 |

The parser currently labels 20 numeric HWIDs as `UNKNOWN_<id>` because its built-in enum is intentionally incomplete. The preserved `ip-hwid-source-mapping.md` cross-references 28/30 IDs against Linux source tables, but that mapping is source-correlated only: the exact source revision used to build the installed Arch kernel is not fully authenticated. HWIDs **282** and **286** remain unresolved in the checked source snapshot. Do not infer names for them.

The original Linux boot log additionally reports driver IP block initialization:
`common_v1_0_0`, `gmc_v11_0_0`, `ih_v6_0_0`, `psp_v13_0_0`, `smu_v13_0_0`, `dce_v1_0_0`, `gfx_v11_0_0`, `sdma_v6_0_0`, `vcn_v4_0_0`, `jpeg_v4_0_0`, and `mes_v11_0_0`. These are Linux driver's reported IP block implementations, not all direct IP Discovery record labels; keep the two evidence layers distinct.

## Interpretation and next engineering implications

1. The captured GC record is GFX **11.0.2**. CherryRed's Navi33 path should be aligned to GFX11.0.2 evidence rather than generic Navi48 assumptions.
2. The captured table reports four UMC instances (0–3), one GC instance, one SDMA0 record, and the listed fabric/management blocks. Instance counts and versions are directly parsed; their operational readiness is not proven by parsing.
3. The capture provides register-base metadata. Those values are **not instructions to map BARs or access MMIO**. No register reads/writes were performed during this intake.
4. IP Discovery alone does not provide a VBIOS image, firmware payloads, firmware licensing, a complete initialization sequence, macOS driver matching, or a Metal device.
5. The Metal dispatch probes still compile but fail closed at runtime on this macOS boot because `MTLCopyAllDevices()` returns no devices and `MTLCreateSystemDefaultDevice()` returns nil. Therefore hardware Metal dispatch remains **BLOCKED**, not passed.
6. The exact RX 7600 acceleration goal still requires a real macOS accelerator driver and successful dispatch on the target GPU. This evidence does not create that driver.

## Reproduction

From repository root:

```sh
shasum -a 256 tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin
python3 -B tools/ipdisc/dump.py \
  tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin \
  --mode strict --json --pretty \
  --provenance tools/ipdisc/evidence/rx7600-amdgpu-discovery-2026-10-23.bin.provenance.json
python3 -B tools/ipdisc/test_ipdiscovery.py
```

The parser validates structure and checksums only; it does not authenticate the source or establish macOS compatibility.
