#!/bin/bash
# stage3_apply.sh — CherryRed Mac-day Stage 3 (runbook step 3)
# Installs the built read-only Navi33Bringup.kext into the OpenCore EFI and
# adds the exact opt-in boot argument `navi33bringup=1`.
#
# MUST BE RUN WITH SUDO:
#   sudo sh stage3_apply.sh [ /path/to/OC-root ]
#   sudo sh stage3_apply.sh --rollback [ /path/to/OC-root ]
#
# The OC root is located automatically (EFI/OC under the ESP, else the
# directory containing OpenCore.efi); pass the path explicitly to override.
#
# Safety:
#   1. Backs up the entire EFI folder twice (local + USB stick if mounted).
#   2. The kext is fail-closed: without `navi33bringup=1` it refuses to start,
#      and with it, it performs PCI config-space READS ONLY (no MMIO, no
#      writes, no firmware load).
#   3. If OpenCore fails to inject the kext, boot proceeds WITHOUT it.
set -eu

BACKUP_LOCAL="$HOME/Developer/cherryred-macday-20261030/stage3"
KEXT_SRC="$BACKUP_LOCAL/Navi33Bringup.kext"
BOOTARG="navi33bringup=1"
MODE="install"
if [ "${1:-}" = "--rollback" ]; then MODE="--rollback"; EXPLICIT_OC="${2:-}"; else EXPLICIT_OC="${1:-}"; fi

[ "$(id -u)" -eq 0 ] || { echo "ERROR: run with sudo"; exit 1; }
[ -d "$KEXT_SRC" ] || { echo "ERROR: $KEXT_SRC missing"; exit 1; }

echo "== Mounting ESP (disk0s1) =="
diskutil mount disk0s1 2>/dev/null || echo "  (mount reported an error — likely already mounted, continuing)"
ESP="$(diskutil info disk0s1 | sed -n 's/^[[:space:]]*Mount Point:[[:space:]]*//p' | head -1)"
if [ -z "$ESP" ] || [ ! -d "$ESP" ]; then
  echo "ERROR: disk0s1 has no mount point"
  exit 1
fi
echo "ESP: $ESP"

TS="$(date +%Y%m%d-%H%M%S)"

locate_oc_root() {
  local esp="$1"
  if [ -f "$esp/EFI/OC/config.plist" ]; then echo "$esp/EFI/OC"; return 0; fi
  local ocdir
  ocdir="$(dirname "$(find "$esp" -maxdepth 4 -name OpenCore.efi 2>/dev/null | head -1)")"
  if [ -n "$ocdir" ] && [ "$ocdir" != "." ] && [ -f "$ocdir/config.plist" ]; then
    echo "$ocdir"; return 0
  fi
  return 1
}

OCROOT=""
if [ -n "$EXPLICIT_OC" ]; then
  [ -f "$EXPLICIT_OC/config.plist" ] || { echo "ERROR: no config.plist in $EXPLICIT_OC"; exit 1; }
  OCROOT="$EXPLICIT_OC"
else
  OCROOT="$(locate_oc_root "$ESP")" || OCROOT=""
fi

if [ -z "$OCROOT" ]; then
  echo "ERROR: OpenCore root not found on $ESP. ESP layout:"
  find "$ESP" -maxdepth 3 -not -path '*/System Volume Information*' 2>/dev/null | head -40
  echo
  echo "Hint: if OpenCore boots from another volume (e.g. a USB stick), find its"
  echo "OC folder (the one containing OpenCore.efi and config.plist) and re-run:"
  echo "  sudo sh $0 -- $HOME/path/to/OC"
  echo "or: sudo sh $0 --rollback /path/to/OC"
  diskutil unmount disk0s1 2>/dev/null || true
  exit 1
fi
CONFIG="$OCROOT/config.plist"
KEXTS_DIR="$OCROOT/Kexts"
echo "OpenCore root: $OCROOT"

if [ "${1:-}" = "--rollback" ]; then
  echo "== ROLLBACK: removing kext + boot-arg =="
  rm -rf "$KEXTS_DIR/Navi33Bringup.kext"
  python3 - "$CONFIG" "$BOOTARG" <<'PYEOF'
import plistlib, sys
p = sys.argv[1]; arg = sys.argv[2]
with open(p,'rb') as f: cfg = plistlib.load(f)
nv = cfg['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']
ba = nv.get('boot-args','')
parts = [x for x in ba.split() if x and x != arg]
nv['boot-args'] = ' '.join(parts)
cfg['Kernel']['Add'] = [e for e in cfg['Kernel'].get('Add', [])
                        if e.get('BundlePath') != 'Navi33Bringup.kext']
with open(p,'wb') as f: plistlib.dump(cfg, f, sort_keys=True)
print('boot-args now:', nv['boot-args'])
PYEOF
  plutil -lint "$CONFIG"
  diskutil unmount disk0s1 2>/dev/null || true
  echo "Rollback done. Reboot to return to the pre-Stage-3 state."
  exit 0
fi

echo "== Backup 1/2: local copy =="
mkdir -p "$BACKUP_LOCAL/EFI-backup-$TS"
cp -R "$(dirname "$OCROOT")" "$BACKUP_LOCAL/EFI-backup-$TS/"
echo "  -> $BACKUP_LOCAL/EFI-backup-$TS"

echo "== Backup 2/2: USB stick (if a writable volume is mounted) =="
for VOL in /Volumes/*; do
  if [ -w "$VOL" ] && [ "$VOL" != "$ESP" ] && [ "$VOL" != "/Volumes/GameAndFile" ]; then
    if cp -R "$(dirname "$OCROOT")" "$VOL/EFI-backup-$TS" 2>/dev/null; then
      echo "  -> $VOL/EFI-backup-$TS"
      break
    fi
  fi
done

echo "== Install kext =="
mkdir -p "$KEXTS_DIR"
rm -rf "$KEXTS_DIR/Navi33Bringup.kext"
cp -R "$KEXT_SRC" "$KEXTS_DIR/Navi33Bringup.kext"
codesign --verify --deep --strict "$KEXTS_DIR/Navi33Bringup.kext" && echo "  signature OK on ESP copy"

echo "== Patch config.plist =="
python3 - "$CONFIG" "$BOOTARG" <<'PYEOF'
import plistlib, sys
p, arg = sys.argv[1], sys.argv[2]
with open(p,'rb') as f: cfg = plistlib.load(f)

nv = cfg['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']
# The host may carry a stale NVRAM boot-args value; OpenCore's Add does not
# override an existing variable, so Delete must clear it for Add to apply.
deletemap = cfg['NVRAM'].setdefault('Delete', {})
delarr = deletemap.setdefault('7C436110-AB2A-4BBB-A880-FE41995C9F82', [])
if 'boot-args' not in delarr:
    delarr.append('boot-args')
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
plutil -lint "$CONFIG"

echo "== Sanity: kext bundle in place =="
ls "$KEXTS_DIR/Navi33Bringup.kext/Contents/MacOS/"
diskutil unmount disk0s1 2>/dev/null || true

echo
echo "DONE. Next step: reboot (sudo shutdown -r now)."
echo "After reboot, run (no sudo):"
echo "  sh $BACKUP_LOCAL/stage3_verify_after_reboot.sh"
echo "Rollback: sudo sh $BACKUP_LOCAL/stage3_apply.sh --rollback ${EXPLICIT_OC:+\"$EXPLICIT_OC\"}"
