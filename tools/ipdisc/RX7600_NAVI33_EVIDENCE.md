# RX 7600 / Navi 33 board evidence record

1002:7480: **PRESENCE VERIFIED** — device present and enumerated on this
Windows host (subsystem `148C:2421`, revision `CF`); **IP Discovery and
firmware rows remain UNVERIFIED** — no board IP Discovery artifact, VBIOS
identity, or firmware state was captured.
**Record date:** 2026-10-21 (updated Phase 25)
**Host environment:** Windows PowerShell in the Cobalt64 repository checkout
**Target PCI ID under investigation:** `1002:7480`

## Result and environment boundary

**Device presence and PCI identity are now VERIFIED** on this host through a
read-only Windows device-inventory query. The board enumerated normally and the
OS reports it bound to an AMD display driver.

The exact query run (read-only; no writes, no register access):

```powershell
Get-CimInstance Win32_VideoController |
  Select-Object Name, PNPDeviceID, DriverVersion, AdapterRAM
Get-PnpDevice | Where-Object { $_.InstanceId -like '*VEN_1002*' } |
  Select-Object Status, Class, FriendlyName, InstanceId
```

Observed result for the target:

```text
FriendlyName : AMD Radeon RX 7600
Status       : OK
Class        : Display
InstanceId   : PCI\VEN_1002&DEV_7480&SUBSYS_2421148C&REV_CF\6&EB99E4&0&00000009
DriverVersion: 32.0.31041.1004
AdapterRAM   : 4293918720
```

**What this establishes:** the physical target is present, it is a
`1002:7480` device, it reports subsystem ID `148C:2421`, revision `CF`, and a
display class driver is bound to it.

**What this does NOT establish:** IP Discovery records or IP versions, firmware
requests, loaded/running firmware, VBIOS identity, VRAM size, or anything about
macOS. `AdapterRAM` is a 32-bit-limited WMI field and is recorded as a raw
observation only; it is not used as a VRAM or board-capability fact.

**Boundary maintained:** this is a read-only OS device-model query, not a
hardware capture. No PCI configuration space, ROM BAR, MMIO, VRAM, firmware
interface, or driver state was read or written. No Linux `lspci`/`dmesg`/sysfs
path was simulated or substituted. The earlier `Get-Command
lspci,dmesg,amdgpu-discover,atomtree` availability check remains recorded below
as a tool-availability probe only; it is **not** evidence about the device.

### Unresolved attribution detail

The subsystem ID is recorded raw as `148C:2421` (vendor `148C`, device
`2421`). Resolving that vendor to a board brand/part number requires a PCI ID
database. The repository-local `pci.ids` is absent (established in M2.5), and
resolving it by name would require network access, which requires separate
authorization. **No brand or part-number name is asserted here.**

## Board-attributed evidence table

Every board-specific row is **UNVERIFIED**. "Expected from source" below is a
conditional Linux source claim only; it is not a request observed in logs or
firmware reported loaded/running on this board.

| Evidence item | Expected from pinned source (conditional only) | Board observation / category | Status |
|---|---|---|---|
| Device presence and identity | No target-specific assignment is established by the generic dispatch specification. | Read-only Windows device inventory: `PCI\VEN_1002&DEV_7480&SUBSYS_2421148C&REV_CF\6&EB99E4&0&00000009`, class Display, status OK. Vendor `1002`, device `7480`, subsystem `148C:2421`, revision `CF`. Subsystem vendor brand/part number **unresolved** (no local `pci.ids`). | **VERIFIED for presence and PCI identity only.** Not an IP Discovery or firmware observation. |
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

**Run (read-only, Windows device model):**

```text
Get-CimInstance Win32_VideoController
    -> AMD Radeon RX 7600
    -> PCI\VEN_1002&DEV_7480&SUBSYS_2421148C&REV_CF\6&EB99E4&0&00000009
Get-PnpDevice (filter VEN_1002)
    -> 7480 Display OK; 1479 Downstream Switch Port OK
    -> 1478 Upstream Switch Port OK; AB30 High Definition Audio Bus OK
```

`1478`/`1479`/`AB30` are recorded as raw observed AMD PCI entries only; no
relationship to the target board is asserted beyond what the OS reported.

**Also attempted (read-only, offline search for a pre-existing VBIOS/atomROM artifact):**

```text
Get-ChildItem -Recurse -Include *vbios*,*atom*,*.rom,*atibios*  over
C:\Windows\System32\DriverStore\FileRepository, C:\AMD, C:\Program Files\AMD
-> no matches; C:\AMD and C:\Program Files\AMD do not exist in this search context
```

No VBIOS/atom ROM image was located, so no IP Discovery table source is
present locally. Acquiring one would require either a ROM BAR read (hardware
probe) or a third-party dump tool (download) — **neither was attempted**;
both need separate explicit authorization.

**Not run because no authorized Linux environment is available:**

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
parsed. No PCI configuration space, ROM BAR, MMIO, VRAM, firmware interface,
driver state, or system configuration was accessed. The only device-level
access in this record is the read-only Windows device-model query listed
above, which reports enumeration and identity, not IP Discovery or firmware
data.

## Evidence required to unblock IP and firmware attribution

Board **presence and PCI identity** are satisfied by the capture above. The
following is still required before any IP or firmware row can move off
**UNVERIFIED**:

1. ~~`lspci -nn` identity for vendor/device `1002:7480`, including subsystem
   IDs, revision, and the exact PCI BDF.~~ **Satisfied** by the read-only
   Windows device model: vendor `1002`, device `7480`, subsystem `148C:2421`,
   revision `CF`, instance `6&EB99E4&0&00000009`. Board make/model and VBIOS
   identity/version are **still outstanding**; the subsystem vendor name is
   unresolved without `pci.ids`.
2. Full relevant `dmesg` boot slice around amdgpu binding and initialization,
   not just matching lines, so request failures and context are retained.
3. Every readable `/sys/kernel/debug/dri/*/amdgpu_firmware_info` file and
   relevant `/sys/bus/pci/devices/<BDF>/` identity attributes listed above.
4. `amdgpu-discover` or atomtree output if already available; otherwise state
   `tool unavailable`. Include the original IP Discovery artifact only if it
   already exists and can be supplied with capture method, timestamp,
   tool/version, original filename, size, SHA-256, custody, and board link.
   A VBIOS image from this board, supplied with the same provenance fields,
   is an acceptable equivalent source because the IP Discovery table is
   embedded in the atom firmware image — but it must be supplied or acquired
   under separately stated authorization, not inferred.
5. Linux distribution, kernel version, capture time/time zone, tool versions,
   and any errors or unreadable paths. Keep originals unchanged.

Do not infer missing IP records or firmware requirements from generic Linux
branches, PCI database names, synthetic fixtures, or Navi48 implementation.
Board identity is now verified, but until an IP Discovery artifact or
equivalent capture is independently reviewed, **all board IP and firmware rows
remain UNVERIFIED**, and Phase 4 dispatch implementation is not justified.
