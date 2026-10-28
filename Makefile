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
OUT := build/$(CONFIG)
KEXT := $(OUT)/RedNavi.kext
CXX := clang++
ARCH := -arch x86_64
CXXFLAGS := $(ARCH) $(OPT) -std=c++17 -fno-rtti -fno-exceptions -fno-builtin -fno-common -mkernel \
	-nostdinc -isystem $(SDK)/Headers -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE \
	-DAPPLE -DNeXT -mmacosx-version-min=12.0 -Wall
# Optional local firmware component (python3 tools/gen_psp_fw_header.py ... RedNavi/psp_fw.h).
ifneq ($(wildcard RedNavi/psp_fw.h),)
CXXFLAGS += -DRN_HAVE_PSP_FW
endif
LDFLAGS := $(ARCH) -Xlinker -kext -nostdlib -lkmod -L$(SDK)/Library -lkmodc++ -lcc_kext \
	-Xlinker -object_path_lto -Xlinker /dev/null -mmacosx-version-min=12.0

all: $(KEXT)

$(OUT)/kern_probe.o: RedNavi/kern_probe.cpp $(wildcard RedNavi/psp_fw.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/kern_info.o: RedNavi/kern_info.c
	@mkdir -p $(OUT)
	clang $(ARCH) $(OPT) -mkernel -nostdinc -isystem $(SDK)/Headers -DKERNEL -mmacosx-version-min=12.0 -c $< -o $@

$(KEXT): $(OUT)/kern_probe.o $(OUT)/kern_info.o RedNavi/Info.plist.in
	@mkdir -p $(KEXT)/Contents/MacOS
	$(CXX) $(LDFLAGS) $(OUT)/kern_probe.o $(OUT)/kern_info.o -o $(KEXT)/Contents/MacOS/RedNavi
	cp RedNavi/Info.plist.in $(KEXT)/Contents/Info.plist
	@echo "built $(KEXT)"

clean:
	rm -rf build

.PHONY: all clean
