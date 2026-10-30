#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/build/navi33-driverkit-pci-probe"
DRIVER_BUNDLE="$OUTPUT_DIR/Navi33PciProbe.dext"
SDK_PATH="$(xcrun --sdk driverkit --show-sdk-path)"
SDK_VERSION="$(xcrun --sdk driverkit --show-sdk-version)"
IIG="$(xcrun --find iig)"
CLANGXX="$(xcrun --sdk driverkit --find clang++)"
FRAMEWORKS="$SDK_PATH/System/DriverKit/System/Library/Frameworks"

mkdir -p "$OUTPUT_DIR/DriverKit" "$DRIVER_BUNDLE/Contents/MacOS"

"$IIG" \
    --def "$SCRIPT_DIR/Navi33PciProbe.iig" \
    --header "$OUTPUT_DIR/DriverKit/Navi33PciProbe.h" \
    --impl "$OUTPUT_DIR/Navi33PciProbe.iig.cpp" \
    --framework-name DriverKit \
    --deployment-target "$SDK_VERSION" \
    -- -x c++ -std=gnu++20 -fblocks -D__IIG=1 \
       -isysroot "$SDK_PATH" -F "$FRAMEWORKS"

"$CLANGXX" \
    -target "x86_64-apple-driverkit$SDK_VERSION" \
    -std=gnu++20 -fno-exceptions -fno-rtti \
    -Wall -Wextra -Werror \
    -isysroot "$SDK_PATH" -F "$FRAMEWORKS" \
    -I "$OUTPUT_DIR" -I "$OUTPUT_DIR/DriverKit" \
    -c "$SCRIPT_DIR/Navi33PciProbe.cpp" \
    -o "$OUTPUT_DIR/Navi33PciProbe.o"

"$CLANGXX" \
    -target "x86_64-apple-driverkit$SDK_VERSION" \
    -std=gnu++20 -fno-exceptions -fno-rtti \
    -Wall -Wextra -Werror \
    -isysroot "$SDK_PATH" -F "$FRAMEWORKS" \
    -I "$OUTPUT_DIR" -I "$OUTPUT_DIR/DriverKit" \
    -c "$OUTPUT_DIR/Navi33PciProbe.iig.cpp" \
    -o "$OUTPUT_DIR/Navi33PciProbe.iig.o"

cp "$SCRIPT_DIR/Info.plist" "$DRIVER_BUNDLE/Contents/Info.plist"

"$CLANGXX" \
    -target "x86_64-apple-driverkit$SDK_VERSION" \
    -bundle -std=gnu++20 \
    -isysroot "$SDK_PATH" -F "$FRAMEWORKS" \
    -framework DriverKit -framework PCIDriverKit \
    -o "$DRIVER_BUNDLE/Contents/MacOS/Navi33PciProbe" \
    "$OUTPUT_DIR/Navi33PciProbe.o" "$OUTPUT_DIR/Navi33PciProbe.iig.o"

plutil -lint "$SCRIPT_DIR/Info.plist"
plutil -lint "$DRIVER_BUNDLE/Contents/Info.plist"

BUILD_INFO="$(xcrun vtool -show-build "$DRIVER_BUNDLE/Contents/MacOS/Navi33PciProbe")"
printf '%s\n' "$BUILD_INFO"
printf '%s\n' "$BUILD_INFO" | grep -q 'platform DRIVERKIT'
file "$DRIVER_BUNDLE/Contents/MacOS/Navi33PciProbe"

printf 'PASS: unsigned x86_64 DriverKit bundle built with SDK %s.\n' "$SDK_VERSION"
printf 'Bundle: %s\n' "$DRIVER_BUNDLE"
printf 'Not signed, installed, activated, or run against the GPU.\n'
