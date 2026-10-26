#!/bin/sh
# Fetch the MacKernelSDK (kernel headers and kext libraries for building on any macOS version).
set -e
if [ ! -d MacKernelSDK ]; then
    git clone --depth 1 https://github.com/acidanthera/MacKernelSDK.git
fi
echo "MacKernelSDK ready"
