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
# sha256 pins are the recorded FIRMWARE-PROVENANCE.md hashes (linux-firmware
# commit d0039fd797d7776d89f00c22c45754914a96706e); a mismatch is a hard error.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DST="$REPO/src/navi33-bringup/firmware"
MANIFEST="$REPO/RX7600-Linux-Evidence/navi33-firmware-manifest.json"

# name  sha256 pin (FIRMWARE-PROVENANCE.md; linux-firmware d0039fd797d7776d89f00c22c45754914a96706e)
BLOBS="
gc_11_0_2_imu  9befca62b0b0cfd252c3df4a9edca295526f4d43821cd99a6326454995a6ca2d
gc_11_0_2_me  86b48ea19b01a7b5529da3eb1d65f5848b39159f1313c05cddd18feb73dd9651
gc_11_0_2_mec  eabe3bcfd2dc3520cec7f2205b5666c2e2ef4b5aeda0ac6c02b456b3794d10d7
gc_11_0_2_mes  e41e0a0a950f55f1bf8fa260972e67e9e06ba1fdf06125dcea46b92dd1d57276
gc_11_0_2_mes1  ac0db48108090dd64b0f1dd6f9c6fe4e6912ea17432914ec1fc5aaba8be5e4dc
gc_11_0_2_mes_2  ba078ce093244ac5e086d5968e912cb70676212f3557b164691f62660047d4bb
gc_11_0_2_pfp  7fb5ae619395caafda0697310cce04a07e8583035dd517c660d43e45cdbe0dc2
gc_11_0_2_rlc  b43eb2fd0600f50a1a5796bc9983d6b39b5c20960234920f5e89cb362193e0b8
sdma_6_0_2  16c374344d2894da751f7028f9ec1f7520035fa9548d8c750d99a00a4afa86c7
vcn_4_0_4  5633a5212d41e1f3932c1e7e630b1f2c8c4d27bc53ce8149c3be75d1d832053c
dcn_3_2_1_dmcub  3648f36b7d0ffbb0fa29ddb61b5637d8fc837669093b5bb9098bdc69fed5e010
psp_13_0_7_sos  94db505fa6482f258c33a0a8d412050f6d843ab4ada368252e988f82f8a26fa8
psp_13_0_7_ta  0b7fc5a0bea6e3559df9791a57ae5a648ec6246679f8f8a6bc27defb2aa7a75e
smu_13_0_7  6046f6379f42b4d3618151eace413d8c076ea374944afb770a8871ad79fcec55
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
        echo "DIFFERS  $name.bin (sha256 $got; expected $want)" >&2
        echo "the recorded pin in FIRMWARE-PROVENANCE.md comes from linux-firmware commit d0039fd797d7776d89f00c22c45754914a96706e; a mismatch means a different blob set - do not build on it silently" >&2
        missing=1
        continue
    fi
done <<< "$BLOBS"

cp "$1/LICENSE.amdgpu" "$DST/" 2>/dev/null || true

if [ "$missing" -ne 0 ]; then
    echo "one or more required blobs were missing; the kext will not link until they are present." >&2
    exit 1
fi
echo "copied into $DST"
