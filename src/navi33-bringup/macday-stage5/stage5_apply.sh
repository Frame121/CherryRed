#!/bin/bash
# stage5_apply.sh — CherryRed Mac-day Stage 5 (PSP mailbox survey + PSP load).
# Installs the NEW Navi33Bringup.kext (with the read-only PSP mailbox survey
# AND the opt-in PSP firmware-load stage) into the OpenCore EFI and sets the
# boot-args for the requested mode.
#
# MUST BE RUN WITH SUDO:
#   sudo sh stage5_apply.sh survey /path/to/Navi33Bringup.kext
#   sudo sh stage5_apply.sh kdb    /path/to/Navi33Bringup.kext
#   sudo sh stage5_apply.sh chain  /path/to/Navi33Bringup.kext
#   sudo sh stage5_apply.sh --rollback
#
# Modes (evidence-gated order, one reboot each):
#   survey — navi33bringup=1 navi33psp=1            (read-only mailbox survey)
#   kdb    — + navi33psp_load=1                     (FIRST WRITE: KDB only)
#   chain  — + navi33psp_chain=1                    (full chain -> sOS alive)
#
# Safety mirrors stage3_apply.sh: backs up the EFI first, the kext is
# fail-closed (no boot-arg = nothing runs), and any navi33psp_* load token
# from a previous mode is removed before the new one is added.
set -eu

MODE="${1:-}"
KEXT_SRC="${2:-}"
ROLLBACK=0
if [ "$MODE" = "--rollback" ]; then ROLLBACK=1; MODE="rollback"; fi
case "$MODE" in
  survey|kdb|chain) [ -n "$KEXT_SRC" ] || { echo "ERROR: pass the kext path (stage5_apply.sh $MODE /path/to/Navi33Bringup.kext)"; exit 1; } ;;
  rollback) ;;
  *) echo "usage: sudo sh $0 [survey|kdb|chain <kext>] | [--rollback]"; exit 1 ;;
esac

[ "$(id -u)" -eq 0 ] || { echo "ERROR: run with sudo"; exit 1; }
if [ "$ROLLBACK" = 0 ]; then
  [ -d "$KEXT_SRC/Contents/MacOS/Navi33Bringup" ] || { echo "ERROR: $KEXT_SRC is not a kext bundle"; exit 1; }
fi

echo "== Mounting ESP (disk0s1) =="
diskutil mount disk0s1 2>/dev/null || echo "  (mount reported an error — likely already mounted, continuing)"
ESP="$(diskutil info disk0s1 | sed -n 's/^[[:space:]]*Mount Point:[[:space:]]*//p' | head -1)"
[ -n "$ESP" ] && [ -d "$ESP" ] || { echo "ERROR: disk0s1 has no mount point"; exit 1; }
echo "ESP: $ESP"

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
OCROOT="$(locate_oc_root "$ESP")" || { echo "ERROR: OpenCore root not found on $ESP"; exit 1; }
CONFIG="$OCROOT/config.plist"
KEXTS_DIR="$OCROOT/Kexts"
TS="$(date +%Y%m%d-%H%M%S)"
BACKUP_DIR="$(dirname "$OCROOT")/../EFI-backup-stage5-$TS"
echo "OpenCore root: $OCROOT"

echo "== Backup EFI =="
cp -R "$(dirname "$OCROOT")" "$BACKUP_DIR"
echo "  -> $BACKUP_DIR"

python3 - "$CONFIG" "$MODE" <<'PYEOF'
import plistlib, sys
p, mode = sys.argv[1], sys.argv[2]
with open(p,'rb') as f: cfg = plistlib.load(f)
nv = cfg['NVRAM']['Add']['7C436110-AB2A-4BBB-A880-FE41995C9F82']
ba = nv.get('boot-args','')
tokens = [x for x in ba.split() if x]

# Remove any previous navi33psp load-mode tokens; keep the base opt-ins.
for drop in ('navi33psp_load=1','navi33psp_chain=1'):
    tokens = [t for t in tokens if t != drop]

if mode == 'rollback':
    for drop in ('navi33bringup=1','navi33psp=1'):
        tokens = [t for t in tokens if t != drop]
    # Also drop the kext from Kernel/Add so boot returns to the pre-stage3 state.
    cfg['Kernel']['Add'] = [e for e in cfg['Kernel'].get('Add', [])
                            if e.get('BundlePath') != 'Navi33Bringup.kext']
else:
    for want in ('navi33bringup=1','navi33psp=1'):
        if want not in tokens: tokens.append(want)
    if mode == 'kdb': tokens.append('navi33psp_load=1')
    if mode == 'chain': tokens.append('navi33psp_chain=1')

nv['boot-args'] = ' '.join(tokens)
# Stale NVRAM must not mask the new value (same fix as stage3_apply.sh).
deletemap = cfg['NVRAM'].setdefault('Delete', {})
delarr = deletemap.setdefault('7C436110-AB2A-4BBB-A880-FE41995C9F82', [])
if 'boot-args' not in delarr:
    delarr.append('boot-args')
with open(p,'wb') as f: plistlib.dump(cfg, f, sort_keys=True)
print('boot-args now:', nv['boot-args'])
PYEOF

plutil -lint "$CONFIG"

if [ "$ROLLBACK" = 0 ]; then
  echo "== Install kext =="
  mkdir -p "$KEXTS_DIR"
  rm -rf "$KEXTS_DIR/Navi33Bringup.kext"
  cp -R "$KEXT_SRC" "$KEXTS_DIR/Navi33Bringup.kext"
  codesign --verify --deep --strict "$KEXTS_DIR/Navi33Bringup.kext" && echo "  signature OK on ESP copy"
  # The Kernel/Add entry exists from stage3; make sure it is present anyway.
  python3 - "$CONFIG" <<'PYEOF'
import plistlib, sys
with open(sys.argv[1],'rb') as f: cfg = plistlib.load(f)
add = cfg['Kernel'].get('Add', [])
if not any(e.get('BundlePath') == 'Navi33Bringup.kext' for e in add):
    add.append({'Arch':'x86_64','BundlePath':'Navi33Bringup.kext',
                'Comment':'CherryRed Navi33 bring-up (survey + opt-in PSP load)',
                'Enabled':True,'ExecutablePath':'Contents/MacOS/Navi33Bringup',
                'MaxKernel':'','MinKernel':'','PlistPath':'Contents/Info.plist'})
    cfg['Kernel']['Add'] = add
    with open(sys.argv[1],'wb') as f: plistlib.dump(cfg, f, sort_keys=True)
    print('Kernel/Add entry added')
else:
    print('Kernel/Add entry already present')
PYEOF
  plutil -lint "$CONFIG"
else
  echo "== Rollback: removing kext =="
  rm -rf "$KEXTS_DIR/Navi33Bringup.kext"
fi

diskutil unmount disk0s1 2>/dev/null || true
echo
echo "DONE. Next step: reboot (sudo shutdown -r now), then run:"
echo "  sh stage5_verify_after_reboot.sh [survey|kdb|chain]"
echo "Rollback: sudo sh $0 --rollback"
