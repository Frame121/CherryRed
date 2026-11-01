#!/usr/bin/env bash
# Read-only capture of the AMD IP Discovery binary for the exact RX 7600 PCI function.
# Run from a Linux session with the target card bound to amdgpu and debugfs mounted.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

[[ "$(uname -s)" == "Linux" ]] || fail "Linux is required; this script cannot run on macOS."
[[ "$EUID" -eq 0 ]] || fail "Run with sudo so the read-only debugfs blob can be read."
[[ $# -eq 1 ]] || fail "Usage: sudo $0 /absolute/path/to/amdgpu_discovery.bin"

DEST="$1"
[[ "$DEST" = /* ]] || fail "Output path must be absolute."
[[ ! -e "$DEST" ]] || fail "Refusing to overwrite existing output: $DEST"
[[ ! -e "${DEST}.provenance.json" && ! -e "${DEST}.parsed.json" ]] || fail "Refusing to overwrite an existing sidecar for: $DEST"
umask 077
PARENT="$(dirname "$DEST")"
[[ -d "$PARENT" && -w "$PARENT" ]] || fail "Output parent must already exist and be writable."

DEBUGFS=/sys/kernel/debug
mountpoint -q "$DEBUGFS" || fail "debugfs is not mounted at $DEBUGFS; mount it explicitly, then retry."

shopt -s nullglob
matches=()
for dev in /sys/bus/pci/devices/*; do
  [[ -r "$dev/vendor" && -r "$dev/device" ]] || continue
  vendor="$(tr '[:upper:]' '[:lower:]' < "$dev/vendor")"
  device="$(tr '[:upper:]' '[:lower:]' < "$dev/device")"
  subvendor="$(tr '[:upper:]' '[:lower:]' < "$dev/subsystem_vendor")"
  subdevice="$(tr '[:upper:]' '[:lower:]' < "$dev/subsystem_device")"
  if [[ "$vendor" == "0x1002" && "$device" == "0x7480" &&
        "$subvendor" == "0x148c" && "$subdevice" == "0x2421" ]]; then
    matches+=("$dev")
  fi
done

[[ ${#matches[@]} -eq 1 ]] || fail "Expected exactly one PCI 1002:7480 / 148c:2421 function; found ${#matches[@]}."
PCI_PATH="$(readlink -f "${matches[0]}")"
BDF="$(basename "$PCI_PATH")"
DRIVER="$(basename "$(readlink -f "$PCI_PATH/driver" 2>/dev/null || true)")"
[[ "$DRIVER" == "amdgpu" ]] || fail "Target $BDF is not bound to amdgpu (driver=$DRIVER)."

debug_matches=()
for name_file in "$DEBUGFS"/dri/*/name; do
  [[ -r "$name_file" ]] || continue
  if grep -Fq "$BDF" "$name_file"; then
    candidate="$(dirname "$name_file")/amdgpu_discovery"
    [[ -r "$candidate" ]] && debug_matches+=("$candidate")
  fi
done
[[ ${#debug_matches[@]} -eq 1 ]] || fail "Expected exactly one debugfs amdgpu_discovery blob for $BDF; found ${#debug_matches[@]}."

SOURCE="${debug_matches[0]}"
BYTES="$(wc -c < "$SOURCE" | tr -d '[:space:]')"
[[ "$BYTES" -ge 512 && "$BYTES" -le 1048576 ]] || fail "Unexpected blob size: $BYTES bytes (expected 512..1048576)."

# The debugfs file is read only; the script does not access BARs, MMIO, VRAM,
# firmware-loading interfaces, reset controls, or GPU registers.
( set -o noclobber; cat "$SOURCE" > "$DEST" )
COPIED_BYTES="$(wc -c < "$DEST" | tr -d '[:space:]')"
[[ "$COPIED_BYTES" == "$BYTES" ]] || fail "Copy size changed ($BYTES -> $COPIED_BYTES); preserve the output for review and stop."

HASH="$(sha256sum "$DEST" | awk '{print $1}')"
STAMP="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
KERNEL="$(uname -r)"
PROVENANCE="${DEST}.provenance.json"
PARSED="${DEST}.parsed.json"

cat > "$PROVENANCE" <<EOF
{
  "evidence_id": "RX7600-IPD-$BDF-$STAMP",
  "claim": "Raw AMD IP Discovery debugfs blob copied from PCI function $BDF.",
  "artifact_path": "$DEST",
  "source_repository": "Local Linux hardware capture",
  "source_revision": "$KERNEL",
  "source_file_or_section": "$SOURCE",
  "checksum": "sha256:$HASH",
  "applies_to": "PCI 1002:7480 subsystem 148c:2421 at $BDF",
  "verification_method": "Read-only copy from Linux AMDGPU debugfs; strict offline parser follows.",
  "verification_status": "UNVERIFIED",
  "limitations": "Capture provenance is recorded by this script but is not independently authenticated; parser checks structure only and does not prove board authenticity or macOS compatibility.",
  "artifact_class": "PHYSICAL_CAPTURE"
}
EOF

printf 'Captured %s bytes from %s\nSHA-256: %s\n' "$BYTES" "$SOURCE" "$HASH"
if python3 "$HERE/dump.py" "$DEST" --mode strict --json --pretty --provenance "$PROVENANCE" > "$PARSED"; then
  printf 'STRICT_PARSE=PASS\nParsed report: %s\n' "$PARSED"
else
  status=$?
  printf 'STRICT_PARSE=FAIL (exit %s); raw capture retained at %s\n' "$status" "$DEST" >&2
  exit "$status"
fi
printf 'Provenance sidecar: %s\n' "$PROVENANCE"
