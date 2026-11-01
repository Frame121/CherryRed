# Builds build/<Config>/RedNavi.kext. Needs macOS + Xcode command-line tools and the
# MacKernelSDK (scripts/bootstrap.sh fetches it).
ifeq ($(DEBUG),1)
CONFIG := Debug
OPT := -O0 -g -DDEBUG
else
CONFIG := Release
OPT := -Os
endif

SDK := MacKernelSDK
NAVI33 ?= 0
ifeq ($(NAVI33),1)
TARGET_SUFFIX := -Navi33
TARGET_FLAGS := -DRN_BUILD_NAVI33=1
GFX_OBJ = $(OUT)/gfx11_navi33.o
PSP_FW_HEADER := RedNavi/psp_fw_navi33.h
SMU_FW_HEADER := RedNavi/smu_fw_navi33.h
GC_FW_HEADER := RedNavi/gc_fw_navi33.h
DMUB_FW_HEADER := RedNavi/dmub_fw_navi33.h
else ifeq ($(NAVI33),0)
TARGET_SUFFIX :=
TARGET_FLAGS :=
GFX_OBJ = $(OUT)/gfx11.o
PSP_FW_HEADER := RedNavi/psp_fw.h
SMU_FW_HEADER := RedNavi/smu_fw.h
GC_FW_HEADER := RedNavi/gc_fw.h
DMUB_FW_HEADER := RedNavi/dmub_fw.h
else
$(error NAVI33 must be 0 or 1)
endif
FIRMWARE_DEPS := $(wildcard $(PSP_FW_HEADER) $(SMU_FW_HEADER) $(GC_FW_HEADER) $(DMUB_FW_HEADER))
ifeq ($(NAVI33),1)
NAVI33_VERIFY_TARGET := navi33-verify-firmware
navi33-verify-firmware:
	python3 tools/verify_navi33_firmware_headers.py RedNavi
.PHONY: navi33-verify-firmware
NAVI33_REQUIRED_HEADERS := $(PSP_FW_HEADER) $(SMU_FW_HEADER) $(GC_FW_HEADER) $(DMUB_FW_HEADER)
NAVI33_MISSING_HEADERS := $(filter-out $(FIRMWARE_DEPS),$(NAVI33_REQUIRED_HEADERS))
ifneq ($(strip $(NAVI33_MISSING_HEADERS)),)
$(error Missing Navi33 firmware headers: $(NAVI33_MISSING_HEADERS). Run tools/build_navi33_firmware_headers.sh on the Linux system with linux-firmware-amdgpu installed, then transfer the ignored headers to the macOS build machine.)
endif
else
NAVI33_VERIFY_TARGET :=
endif
OUT := build/$(CONFIG)$(TARGET_SUFFIX)
KEXT := $(OUT)/RedNavi.kext
FBKEXT := $(OUT)/RedNaviFB.kext
CXX := clang++
ARCH := -arch x86_64
CXXFLAGS := $(ARCH) $(OPT) -std=c++17 -fno-rtti -fno-exceptions -fno-builtin -fno-common -mkernel \
	-nostdinc -isystem $(SDK)/Headers -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE \
	-DAPPLE -DNeXT -mmacosx-version-min=12.0 -Wall -Icompat/include $(TARGET_FLAGS)
# Firmware wrappers are generated locally from matching linux-firmware files; no vendor payload is checked in.
ifneq ($(wildcard $(PSP_FW_HEADER)),)
CXXFLAGS += -DRN_HAVE_PSP_FW
endif
ifneq ($(wildcard $(SMU_FW_HEADER)),)
CXXFLAGS += -DRN_HAVE_SMU_FW
endif
ifneq ($(wildcard $(GC_FW_HEADER)),)
CXXFLAGS += -DRN_HAVE_GC_FW
endif
ifneq ($(wildcard $(DMUB_FW_HEADER)),)
CXXFLAGS += -DRN_HAVE_DMUB_FW
endif
LDFLAGS := $(ARCH) -Xlinker -kext -nostdlib -lkmod -L$(SDK)/Library -lkmodc++ -lcc_kext \
	-Xlinker -object_path_lto -Xlinker /dev/null -mmacosx-version-min=12.0

ifeq ($(NAVI33),1)
# The RX7600 experimental target has no validated Navi33 scanout/framebuffer yet.
all: $(KEXT)
else
all: $(KEXT) $(FBKEXT)
endif

$(OUT)/kern_probe.o: RedNavi/kern_probe.cpp RedNavi/gpu_target.h RedNavi/gpu_ip_profile.h RedNavi/navi33_board.h RedNavi/gc_regs.h RedNavi/gc_regs_navi33.h $(FIRMWARE_DEPS) RedNavi/smu13.h compat/include/*/*.h
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/edid.o: RedNavi/edid.cpp RedNavi/edid.h
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/mode_regs.o: RedNavi/mode_regs.cpp RedNavi/mode_regs.h RedNavi/edid.h
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/gfx11.o: RedNavi/gfx11.cpp RedNavi/gfx11.h RedNavi/gc_regs.h RedNavi/gfx11_packets.h $(wildcard compat/include/*/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/gfx11_navi33.o: RedNavi/gfx11.cpp RedNavi/gfx11.h RedNavi/gc_regs_navi33.h RedNavi/gfx11_packets.h $(wildcard compat/include/*/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -DRN_GFX11_NAVI33=1 -c $< -o $@

$(OUT)/smu13.o: RedNavi/smu13.cpp RedNavi/smu13.h $(wildcard compat/include/*/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# LinuxKPI-style compat layer (see docs/COMPAT.md). Plain objects linked into RedNavi.kext.
COMPAT_SRC := $(wildcard compat/src/*.cpp)
COMPAT_OBJ := $(patsubst compat/src/%.cpp,$(OUT)/compat_%.o,$(COMPAT_SRC))

$(OUT)/compat_%.o: compat/src/%.cpp $(wildcard compat/include/*/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/kern_info.o: RedNavi/kern_info.c
	@mkdir -p $(OUT)
	clang $(ARCH) $(OPT) -mkernel -nostdinc -isystem $(SDK)/Headers -DKERNEL -mmacosx-version-min=12.0 -c $< -o $@

$(KEXT): $(OUT)/kern_probe.o $(OUT)/smu13.o $(GFX_OBJ) $(OUT)/edid.o $(OUT)/mode_regs.o $(COMPAT_OBJ) $(OUT)/kern_info.o RedNavi/Info.plist.in $(NAVI33_VERIFY_TARGET)
	@mkdir -p $(KEXT)/Contents/MacOS
	$(CXX) $(LDFLAGS) $(OUT)/kern_probe.o $(OUT)/smu13.o $(GFX_OBJ) $(OUT)/edid.o $(OUT)/mode_regs.o $(COMPAT_OBJ) $(OUT)/kern_info.o -o $(KEXT)/Contents/MacOS/RedNavi
	@if nm -u $(KEXT)/Contents/MacOS/RedNavi | grep -E "RedNavi|rn_|kRN"; then echo "unresolved RedNavi symbols in the kext"; exit 1; fi
	cp RedNavi/Info.plist.in $(KEXT)/Contents/Info.plist
	@echo "built $(KEXT)"

# The framebuffer is its own kext: IOGraphicsFamily is not in the boot kernel collection OpenCore injects into.
$(OUT)/fb_kern_fb.o: RedNaviFB/kern_fb.cpp
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/fb_kern_info.o: RedNaviFB/kern_info.c
	@mkdir -p $(OUT)
	clang $(ARCH) $(OPT) -mkernel -nostdinc -isystem $(SDK)/Headers -DKERNEL -mmacosx-version-min=12.0 -c $< -o $@

$(FBKEXT): $(OUT)/fb_kern_fb.o $(OUT)/fb_kern_info.o RedNaviFB/Info.plist.in
	@mkdir -p $(FBKEXT)/Contents/MacOS
	$(CXX) $(LDFLAGS) $(OUT)/fb_kern_fb.o $(OUT)/fb_kern_info.o -o $(FBKEXT)/Contents/MacOS/RedNaviFB
	cp RedNaviFB/Info.plist.in $(FBKEXT)/Contents/Info.plist
	@echo "built $(FBKEXT)"

clean:
	rm -rf build

# Host tests for the pure parts of the compat layer (they run on the Mac, not in the kernel).
test:
	@mkdir -p build
	clang++ -std=c++17 -Wall -DRN_HOST_TEST -Icompat/include tests/host/compat_test.cpp tests/host/host_hooks.cpp \
		compat/src/rn_slab.cpp compat/src/rn_firmware.cpp compat/src/rn_reg.cpp -o build/compat_test
	./build/compat_test
	clang++ -std=c++17 -Wall tests/host/edid_test.cpp RedNavi/edid.cpp -o build/edid_test
	./build/edid_test
	clang++ -std=c++17 -Wall tests/host/mode_regs_test.cpp RedNavi/mode_regs.cpp RedNavi/edid.cpp -o build/mode_regs_test
	./build/mode_regs_test
	clang++ -std=c++17 -Wall -IRedNavi tests/host/gpu_target_test.cpp -o build/gpu_target_test
	./build/gpu_target_test

.PHONY: all clean test

# ASIC/IP profile tests are standalone and can run on Linux without MacKernelSDK.
test-navi33-profile:
	@mkdir -p build
	clang++ -std=c++17 -Wall -IRedNavi tests/host/gpu_ip_profile_test.cpp -o build/gpu_ip_profile_test
	./build/gpu_ip_profile_test
	clang++ -std=c++17 -Wall -IRedNavi tests/host/gc_regs_navi33_test.cpp -o build/gc_regs_navi33_test
	./build/gc_regs_navi33_test
	clang++ -std=c++17 -Wall -IRedNavi tests/host/gfx11_packet_test.cpp -o build/gfx11_packet_test
	./build/gfx11_packet_test
	clang++ -std=c++17 -Wall -IRedNavi tests/host/gpu_target_test.cpp -o build/gpu_target_test
	./build/gpu_target_test
	@if [ -f RedNavi/psp_fw_navi33.h ] && [ -f RedNavi/smu_fw_navi33.h ] && [ -f RedNavi/gc_fw_navi33.h ] && [ -f RedNavi/dmub_fw_navi33.h ]; then python3 tools/verify_navi33_firmware_headers.py RedNavi; else echo "SKIP: local Navi33 firmware wrappers not present"; fi

.PHONY: test-navi33-profile

test: test-navi33-profile

# Generate ignored, RX7600-matched firmware wrappers from locally installed linux-firmware blobs.
firmware-navi33:
	bash tools/build_navi33_firmware_headers.sh

# Build the RX7600-specific kext variant; hardware write stages remain opt-in by boot-args.
navi33:
	$(MAKE) NAVI33=1 all

.PHONY: firmware-navi33 navi33
