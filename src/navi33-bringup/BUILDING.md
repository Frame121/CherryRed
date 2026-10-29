# Building Navi33Bringup.kext (RX 7600 / Navi 33)

> **WARNING.** This is a **fail-closed bring-up SCAFFOLD**, not a working GPU driver. At
> this stage it performs read-only PCI discovery only: it does **not** map a register BAR,
> write any register, load firmware, or provide display/acceleration/Metal. The staged
> hardware bring-up (firmware → memory → interrupts → command → DCN 3.2.1 display → Metal)
> is future, evidence-gated work (see `RX7600-Linux-Evidence/` and the Mac-day runbook).
> Do not load experimental kernel extensions on a machine you cannot afford to restart.

## What this target is

- A separate kext from the Navi 48 bring-up. It matches **only** the RX 7600
  (`1002:7480`, subsystem `148c:2421`) and is opt-in (`navi33bringup=1`).
- It never references the Navi 48 PCI IDs (`1002:7550` / `1002:7551`).
- It embeds the RX 7600 firmware blobs (from a local linux-firmware checkout) but does not
  load them at this stage.

## Prerequisites (macOS host)

- macOS with Xcode Command Line Tools (`clang`, `clang++`, `make`, `codesign`, `python3`).
- MacKernelSDK from the RDNA4FB clone (same as the Navi 48 kext; not vendored here):

  ```
  git clone https://github.com/somestupidgirl/RDNA4FB
  ```

- AMD firmware for the RX 7600 from linux-firmware (AMD's own licence; not in this repo):

  ```
  git clone https://gitlab.com/kernel-firmware/linux-firmware.git
  tools/fetch-firmware-navi33.sh <path-to-that-checkout>
  ```

  This copies the 14 blobs listed in `RX7600-Linux-Evidence/navi33-firmware-manifest.json`
  (GC 11.0.2, SDMA 6.0.2, VCN 4.0.4, DCN 3.2.1 dmcub, PSP 13.0.7, SMU 13.0.7) into
  `src/navi33-bringup/firmware/` (gitignored). `tools/fetch-firmware-navi33.sh --dry-run`
  lists the set with no checkout required.

## Build

```
make -C src/navi33-bringup MKSDK=/path/to/RDNA4FB/MacKernelSDK all
```

The result is `src/navi33-bringup/build/Navi33Bringup.kext` (x86_64, ad-hoc signed, min
macOS 11 ABI). The Makefile fails early if any required firmware blob is missing, checks for
our own undefined symbols at link time, and ad-hoc signs the bundle.

## Host static-safety test (no macOS SDK or hardware needed)

```
python -B src/navi33-bringup/tests/test_bringup_safety.py
```

This is a static source/plist check, not a compiled or runtime IOKit test.

## Not a claim of support

Building or loading this scaffold does not mean the RX 7600 is initialized, accelerated, or
supported on macOS. "Works on macOS" is reserved for reproducible target-hardware evidence.
