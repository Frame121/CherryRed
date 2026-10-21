# RX 7600 / Navi 33 board evidence record

1002:7480: UNVERIFIED — hardware presence unknown (non-Linux host; lspci/dmesg/amdgpu tools unavailable)
**Record date:** 2026-10-21
**Host environment:** Windows PowerShell in the Cobalt64 repository checkout
**Target PCI ID under investigation:** `1002:7480`

## Result and environment boundary

The target was **not checked**. This Windows host does not provide an
authorized Linux hardware-capture environment. The commands and Linux sysfs
paths required for direct observation were not run or read. Therefore this
record does **not** establish that the RX 7600 is absent; hardware presence on
this host is **unknown**.

The following PowerShell availability check was run:

```powershell
Get-Command lspci,dmesg,amdgpu-discover,atomtree -ErrorAction SilentlyContinue
```

It returned no command entries. No substitutes, WSL, virtual machines, or
improvised Linux interfaces were used.

## Board-attributed evidence table

Every board-specific row is **UNVERIFIED**. "Expected from source" below is a
conditional Linux source claim only; it is not a request observed in logs or
firmware reported loaded/running on this board.

| Evidence item | Expected from pinned source (conditional only) | Board observation / category | Status |
|---|---|---|---|
| Device presence and identity | No target-specific assignment is established by the generic dispatch specification. | `lspci -nn | grep -i 1002:7480` was **not run**; no Linux `lspci` environment available. No PCI IDs or subsystem IDs observed. | **UNVERIFIED**; presence/absence unknown. |
| Boot-time amdgpu initialization | Logs could show device binding and initialization, but generic source branches alone cannot predict this board's full boot log. | `dmesg | grep -i amdgpu` and full boot slice were **not run**. | **UNVERIFIED**; no log-observed request evidence. |
| IP Discovery records | The pinned Linux parser consumes records and the dispatch switches use reported IP versions; no version is assigned to `1002:7480` by the sources cited in the spec. | No board IP Discovery dump or `amdgpu-discover`/atomtree output supplied. | **UNVERIFIED**; no board-attributed IP versions, instances, variants, harvest state, or bases. |
| GFX / GC firmware | If source conditions select the relevant GFX v11 path and GC version, version-derived GFX microcode requests may be expected (spec lines 56 and 84). | No logs or firmware state available. No request observed; no loaded/running firmware observed. | **UNVERIFIED** for this board. |
| SDMA firmware | A required, version-derived request is expected only if the source-dispatched SDMA v6 path and its initialization conditions apply (spec lines 57 and 89). | No logs or firmware state available. No request observed; no loaded/running firmware observed. | **UNVERIFIED** for this board. |
| PSP / MP0 firmware | For MP0 13.0.4, Linux's selected PSP implementation calls TOC then TA initialization; version and request path are conditional (spec lines 58 and 87). | No MP0 record, PSP request log, or firmware state available. No request observed; no loaded/running firmware observed. | **UNVERIFIED** for this board. |
| SMU / MP1 firmware | If the MP1 version and non-VF branch select the path, Linux requests version-derived required firmware, with kicker selection conditional (spec lines 59 and 88). | No MP1 record, request log, or firmware state available. No request observed; no loaded/running firmware observed. | **UNVERIFIED** for this board. |
| VCN firmware | If UVD version, instance count, and per-instance-firmware conditions select the path, Linux requests corresponding firmware (spec lines 60 and 90). | No UVD/VCN records, request logs, or firmware state available. No request observed; no loaded/running firmware observed. | **UNVERIFIED** for this board. |
| Display / DMUB firmware | If display initialization and a supported DCE path apply, Linux selects the corresponding DMUB version path (spec lines 61 and 92). | No DCE/DC records, display initialization log, request log, or firmware state available. | **UNVERIFIED** for this board. |
| MES firmware | GC version dispatch and MES initialization/scheduler conditions must select the path (spec lines 62 and 91). A declared firmware name alone is not a request. | No GC record, MES log, request log, or firmware state available. | **UNVERIFIED** for this board. |
| Other blocks / firmware | The specification is explicitly partial and is not a complete board inventory. | No full IP Discovery artifact, boot logs, firmware information, or per-device sysfs data available. | **UNVERIFIED**. |

### Firmware evidence classification

| Required category | Result |
|---|---|
| (a) Firmware request explicitly observed in this board's logs | **UNVERIFIED / not observed** — logs unavailable; this does not mean no request occurred. |
| (b) Firmware reported loaded/running on this board | **UNVERIFIED / not observed** — firmware information unavailable; this does not mean firmware was not loaded. |
| (c) Expected request inferred from pinned source conditions | **Conditional only.** Candidate Linux source paths are listed in `RX7600_NAVI33_IP_DISPATCH_SPEC.md`; no board IP versions or branch conditions were observed to select them. |
| (d) Board firmware requirements | **UNVERIFIED.** No request or loaded state is attributed to `1002:7480`. |

## Exact checks and paths

Not run because this is Windows and no authorized Linux environment is
available:

```text
lspci -nn | grep -i 1002:7480
dmesg | grep -i amdgpu
```

Not read; Linux debugfs/sysfs are unavailable in this host context:

```text
/sys/kernel/debug/dri/*/amdgpu_firmware_info
/sys/bus/pci/devices/0000:*/vendor
/sys/bus/pci/devices/0000:*/device
/sys/bus/pci/devices/0000:*/subsystem_vendor
/sys/bus/pci/devices/0000:*/subsystem_device
/sys/bus/pci/devices/0000:*/revision
/sys/bus/pci/devices/0000:*/class
/sys/bus/pci/devices/0000:*/resource
```

`amdgpu-discover` and `atomtree` availability was not established beyond the
empty Windows `Get-Command` result above. No artifact was received, hashed, or
parsed. No physical hardware, PCI configuration space, MMIO, VRAM, firmware
interface, or system configuration was accessed.

## Evidence required to unblock board attribution

Provide an existing, read-only capture from an authorized Linux host with the
RX 7600 connected, together with provenance sufficient to tie the capture to
the physical board:

1. `lspci -nn` identity for vendor/device `1002:7480`, including subsystem IDs,
   revision, and the exact PCI BDF; include board make/model and VBIOS
   identity/version if already available.
2. Full relevant `dmesg` boot slice around amdgpu binding and initialization,
   not just matching lines, so request failures and context are retained.
3. Every readable `/sys/kernel/debug/dri/*/amdgpu_firmware_info` file and
   relevant `/sys/bus/pci/devices/<BDF>/` identity attributes listed above.
4. `amdgpu-discover` or atomtree output if already available; otherwise state
   `tool unavailable`. Include the original IP Discovery artifact only if it
   already exists and can be supplied with capture method, timestamp,
   tool/version, original filename, size, SHA-256, custody, and board link.
5. Linux distribution, kernel version, capture time/time zone, tool versions,
   and any errors or unreadable paths. Keep originals unchanged.

Do not infer missing IP records or firmware requirements from generic Linux
branches, PCI database names, synthetic fixtures, or Navi48 implementation.
Until the above is independently reviewed, `1002:7480` is **not board-
attributed in this project**, all board IP and firmware rows remain
**UNVERIFIED**, and Phase 4 dispatch implementation is not justified.
