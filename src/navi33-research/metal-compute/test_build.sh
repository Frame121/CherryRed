#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SRC="$ROOT/src/navi33-research/metal-compute"
OUT="$ROOT/build/navi33-metal-compute"

"$SRC/build_and_run.sh" --build-only

test -s "$OUT/VectorAdd.air"
test -s "$OUT/CherryRedCompute.metallib"
test -x "$OUT/MetalComputeProbe"

# Verify the linked metallib actually contains the expected kernel symbol.
if ! xcrun -sdk macosx metal-objdump --metallib --all-headers "$OUT/CherryRedCompute.metallib" >/dev/null 2>&1; then
  echo "FAIL: metallib structural inspection failed" >&2
  exit 1
fi
if ! /usr/bin/strings "$OUT/CherryRedCompute.metallib" | grep -Fq 'cherryred_vector_add'; then
  echo "FAIL: kernel symbol missing from metallib" >&2
  exit 1
fi

# Guard the source contract: bounds check and all four declared buffer slots.
grep -Fq 'if (index >= elementCount) return;' "$SRC/VectorAdd.metal"
grep -Fq '[[buffer(0)]]' "$SRC/VectorAdd.metal"
grep -Fq '[[buffer(1)]]' "$SRC/VectorAdd.metal"
grep -Fq '[[buffer(2)]]' "$SRC/VectorAdd.metal"
grep -Fq '[[buffer(3)]]' "$SRC/VectorAdd.metal"

echo "PASS: Metal source contract, AIR output, metallib structure, kernel symbol, and Swift host build."
echo "SKIP: Hardware dispatch is not counted as passed; run build_and_run.sh on a host with a working Metal device."
