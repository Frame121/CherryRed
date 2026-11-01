#!/bin/bash
# stage5_verify_after_reboot.sh — capture the Stage 5 evidence after a reboot
# (no sudo needed). Usage:  sh stage5_verify_after_reboot.sh [survey|kdb|chain|ring]
#
# Captures: the Navi33Bringup ioreg node, the unified log for the PSP survey,
# load and ring stages, the boot-args, and prints a PASS/FAIL table against
# the mode's expectations. Everything lands in ./evidence-stage5-<ts>/.
set -eu

MODE="${1:-survey}"
TS="$(date +%Y%m%d-%H%M%S)"
OUT="evidence-stage5-$TS"
mkdir -p "$OUT"

echo "== boot-args =="
nvram boot-args > "$OUT/nvram-boot-args.txt" 2>/dev/null || true
sysctl -n kern.bootargs > "$OUT/kern-bootargs.txt" 2>/dev/null || true
cat "$OUT/kern-bootargs.txt" 2>/dev/null || true

echo "== ioreg node =="
ioreg -rn Navi33Bringup -l -x > "$OUT/ioreg-Navi33Bringup.txt"
wc -l < "$OUT/ioreg-Navi33Bringup.txt" | xargs echo "  ioreg lines:"

echo "== unified log (survey + load + ring) =="
log show --last 10m --predicate \
  'eventMessage CONTAINS "Navi33Bringup/PSP" OR eventMessage CONTAINS "Navi33Bringup/PSPLOAD" OR eventMessage CONTAINS "Navi33Bringup/PSPRING"' \
  > "$OUT/log-psp.txt" 2>/dev/null || true
wc -l < "$OUT/log-psp.txt" | xargs echo "  log lines:"

echo "== metal probe (expected: no Metal device) =="
swift -e 'import Metal; print(MTLCopyAllDevices())' > "$OUT/metal-probe.txt" 2>&1 || true

echo
echo "== Summary (mode: $MODE) =="
python3 - "$OUT/ioreg-Navi33Bringup.txt" "$MODE" <<'PYEOF'
import plistlib, sys
path, mode = sys.argv[1], sys.argv[2]
raw = open(path, 'rb').read()
def prop(name, default=None):
    # crude but sufficient extraction from the ioreg XML dump
    key = f'<key>{name}</key>'
    i = raw.find(key.encode())
    if i < 0: return default
    j = raw.find(b'<', i + len(key))
    k = raw.find(b'>', j)
    return raw[j+1:k].decode(errors='replace')

rows = [
    ("Kext node present", len(raw) > 500),
    ("Stage 3 PCI survey", prop("Navi33Bringup,Stage") == "gpu-identified-read-only"),
    ("PspStage ran", prop("Navi33Bringup,PspStage") not in (None, "disabled (boot-arg navi33psp=1 absent)")),
]
if mode in ("kdb", "chain", "ring"):
    rows.append(("PspLoad stage ran", prop("Navi33Bringup,PspLoad,Stage") not in (None,)))
    rows.append(("PspLoad aborted early", (prop("Navi33Bringup,PspLoad,Stage") or "").startswith("aborted")))
    if mode in ("chain", "ring"):
        rows.append(("Chain complete (sOS alive)", prop("Navi33Bringup,PspLoad,ChainComplete") in ("true", "1")))
        rows.append(("FirmwareVerified (accelerator gate)", prop("Navi33Bringup,FirmwareVerified") in ("true", "1")))
if mode == "ring":
    rows.append(("PspRing stage ran", prop("Navi33Bringup,PspRing,Stage") not in (None,)))
    rows.append(("Ring created", prop("Navi33Bringup,PspRing,RingCreated") in ("true", "1")))
    rows.append(("LOAD_TOC fence ok", prop("Navi33Bringup,PspRing,LoadTocFenceOk") in ("true", "1")))
    rows.append(("LOAD_TOC status 0", prop("Navi33Bringup,PspRing,LoadTocStatus") == "0"))
    rows.append(("SETUP_TMR fence ok", prop("Navi33Bringup,PspRing,SetupTmrFenceOk") in ("true", "1")))
    rows.append(("SETUP_TMR status 0", prop("Navi33Bringup,PspRing,SetupTmrStatus") == "0"))
w = max(len(r[0]) for r in rows)
fails = 0
for name, ok in rows:
    print(f"  {'PASS' if ok else 'FAIL'}  {name:<{w}}")
    if not ok: fails += 1
print(f"  => {len(rows)-fails}/{len(rows)} checks passed")
sys.exit(0)
PYEOF

echo
echo "Evidence in: $OUT"
echo "Next: review the PspStage/PspMailboxRegs values against the Linux captures"
echo "(RX7600-Linux-Evidence/) BEFORE enabling the next write mode."
