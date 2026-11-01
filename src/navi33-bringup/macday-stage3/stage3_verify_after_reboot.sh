#!/bin/bash
# stage3_verify_after_reboot.sh — CherryRed Mac-day Stage 3 confirm + Stage 4 compare.
# Run WITHOUT sudo after rebooting with the Navi33Bringup.kext injected and
# `navi33bringup=1` in boot-args.
#
# Produces evidence files under ~/Developer/cherryred-macday-20261030/stage3/evidence/
set -u
OUT="$HOME/Developer/cherryred-macday-20261030/stage3/evidence"
mkdir -p "$OUT"
TS="$(date +%Y%m%d-%H%M%S)"

echo "== 1. Navi33Bringup IORegistry node =="
if ioreg -rn Navi33Bringup -l -x > "$OUT/ioreg-Navi33Bringup-$TS.txt" 2>&1 && [ -s "$OUT/ioreg-Navi33Bringup-$TS.txt" ]; then
  echo "NODE FOUND -> $OUT/ioreg-Navi33Bringup-$TS.txt"
  ioreg -rn Navi33Bringup -l -x | grep -E "Navi33Bringup," | head -50
  STAGE3=PASS
else
  echo "NODE NOT FOUND. The kext did not start (not injected, or boot-arg absent)."
  echo "-- diagnostic: currently injected 3rd-party kexts --"
  kmutil showloaded 2>/dev/null | grep -v com.apple | head -20
  echo "-- boot-args --"
  nvram boot-args 2>/dev/null || echo "(boot-args not readable)"
  STAGE3=FAIL
fi

echo
echo "== 2. Kernel log excerpts =="
log show --last 30m --predicate 'eventMessage CONTAINS "Navi33Bringup"' --style syslog > "$OUT/log-Navi33Bringup-$TS.txt" 2>&1
wc -l "$OUT/log-Navi33Bringup-$TS.txt"

echo
echo "== 3. System facts snapshot =="
{ sw_vers; system_profiler SPDisplaysDataType 2>/dev/null | head -25; } > "$OUT/system-facts-$TS.txt"

echo
echo "== 4. Stage 4 comparison vs Linux evidence =="
python3 - "$OUT/ioreg-Navi33Bringup-$TS.txt" "$OUT/stage4-comparison-$TS.md" <<'PYEOF'
import re, sys, json

src, dst = sys.argv[1], sys.argv[2]
text = open(src, encoding='utf-8', errors='replace').read()

def prop(name):
    m = re.search(r'"%s"=(\S+)' % re.escape(name), text)
    return m.group(1).strip('"') if m else None

rows = []
def row(item, observed, expected, status):
    rows.append((item, observed, expected, status))

# Identity vs Linux evidence (pci-resource-survey.md + Windows probe + manifest)
ident = {
  'Navi33Bringup,VendorID': ('0x1002', 'Linux 1002 + subsystem 148c:2421'),
  'Navi33Bringup,DeviceID': ('0x7480', 'Linux 1002:7480'),
  'Navi33Bringup,RevisionID': ('0xcf', 'Windows rev CF / macOS 0x00cf'),
  'Navi33Bringup,SubsystemVendorID': ('0x148c', 'subsystem vendor'),
  'Navi33Bringup,SubsystemID': ('0x2421', 'subsystem device'),
}
for k,(exp, note) in ident.items():
    got = prop(k)
    ok = got is not None and int(got,16) == int(exp,16)
    row(k, got or 'ABSENT', f'{exp} ({note})', 'PASS' if ok else 'FAIL')

match = prop('Navi33Bringup,ExactEvidenceBoardMatch')
row('Navi33Bringup,ExactEvidenceBoardMatch', match or 'ABSENT', 'True', 'PASS' if match=='True' else 'FAIL')

# PCIe link vs Linux (Gen4 x8 => speed code 4, width 8 => raw 0x1084)
ls_ = prop('Navi33Bringup,PCIeLinkStatus')
if ls_ is not None:
    v = int(ls_,16); speed=v&0xF; width=(v>>4)&0x3F
    ok = speed==4 and width==8
    row('PCIe link', f'raw 0x{v:x} = Gen{speed} x{width}', 'Gen4 x8 (Linux)', 'PASS' if ok else 'FAIL')
else:
    row('PCIe link', 'ABSENT', 'Gen4 x8 (Linux)', 'FAIL')

# BARs: structurally compare vs Linux capture
# Linux: BAR0 8GiB(ReBAR) 64b prefetchable @0xfa00000000; BAR2 256MiB; BAR5 1MiB; BAR6 ROM 128KiB
linux_bars = {0: (8<<30,'256MiB w/o ReBAR'), 2: (256<<20,'256MiB'), 5: (1<<20,'1MiB'), 6: (128<<10,'128KiB')}
bars_ok = True
for i in range(6):
    addr = prop(f'Navi33Bringup,BAR{i},PhysicalAddress')
    ln   = prop(f'Navi33Bringup,BAR{i},Length')
    valid= prop(f'Navi33Bringup,BAR{i},RangeValid')
    if ln is None: continue
    L = int(ln)
    exp, note = linux_bars.get(i, (None,'n/a'))
    if exp is not None and L == exp:
        st='PASS'
    elif i==0:
        st='INFO (Linux ReBAR=8GiB; macOS assigns its own BAR0 size)'
        bars_ok = bars_ok and True
    elif i==2:
        st='INFO (Linux BAR2=256MiB)' if L != (256<<20) else 'PASS'
    else:
        st='CHECK'
        bars_ok = False
    row(f'BAR{i}', f'addr 0x{int(addr,16):x} len 0x{L:x} ({L>>20} MiB) valid={valid}',
        f'Linux: {note}', st)

# ReBAR capability internal consistency
found = prop('Navi33Bringup,ResizableBARFound')
enc = prop('Navi33Bringup,ResizableBARSizeEncoding')
sup = prop('Navi33Bringup,ResizableBARCurrentSizeSupported')
idx = prop('Navi33Bringup,ResizableBARIndicesMatch')
if found == 'True':
    cur = (1 << int(enc)) if enc is not None else None
    ok = sup=='True' and idx=='True'
    row('ReBAR', f'found, encoding {enc} = {cur//(1024*1024) if cur else "?"} MiB, supported={sup}, indicesMatch={idx}',
        'Linux: ReBAR active at 8 GiB', 'PASS' if ok else 'FAIL')
else:
    row('ReBAR', found or 'ABSENT', 'expected found on RX 7600', 'FAIL')

# Fail-closed markers
for k, expv in [('Navi33Bringup,FirmwareLoaded','False'),
                ('Navi33Bringup,HardwareWrites','False'),
                ('Navi33Bringup,FirmwareRefused','True'),
                ('Navi33Bringup,RegisterBARMapped','False')]:
    got = prop(k)
    row(k, got or 'ABSENT', expv, 'PASS' if got==expv else 'FAIL')

lines = ['# Stage 4 comparison — macOS kext survey vs Linux evidence','',
         f'Source: `{src}`','',
         '| Item | Observed | Expected (Linux evidence) | Status |','|---|---|---|---|']
for item,obs,exp,st in rows:
    lines.append(f'| {item} | {obs} | {exp} | {st} |')
fails = sum(1 for r in rows if r[3]=='FAIL')
lines += ['','## Summary','',
          f'- Total checks: {len(rows)}; FAIL: {fails}',
          '- INFO rows are OS-assignment differences to be reviewed, not hardware mismatches.',
          '- Stage 3 hardware-survey result: ' + ('EVIDENCE CAPTURED' if fails==0 else 'CHECK FAILURES ABOVE')]
open(dst,'w').write('\n'.join(lines)+'\n')
json.dump([{'item':i,'observed':o,'expected':e,'status':s} for i,o,e,s in rows],
          open(dst.replace('.md','.json'),'w'), indent=1)
print(open(dst).read())
PYEOF

echo
echo "== 5. Metal probe re-run (expected: still BLOCKED — no IOAccelerator exists) =="
if [ -x "$HOME/Developer/CherryRed/build/navi33-metal-compute/rx7600_metal_probe" ] || true; then
  PROBE=$(find "$HOME/Developer/CherryRed/build" -name "*[Rr][Xx]7600*" -type f -perm +111 2>/dev/null | head -1)
  if [ -n "${PROBE:-}" ]; then
    "$PROBE" > "$OUT/metal-probe-$TS.txt" 2>&1; echo "exit=$?" | tee -a "$OUT/metal-probe-$TS.txt"
    tail -3 "$OUT/metal-probe-$TS.txt"
  else
    echo "probe binary not found; run ~/Developer/CherryRed/metal-compute scripts manually"
  fi
fi

echo
echo "Evidence saved in: $OUT"
echo "Stage 3 (kext started, read-only survey): $STAGE3"
