#!/bin/sh
set -eu

# Portable, hardware-independent Navi 33 checks for macOS hosts.
# This does NOT build, install, load, or validate a GPU kernel extension.
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
RESEARCH="$ROOT/src/navi33-research"
BRINGUP="$ROOT/src/navi48-bringup"
CXX=${CXX:-clang++}
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/cherryred-navi33-tests.XXXXXX")
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

COMMON="-std=c++17 -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti"

printf '\n== Build Navi 33 research model ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" \
  "$RESEARCH/src/Navi33Research.cpp" \
  "$RESEARCH/tests/test_Navi33Research.cpp" \
  -o "$BUILD_DIR/navi33_research_tests"
"$BUILD_DIR/navi33_research_tests"

printf '\n== Build Navi 33 lifecycle core ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" \
  "$RESEARCH/src/Navi33Research.cpp" \
  "$RESEARCH/src/Navi33DriverCore.cpp" \
  "$RESEARCH/tests/test_Navi33DriverCore.cpp" \
  -o "$BUILD_DIR/navi33_driver_core_tests"
"$BUILD_DIR/navi33_driver_core_tests"

printf '\n== Build Navi 33 bring-up contract model ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" \
  "$RESEARCH/src/Navi33BringupModel.cpp" \
  "$RESEARCH/tests/test_Navi33BringupModel.cpp" \
  -o "$BUILD_DIR/navi33_bringup_model_tests"
"$BUILD_DIR/navi33_bringup_model_tests"

printf '\n== Build macOS integration contract models ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" \
  "$RESEARCH/src/Navi33IntegrationContracts.cpp" \
  "$RESEARCH/tests/test_Navi33IntegrationContracts.cpp" \
  -o "$BUILD_DIR/navi33_integration_contract_tests"
"$BUILD_DIR/navi33_integration_contract_tests"

printf '\n== Build Navi 33 display contract models ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" "$RESEARCH/src/Navi33DisplayContracts.cpp" "$RESEARCH/tests/test_Navi33DisplayContracts.cpp" -o "$BUILD_DIR/navi33_display_contract_tests"
"$BUILD_DIR/navi33_display_contract_tests"

printf '\n== Build Navi 33 compute contract models ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" "$RESEARCH/src/Navi33ComputeContracts.cpp" "$RESEARCH/tests/test_Navi33ComputeContracts.cpp" -o "$BUILD_DIR/navi33_compute_contract_tests"
"$BUILD_DIR/navi33_compute_contract_tests"

printf '\n== Build GFX11 firmware manifest gate ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" \
  "$RESEARCH/src/Navi33Gfx11Manifest.cpp" \
  "$RESEARCH/tests/test_Navi33Gfx11Manifest.cpp" \
  -o "$BUILD_DIR/navi33_gfx11_manifest_tests"
"$BUILD_DIR/navi33_gfx11_manifest_tests"

printf '\n== Build GFX11 bring-up foundations ==\n'
"$CXX" $COMMON -I"$RESEARCH/include" \
  "$RESEARCH/src/Navi33Gfx11Bringup.cpp" \
  "$RESEARCH/tests/test_Navi33Gfx11Bringup.cpp" \
  -o "$BUILD_DIR/navi33_gfx11_bringup_tests"
"$BUILD_DIR/navi33_gfx11_bringup_tests"

printf '\n== Build IP Discovery parser bounds tests ==\n'
"$CXX" $COMMON -I"$BRINGUP/src" \
  "$BRINGUP/src/ipdiscovery.cpp" \
  "$RESEARCH/tests/test_ipdiscovery_bounds.cpp" \
  -o "$BUILD_DIR/ipdiscovery_bounds_tests"
"$BUILD_DIR/ipdiscovery_bounds_tests"

printf '\n== Run Navi 48 isolation guard ==\n'
python3 -B "$RESEARCH/tests/test_navi48_isolation.py"
python3 -B "$RESEARCH/driverkit-pci-probe/test_probe_contract.py"

printf '\nPASS: macOS host-side tests only. No hardware initialization was attempted.\n'
