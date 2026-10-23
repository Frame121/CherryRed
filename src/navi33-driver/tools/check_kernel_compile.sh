#!/usr/bin/env bash
# Compile real production sources against MacKernelSDK, without linking/loading.
set -euo pipefail
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: bash check_kernel_compile.sh /path/to/MacKernelSDK [output-directory]" >&2
    exit 2
fi
sdk_path="$(cd "$1" && pwd)"
component_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
compile_output="${2:-$component_root/build/kernel-objects}"
test -f "$sdk_path/Headers/IOKit/IOService.h"
mkdir -p "$compile_output"
common=(-target x86_64-apple-macos11.0 -nostdinc -isystem "$sdk_path/Headers"
    -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT
    -D_FORTIFY_SOURCE=0 -fno-builtin -fno-common -fno-stack-protector
    -mkernel -Wall -Wextra -Os)
cpp=(-std=c++17 -fapple-kext -fno-exceptions -fno-rtti -fcheck-new -fno-c++-static-destructors)
for source in Firmware VramArena Preparation Discovery; do
    "${CXX:-clang++}" "${common[@]}" "${cpp[@]}" -Werror \
        -I"$component_root/include" -c "$component_root/src/$source.cpp" \
        -o "$compile_output/$source.o"
done
"${CXX:-clang++}" "${common[@]}" "${cpp[@]}" \
    -c "$component_root/../navi33-probe/src/Navi33Probe.cpp" -o "$compile_output/Navi33Probe.o"
"${CC:-clang}" "${common[@]}" \
    -c "$component_root/../navi33-probe/src/kmod_info.c" -o "$compile_output/kmod_info.o"
"${CXX:-clang++}" "${common[@]}" "${cpp[@]}" \
    -c "$component_root/../navi33-survey/Navi33Survey.cpp" -o "$compile_output/Navi33Survey.o"
"${CC:-clang}" "${common[@]}" \
    -c "$component_root/../navi33-survey/kmod_info.c" -o "$compile_output/survey_kmod_info.o"
echo "Kernel-header compilation passed (8 objects); link, signing, and hardware remain separate checks."
