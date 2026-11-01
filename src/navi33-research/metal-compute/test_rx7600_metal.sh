#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SRC="$ROOT/src/navi33-research/metal-compute"
OUT="$ROOT/build/navi33-metal-compute"
mkdir -p "$OUT"

xcrun -sdk macosx metal -c "$SRC/VectorAdd.metal" -o "$OUT/VectorAdd.air"
xcrun -sdk macosx metallib "$OUT/VectorAdd.air" -o "$OUT/CherryRedCompute.metallib"
xcrun swiftc -O -framework Foundation -framework Metal -framework IOKit \
  "$SRC/RX7600MetalComputeProbe.swift" -o "$OUT/RX7600MetalComputeProbe"

echo "Build PASS: RX7600-specific Metal hardware-verification probe compiled."
echo "Runtime acceptance requires the exact PCI/subsystem identity and a successful GPU dispatch."
exec "$OUT/RX7600MetalComputeProbe" "$OUT/CherryRedCompute.metallib"
