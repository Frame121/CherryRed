#!/bin/sh
# Read-only macOS toolchain preflight for the Navi33 research track.
# This does not build/load a kext, change security settings, or access hardware.
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
cd "$ROOT"

fail=0
warn=0
pass() { printf '[PASS] %s\n' "$1"; }
missing() { printf '[MISSING] %s\n' "$1"; fail=$((fail + 1)); }
note() { printf '[NOTE] %s\n' "$1"; }
warning() { printf '[WARN] %s\n' "$1"; warn=$((warn + 1)); }

printf '%s\n' 'Cobalt64 / Navi33 macOS preflight (read-only)'
printf 'Repository: %s\n' "$ROOT"
printf 'Host: '
uname -s 2>/dev/null || printf 'unknown\n'

if [ "$(uname -s 2>/dev/null || true)" != Darwin ]; then
    missing "macOS host (this script can only verify the host OS here)"
    note "No files were built or changed by this preflight."
    printf 'Summary: %s missing prerequisite(s), %s warning(s).\n' "$fail" "$warn"
    exit 2
fi
pass "Running on macOS"

if command -v xcode-select >/dev/null 2>&1; then
    if xcode-select -p 2>/dev/null; then pass "xcode-select path"; else missing "Xcode Command Line Tools selection"; fi
else
    missing "xcode-select"
fi

if command -v xcrun >/dev/null 2>&1; then
    if xcrun --find clang++ >/dev/null 2>&1; then
        printf '[PASS] Apple clang++: '
        xcrun --find clang++
    else
        missing "Apple clang++ via xcrun"
    fi
    if SDK=$(xcrun --sdk macosx --show-sdk-path 2>/dev/null) && [ -d "$SDK" ]; then
        pass "macOS SDK: $SDK"
    else
        missing "macOS SDK (xcrun --sdk macosx --show-sdk-path)"
    fi
else
    missing "xcrun"
fi

for tool in make nm otool codesign; do
    if command -v "$tool" >/dev/null 2>&1; then pass "$tool available"; else missing "$tool"; fi
done

if [ -d src/navi33-research/include ]; then
    pass "portable Navi33 research sources present"
else
    missing "src/navi33-research/include"
fi

if [ -d src/navi33-survey ]; then
    pass "read-only survey scaffold present"
else
    missing "src/navi33-survey"
fi

if [ -d src/RDNA4FB/MacKernelSDK/Headers ] && [ -d src/RDNA4FB/MacKernelSDK/Library/x86_64 ]; then
    pass "MacKernelSDK headers and x86_64 libraries found at expected legacy path"
else
    warning "MacKernelSDK not found at src/RDNA4FB/MacKernelSDK; no kernel-extension build is authorized by this result"
fi

if [ -f src/navi33-survey/Info.plist ]; then
    if /usr/libexec/PlistBuddy -c 'Print :IOKitPersonalities:Navi33Survey:IOPCIPrimaryMatch' src/navi33-survey/Info.plist 2>/dev/null | grep -qi '0x74801002'; then
        pass "survey personality matches only the expected RX 7600 PCI ID"
    else
        warning "survey plist PCI match could not be confirmed"
    fi
fi

cat <<'EOF'

Safety boundary:
- PASS here means only that a prerequisite was found.
- This does not validate kernel ABI compatibility or build a kext.
- Do not load Navi33Survey.kext: it has not been compiled or tested on macOS.
- Do not disable SIP, lower kext security, alter OpenCore, or install anything
  as part of this preflight.
EOF
printf 'Summary: %s missing prerequisite(s), %s warning(s).\n' "$fail" "$warn"
[ "$fail" -eq 0 ] || exit 1
exit 0
