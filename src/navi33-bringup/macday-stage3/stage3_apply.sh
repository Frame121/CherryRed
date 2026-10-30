#!/bin/bash
# stage3_apply.sh — CherryRed Mac-day Stage 3 (runbook step 3)
# Installs the built read-only Navi33Bringup.kext into the OpenCore EFI and
# adds the exact opt-in boot argument `navi33bringup=1`.
#
# MUST BE RUN WITH SUDO. Run from a terminal as:
#   sudo sh stage3_apply.sh
#
# Safety:
#   1. Backs up the entire ESP EFI folder twice (local + USB stick if mounted).
#   2. The kext is fail-closed: without `navi33bringup=1` it refuses to start,
#      and with it, it performs PCI config-space READS ONLY (no MMIO, no
#      writes, no firmware load).
#   3. If OpenCore fails to inject the kext, boot proceeds WITHOUT it; the
#      only trace is a line in the OpenCore log.
# Rollback: re-run with --rollback  (removes the kext and the boot-arg entry)
set -eu

BACKUP_LOCAL="$HOME/Developer/cherryred-macday-20261030/stage3"
KEXT_SRC="$BACKUP_LOCAL/Navi33Bringup.kext"
OC_KEXTS="EFI/OC/Kexts"
PLIST="EFI/OC/config.plist"
BOOTARG="navi33bringup=1"

[ "$(id -u)" -eq 0 ] || { echo "ERROR: run with sudo"; exit 1; }
[ -d "$KEXT_SRC" ] || { echo "ERROR: $KEXT_SRC missing"; exit 1; }

echo "== Mounting ESP (disk0s1) =="
diskutil mount disk0s1 2>/dev/null || echo "  (mount reported an error — likely already mounted, continuing)"
# The ESP label is "NO NAME"; the mount point contains a space, so take the
# whole remainder of the Mount Point line, not just one field.
ESP="$(diskutil info disk0s1 | sed -n 's/^[[:space:]]*Mount Point:[[:space:]]*//p' | head -1)"
if [ -z "$ESP" ] || [ ! -d "$ESP/$PLIST" ]; then
  echo "ERROR: $PLIST not found under ESP mount point '${ESP:-<none>}'"
  diskutil unmount disk0s1 2>/dev/null || true
  exit 1
fi
echo "ESP: $ESP"

TS="$(date +%Y%m%d-%H%M%S)"

if [ "${1:-}" = "--rollback" ]; then
  echo "== ROLLBACK: removing kext + boot-arg =="
  rm -rf "$ESP/$OC_KEXTS/Navi33Bringup.kext"
  python3 - "$ESP/$PLIST" "$BOOTARG" <<'PYEOF'
import plistlib, sys
p = sys.argv[1]; arg = sys.argv[2]
with open(p,'rb') as f: cfg = plistlib.load(f)
nv = cfg['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']
ba = nv.get('boot-args','')
parts = [x for x in ba.split() if x and x != arg]
nv['boot-args'] = ' '.join(parts)
add = cfg['Kernel']['Add']
cfg['Kernel']['Add'] = [e for e in add if e.get('BundlePath') != 'Navi33Bringup.kext']
with open(p,'wb') as f: plistlib.dump(cfg, f, sort_keys=True)
print('boot-args now:', nv['boot-args'])
PYEOF
  plutil -lint "$ESP/$PLIST"
  diskutil unmount disk0s1 || true
  echo "Rollback done. Reboot to return to the pre-Stage-3 state."
  exit 0
fi

echo "== Backup 1/2: local copy =="
mkdir -p "$BACKUP_LOCAL/EFI-backup-$TS"
cp -R "$ESP/EFI" "$BACKUP_LOCAL/EFI-backup-$TS/"
echo "  -> $BACKUP_LOCAL/EFI-backup-$TS"

echo "== Backup 2/2: USB stick (if a writable volume is mounted) =="
for VOL in /Volumes/*; do
  if [ -w "$VOL" ] && [ "$VOL" != "$ESP" ] && [ "$VOL" != "/Volumes/GameAndFile" ]; then
    if cp -R "$ESP/EFI" "$VOL/EFI-backup-$TS" 2>/dev/null; then
      echo "  -> $VOL/EFI-backup-$TS"
      break
    fi
  fi
done

echo "== Install kext =="
mkdir -p "$ESP/$OC_KEXTS"
rm -rf "$ESP/$OC_KEXTS/Navi33Bringup.kext"
cp -R "$KEXT_SRC" "$ESP/$OC_KEXTS/Navi33Bringup.kext"
codesign --verify --deep --strict "$ESP/$OC_KEXTS/Navi33Bringup.kext" && echo "  signature OK on ESP copy"

echo "== Patch config.plist =="
python3 - "$ESP/$PLIST" "$BOOTARG" <<'PYEOF'
import plistlib, sys
p, arg = sys.argv[1], sys.argv[2]
with open(p,'rb') as f: cfg = plistlib.load(f)

nv = cfg['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']
ba = nv.get('boot-args','')
if arg in ba.split():
    print('boot-arg already present:', ba)
else:
    nv['boot-args'] = (ba + ' ' + arg).strip()
    print('boot-args now:', nv['boot-args'])

add = cfg['Kernel'].get('Add', [])
if any(e.get('BundlePath') == 'Navi33Bringup.kext' for e in add):
    print('Kernel/Add entry already present')
else:
    add.append({
        'Arch': 'x86_64',
        'BundlePath': 'Navi33Bringup.kext',
        'Comment': 'CherryRed Navi33 bring-up scaffold (read-only, opt-in)',
        'Enabled': True,
        'ExecutablePath': 'Contents/MacOS/Navi33Bringup',
        'MaxKernel': '',
        'MinKernel': '',
        'PlistPath': 'Contents/Info.plist',
    })
    cfg['Kernel']['Add'] = add
    print('Kernel/Add entry appended (Enabled=true)')

with open(p,'wb') as f: plistlib.dump(cfg, f, sort_keys=True)
PYEOF

echo "== Validate plist =="
plutil -lint "$ESP/$PLIST"

echo "== Sanity: kext bundle on ESP =="
ls "$ESP/$OC_KEXTS/Navi33Bringup.kext/Contents/MacOS/"
diskutil unmount disk0s1 || true

echo
echo "DONE. Next step: reboot (sudo shutdown -r now)."
echo "After reboot, run (no sudo, from this same directory):"
echo "  sh $BACKUP_LOCAL/stage3_verify_after_reboot.sh"
echo "Rollback at any time: sudo sh $BACKUP_LOCAL/stage3_apply.sh --rollback"
