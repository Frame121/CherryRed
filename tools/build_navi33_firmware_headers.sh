#!/usr/bin/env bash
set -euo pipefail

FW_DIR="${1:-/usr/lib/firmware/amdgpu}"
OUT_DIR="${2:-RedNavi}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

command -v zstd >/dev/null 2>&1 || { echo "zstd is required to unpack linux-firmware .zst files" >&2; exit 1; }
for f in \
  psp_13_0_7_sos.bin \
  smu_13_0_7.bin \
  gc_11_0_2_pfp.bin gc_11_0_2_me.bin gc_11_0_2_mec.bin \
  gc_11_0_2_rlc.bin gc_11_0_2_imu.bin gc_11_0_2_mes_2.bin gc_11_0_2_mes1.bin \
  sdma_6_0_2.bin \
  dcn_3_2_1_dmcub.bin; do
  src="${FW_DIR}/${f}.zst"
  if [[ ! -f "$src" ]]; then
    echo "Missing firmware input: $src" >&2
    exit 2
  fi
  zstd -q -d -c "$src" > "${TMP}/${f}"
done

cd "$ROOT"
mkdir -p "$OUT_DIR"
python3 tools/gen_psp_fw_header.py "${TMP}/psp_13_0_7_sos.bin" "${OUT_DIR}/psp_fw_navi33.h"
python3 tools/gen_smu_fw_header.py "${TMP}/smu_13_0_7.bin" "${OUT_DIR}/smu_fw_navi33.h"
python3 tools/gen_gc_fw_header.py "$TMP" "${OUT_DIR}/gc_fw_navi33.h" 11_0_2 6_0_2
python3 tools/gen_dmub_fw_header.py "${TMP}/dcn_3_2_1_dmcub.bin" 57856 "${OUT_DIR}/dmub_fw_navi33.h"
python3 tools/verify_navi33_firmware_headers.py "$OUT_DIR"

echo "Navi 33 local firmware headers generated under ${OUT_DIR}."
echo "These headers embed AMD firmware payloads; keep them local and do not commit or redistribute them."
echo "PSP/SMU/DMCUB header versions are checked against the RX7600 runtime evidence in the accompanying research notes."
