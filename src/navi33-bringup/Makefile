# Makefile — cross-compile Navi33Bringup.kext (RX 7600) for x86_64 from any host.
# Reuses RDNA4FB's MacKernelSDK (same as the Navi 48 kext). Targets macOS 11 ABI.
#   make          build build/Navi33Bringup.kext
#   make test     run the host static-safety test (no macOS SDK / hardware needed)
#   make clean
#
# This is a FAIL-CLOSED SCAFFOLD: read-only PCI discovery, no register writes,
# no firmware load. It embeds the RX 7600 firmware blobs (from a local
# linux-firmware checkout via tools/fetch-firmware-navi33.sh) but does NOT load
# them at this stage. See BUILDING.md and RX7600-Linux-Evidence/.

PRODUCT   := Navi33Bringup
BUNDLE_ID := com.navi33.bringup
ARCH      := x86_64
DEPLOY    := 11.0
SDK       := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
MKSDK     ?= ../../RDNA4FB/MacKernelSDK

BUILD := build
KEXT  := $(BUILD)/$(PRODUCT).kext
EXEC  := $(KEXT)/Contents/MacOS/$(PRODUCT)

# The C++ kmod runtime ships in Library/x86_64 in the RDNA4FB-bundled SDK and
# in Library/universal in acidanthera/MacKernelSDK; accept either layout.
KMODCPP := $(firstword $(wildcard $(MKSDK)/Library/x86_64/libkmodc++.a \
                             $(MKSDK)/Library/universal/libkmodc++.a))

CXX := clang++
CC  := clang

EMBED := ../navi48-bringup/tools/embed-firmware.py

CXX_SRCS := Navi33Bringup.cpp Navi33PspSurvey.cpp Navi33PspLoad.cpp Navi33Accelerator.cpp

# Firmware blobs (firmware/*.bin, gitignored) are embedded as C arrays generated at
# build time into src/fw/ (gitignored) by the shared embed tool: symbol fw_<blob>[] +
# fw_<blob>_size. The blob set is the RX 7600's own (see navi33-firmware-manifest.json).
FW_BINS  := $(wildcard firmware/*.bin)
FW_SRCS  := $(patsubst firmware/%.bin,src/fw/fw_%.c,$(FW_BINS))
# REQUIRED blobs for the RX 7600 (GC 11.0.2 / SDMA 6.0.2 / VCN 4.0.4 / DCN 3.2.1 dmcub /
# PSP 13.0.7 / SMU 13.0.7). The kext cannot link without them, so fail early.
FW_REQUIRED := gc_11_0_2_imu gc_11_0_2_me gc_11_0_2_mec gc_11_0_2_mes gc_11_0_2_mes1 \
               gc_11_0_2_mes_2 gc_11_0_2_pfp gc_11_0_2_rlc \
               sdma_6_0_2 vcn_4_0_4 dcn_3_2_1_dmcub \
               psp_13_0_7_sos psp_13_0_7_ta smu_13_0_7
FW_MISSING  := $(filter-out $(patsubst firmware/%.bin,%,$(FW_BINS)),$(FW_REQUIRED))

C_SRCS   := $(FW_SRCS)

src/fw/fw_%.c: firmware/%.bin $(EMBED)
	@mkdir -p src/fw
	python3 $(EMBED) $< $@ fw_$*

OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(CXX_SRCS)) $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS))

COMMON := -arch $(ARCH) -target $(ARCH)-apple-macos$(DEPLOY) -isysroot $(SDK) \
  -I$(MKSDK)/Headers -I. -I../navi33-research/include -mmacosx-version-min=$(DEPLOY) \
  -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
  -D_FORTIFY_SOURCE=0 -nostdinc -fno-builtin -fno-common -fno-stack-protector \
  -mkernel -fapple-kext -Wall -Wextra -Werror -Os -g
CXXFLAGS := $(COMMON) -std=c++17 -fno-exceptions -fno-rtti -fcheck-new -fno-c++-static-destructors
CFLAGS   := $(COMMON)
LDFLAGS  := -arch $(ARCH) -target $(ARCH)-apple-macos$(DEPLOY) -isysroot $(SDK) \
  -nostdlib -Xlinker -kext -L$(MKSDK)/Library/x86_64 -L$(MKSDK)/Library/universal \
  -lkmodc++ -lkmod -Wl,-no_deduplicate

.PHONY: all preflight test clean
all: preflight $(KEXT)

preflight:
	@test -n "$(SDK)" || { echo 'ERROR: xcrun/macOS SDK unavailable. Build on a macOS host.'; exit 1; }
	@test -f "$(MKSDK)/Headers/IOKit/IOService.h" || { echo 'ERROR: MacKernelSDK headers missing; set MKSDK=/path/to/MacKernelSDK'; exit 1; }
	@test -n "$(KMODCPP)" || { echo 'ERROR: MacKernelSDK libkmodc++.a missing (looked in Library/x86_64 and Library/universal)'; exit 1; }
	@test -f "$(MKSDK)/Library/x86_64/libkmod.a" || { echo 'ERROR: MacKernelSDK x86_64 libkmod.a missing'; exit 1; }

$(OBJS): | check-firmware
check-firmware:
	@if [ -n "$(FW_MISSING)" ]; then \
	  echo "error: missing required firmware in firmware/: $(addsuffix .bin,$(FW_MISSING))"; \
	  echo "       copy them from a linux-firmware checkout: tools/fetch-firmware-navi33.sh <path>"; \
	  echo "       (blob set: RX7600-Linux-Evidence/navi33-firmware-manifest.json)"; exit 1; fi
.PHONY: check-firmware

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
-include $(OBJS:.o=.d)

$(KEXT): $(OBJS) Info.plist
	@mkdir -p $(KEXT)/Contents/MacOS
	$(CXX) $(LDFLAGS) $(OBJS) -o $(EXEC)
	@# Catch our own undefined symbols at build time (they would make the kext fail to
	@# load silently in the kernel linker).
	@bad=$$(nm -u -arch $(ARCH) $(EXEC) | c++filt | grep -E 'Navi33|navi33' || true); \
	if [ -n "$$bad" ]; then \
	  echo "LINK ERROR: these symbols are OURS but undefined — the kext will not load:"; \
	  echo "$$bad" | sed 's/^/    /'; \
	  rm -f $(EXEC); exit 1; \
	fi
	cp Info.plist $(KEXT)/Contents/Info.plist
	find $(KEXT) \( -name '.DS_Store' -o -name '._*' \) -delete
	-xattr -d com.apple.FinderInfo $(KEXT) 2>/dev/null
	xattr -cr $(KEXT)
	codesign --force --sign - $(KEXT) || { xattr -d com.apple.FinderInfo $(KEXT) 2>/dev/null; xattr -cr $(KEXT); codesign --force --sign - $(KEXT); }
	@test -f $(KEXT)/Contents/_CodeSignature/CodeResources || { echo 'codesign produced no CodeResources'; exit 1; }
	@echo "Built $(KEXT) for $(ARCH) (min macOS $(DEPLOY)) — bring-up SCAFFOLD: read-only, no writes, no firmware load."

# Host static-safety test: no macOS SDK or hardware required.
test:
	python3 -B tests/test_bringup_safety.py

clean:
	rm -rf $(BUILD)
	rm -f src/fw/fw_*.c
.PHONY: all preflight test clean
