# Build-only x86_64 kext prototype for macOS; does not install or load it.
ROOT := ../..
PRODUCT := Navi33Survey
ARCH := x86_64
DEPLOY := 11.0
SDK := $(shell xcrun --sdk macosx --show-sdk-path)
MKSDK ?= $(ROOT)/build/deps/MacKernelSDK
BUILD := $(ROOT)/build/navi33-survey
KEXT := $(BUILD)/$(PRODUCT).kext
EXEC := $(KEXT)/Contents/MacOS/$(PRODUCT)

CXX := xcrun --sdk macosx clang++
CC := xcrun --sdk macosx clang

COMMON := -arch $(ARCH) -target $(ARCH)-apple-macos$(DEPLOY) -isysroot $(SDK) \
  -I$(MKSDK)/Headers -I. -mmacosx-version-min=$(DEPLOY) \
  -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
  -D_FORTIFY_SOURCE=0 -nostdinc -fno-builtin -fno-common \
  -fno-stack-protector -mkernel -fapple-kext -Wall -Os -g
CXXFLAGS := $(COMMON) -std=c++17 -fno-exceptions -fno-rtti -fcheck-new \
  -fno-c++-static-destructors
CFLAGS := $(filter-out -fapple-kext,$(COMMON))
LDFLAGS := -arch $(ARCH) -target $(ARCH)-apple-macos$(DEPLOY) -isysroot $(SDK) \
  -nostdlib -Xlinker -kext -L$(MKSDK)/Library/universal \
  -lkmodc++ -lkmod -Wl,-no_deduplicate

OBJS := $(BUILD)/Navi33Survey.o $(BUILD)/kmod_info.o

.PHONY: all clean
all: $(KEXT)

$(BUILD)/Navi33Survey.o: Navi33Survey.cpp Navi33Survey.hpp Makefile
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kmod_info.o: kmod_info.c Makefile
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KEXT): $(OBJS) Info.plist Makefile
	@mkdir -p $(KEXT)/Contents/MacOS
	$(CXX) $(LDFLAGS) $(OBJS) -o $(EXEC)
	cp Info.plist $(KEXT)/Contents/Info.plist
	xattr -cr $(KEXT)
	codesign --force --sign - $(KEXT)
	codesign --verify --deep --strict $(KEXT)
	plutil -lint $(KEXT)/Contents/Info.plist
	@echo "Built $(KEXT) for $(ARCH); not installed, activated, or loaded."

clean:
	rm -rf $(BUILD)
