#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SRC="$ROOT/src/navi33-research/metal-compute"
OUT="$ROOT/build/navi33-metal-compute"
mkdir -p "$OUT"

xcrun -sdk macosx metal -c "$SRC/VectorAdd.metal" -o "$OUT/VectorAdd.air"
xcrun -sdk macosx metallib "$OUT/VectorAdd.air" -o "$OUT/CherryRedCompute.metallib"
swiftc -O -framework Foundation -framework Metal \
  "$SRC/MetalComputeProbe.swift" -o "$OUT/MetalComputeProbe"

echo "Build PASS: Metal shader, metallib and Swift host probe compiled."
if [[ "${1:-}" == "--build-only" ]]; then
  exit 0
fi
exec "$OUT/MetalComputeProbe" "$OUT/CherryRedCompute.metallib"
