# Standalone diagnostic kext; no Navi48 code or firmware is linked.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
PRODUCT := Navi33Probe
BUILD := build
MKSDK ?= ../RDNA4FB/MacKernelSDK
SDK = $(shell xcrun --sdk macosx --show-sdk-path)
CXX := clang++
CC := clang
KEXT := $(BUILD)/$(PRODUCT).kext
EXEC := $(KEXT)/Contents/MacOS/$(PRODUCT)
OBJS := $(BUILD)/Navi33Probe.o $(BUILD)/kmod_info.o

COMMON = -arch x86_64 -target x86_64-apple-macos11.0 -isysroot "$(SDK)" \
    -I"$(MKSDK)/Headers" -Isrc -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE \
    -DAPPLE -DNeXT -D_FORTIFY_SOURCE=0 -nostdinc -fno-builtin -fno-common \
    -fno-stack-protector -mkernel -Wall -Wextra -Os -g
CXXFLAGS = $(COMMON) -std=c++17 -fapple-kext -fno-exceptions -fno-rtti -fcheck-new \
    -fno-c++-static-destructors
CFLAGS = $(COMMON)
LDFLAGS = -arch x86_64 -target x86_64-apple-macos11.0 -isysroot "$(SDK)" \
    -nostdlib -Xlinker -kext -L"$(MKSDK)/Library/x86_64" -lkmod \
    -Wl,-no_deduplicate

.PHONY: all check-env clean
all: $(EXEC)

check-env:
	@test "$$(uname -s)" = Darwin || { echo "error: macOS with Xcode Command Line Tools is required"; exit 1; }
	@xcrun --sdk macosx --show-sdk-path >/dev/null
	@test -f "$(MKSDK)/Headers/IOKit/IOService.h" || { echo "error: set MKSDK to MacKernelSDK (see README.md)"; exit 1; }
	@test -d "$(MKSDK)/Library/x86_64" || { echo "error: MacKernelSDK x86_64 libraries missing"; exit 1; }

$(OBJS): Makefile | check-env

$(BUILD)/%.o: src/%.cpp
	@mkdir -p "$(BUILD)"
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: src/%.c
	@mkdir -p "$(BUILD)"
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(EXEC): $(OBJS) Info.plist Makefile | check-env
	@mkdir -p "$(KEXT)/Contents/MacOS"
	$(CXX) $(LDFLAGS) $(OBJS) -o $@
	cp Info.plist "$(KEXT)/Contents/Info.plist"
	plutil -lint "$(KEXT)/Contents/Info.plist"
	codesign --force --sign - "$(KEXT)"
	codesign --verify "$(KEXT)"

-include $(OBJS:.o=.d)

clean:
	rm -rf "$(BUILD)"
