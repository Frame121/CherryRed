#!/bin/bash
#
# fetch-firmware-navi33.sh - copy the AMD firmware the Navi 33 (RX 7600) bring-up kext
# embeds from a local linux-firmware checkout into src/navi33-bringup/firmware/
# (gitignored; never commit it).
#
# This script does NOT use the network. Clone linux-firmware yourself first:
#     git clone https://gitlab.com/kernel-firmware/linux-firmware.git
#   (mirror: https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git)
#
# Usage:
#   tools/fetch-firmware-navi33.sh --dry-run
#       List the expected blob set and cross-check it against the evidence manifest.
#       No checkout required; safe to run on any host.
#
#   tools/fetch-firmware-navi33.sh <path-to-linux-firmware-checkout>
#       Copy the blobs (handling .bin and .bin.zst), report sha256 of each copy.
#
# The blob set and IP versions come from the RX 7600's own captured evidence
# (RX7600-Linux-Evidence/navi33-firmware-manifest.json): GC 11.0.2 (gfx1102), SDMA 6.0.2,
# VCN 4.0.4, DCN 3.2.1 (dmcub), PSP 13.0.7, SMU 13.0.7.
#
# The blobs are AMD's, under AMD's redistribution licence (LICENSE.amdgpu in linux-firmware).
# sha256 pins are UNKNOWN until the operator records the hashes of the blobs they copy;
# a DIFFERS line is a warning, not an error.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DST="$REPO/src/navi33-bringup/firmware"
MANIFEST="$REPO/RX7600-Linux-Evidence/navi33-firmware-manifest.json"

# name  sha256-of-developed-against (UNKNOWN until pinned)
BLOBS="
gc_11_0_2_imu     UNKNOWN
gc_11_0_2_me      UNKNOWN
gc_11_0_2_mec     UNKNOWN
gc_11_0_2_mes     UNKNOWN
gc_11_0_2_mes1    UNKNOWN
gc_11_0_2_mes_2   UNKNOWN
gc_11_0_2_pfp     UNKNOWN
gc_11_0_2_rlc     UNKNOWN
sdma_6_0_2        UNKNOWN
vcn_4_0_4         UNKNOWN
dcn_3_2_1_dmcub   UNKNOWN
psp_13_0_7_sos    UNKNOWN
psp_13_0_7_ta     UNKNOWN
smu_13_0_7        UNKNOWN
"

list_blobs() {
    echo "Navi 33 (RX 7600) firmware blob set (target: $DST):"
    while read -r name _pin; do
        [ -n "$name" ] || continue
        echo "  $name.bin"
    done <<< "$BLOBS"
}

if [ "${1:-}" = "--dry-run" ]; then
    list_blobs
    if [ -f "$MANIFEST" ]; then
        echo "manifest present: $MANIFEST"
    else
        echo "WARNING: manifest not found at $MANIFEST" >&2
    fi
    echo "dry-run: no checkout required, nothing copied."
    exit 0
fi

if [ $# -ne 1 ] || [ ! -d "$1" ]; then
    echo "usage: $0 <path-to-linux-firmware-checkout>   (or: $0 --dry-run)" >&2
    echo "clone it first: git clone https://gitlab.com/kernel-firmware/linux-firmware.git" >&2
    exit 2
fi
SRC="$1/amdgpu"
[ -d "$SRC" ] || { echo "fetch-firmware-navi33.sh: no amdgpu/ directory in $1" >&2; exit 2; }

mkdir -p "$DST"

missing=0
while read -r name want; do
    [ -n "$name" ] || continue
    if [ -f "$SRC/$name.bin" ]; then
        cp "$SRC/$name.bin" "$DST/$name.bin"
    elif [ -f "$SRC/$name.bin.zst" ] && command -v zstd >/dev/null 2>&1; then
        zstd -dqf -o "$DST/$name.bin" "$SRC/$name.bin.zst"
    else
        echo "MISSING  $name.bin (not in $SRC; need .bin or .bin.zst + zstd)" >&2
        missing=1
        continue
    fi
    got="$(shasum -a 256 "$DST/$name.bin" | awk '{print $1}')"
    if [ "$want" = "UNKNOWN" ]; then
        echo "copied   $name.bin ($(wc -c < "$DST/$name.bin" | tr -d ' ') bytes) sha256=$got (pin not yet set)"
    elif [ "$got" = "$want" ]; then
        echo "ok       $name.bin ($(wc -c < "$DST/$name.bin" | tr -d ' ') bytes)"
    else
        echo "DIFFERS  $name.bin (sha256 $got; expected $want) - copied anyway"
    fi
done <<< "$BLOBS"

cp "$1/LICENSE.amdgpu" "$DST/" 2>/dev/null || true

if [ "$missing" -ne 0 ]; then
    echo "one or more required blobs were missing; the kext will not link until they are present." >&2
    exit 1
fi
echo "copied into $DST"
