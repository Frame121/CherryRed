# Navi33Survey: opt-in, read-only RX 7600 PCI survey kext scaffold.
# Requires macOS + Xcode Command Line Tools + a compatible MacKernelSDK.
# This target is NOT a GPU driver and does not enable acceleration.
PRODUCT := Navi33Survey
BUNDLE_ID := com.navi33.survey
ARCH ?= x86_64
DEPLOY ?= 11.0
SDK := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
MKSDK ?= ../../RDNA4FB/MacKernelSDK
BUILD := build
KEXT := $(BUILD)/$(PRODUCT).kext
EXEC := $(KEXT)/Contents/MacOS/$(PRODUCT)
CXX ?= clang++
COMMON := -arch $(ARCH) -target $(ARCH)-apple-macos$(DEPLOY) -isysroot $(SDK) \
  -I$(MKSDK)/Headers -mmacosx-version-min=$(DEPLOY) -DKERNEL -DKERNEL_PRIVATE \
  -DDRIVER_PRIVATE -DAPPLE -DNeXT -nostdinc -fno-builtin -fno-common \
  -fno-stack-protector -mkernel -fapple-kext -Wall -Wextra -Werror -Os -g
CXXFLAGS := $(COMMON) -std=c++17 -fno-exceptions -fno-rtti -fno-c++-static-destructors
LDFLAGS := -arch $(ARCH) -target $(ARCH)-apple-macos$(DEPLOY) -isysroot $(SDK) \
  -nostdlib -Xlinker -kext -L$(MKSDK)/Library/x86_64 -lkmodc++ -lkmod -Wl,-no_deduplicate

.PHONY: all preflight test clean
all: preflight $(KEXT)

preflight:
	@test -n "$(SDK)" || { echo 'ERROR: xcrun/macOS SDK unavailable. Build on a macOS host.'; exit 1; }
	@test -f "$(MKSDK)/Headers/IOKit/IOService.h" || { echo 'ERROR: MacKernelSDK headers missing; set MKSDK=/path/to/MacKernelSDK'; exit 1; }
	@test -f "$(MKSDK)/Library/x86_64/libkmodc++.a" || { echo 'ERROR: MacKernelSDK x86_64 libkmodc++.a missing'; exit 1; }
	@test -f "$(MKSDK)/Library/x86_64/libkmod.a" || { echo 'ERROR: MacKernelSDK x86_64 libkmod.a missing'; exit 1; }

$(BUILD)/Navi33Survey.o: Navi33Survey.cpp Navi33Survey.hpp | preflight
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c Navi33Survey.cpp -o $@

$(KEXT): $(BUILD)/Navi33Survey.o Info.plist
	@mkdir -p $(KEXT)/Contents/MacOS
	$(CXX) $(LDFLAGS) $(BUILD)/Navi33Survey.o -o $(EXEC)
	cp Info.plist $(KEXT)/Contents/Info.plist
	@echo 'Built read-only survey bundle only; it does not initialize RX 7600 or provide acceleration.'

# Static safety tests only; no macOS SDK or hardware required.
test:
	python3 -B tests/test_survey_safety.py

clean:
	rm -rf $(BUILD)
