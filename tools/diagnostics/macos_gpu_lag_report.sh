#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)

# Read-only macOS graphics/WindowServer diagnostic for CherryRed.
# Writes only to a temporary report. No serials, UUIDs, full IORegistry dumps,
# settings changes, or kernel-extension load/unload operations.
OUT="${TMPDIR:-/tmp}/cherryred-gpu-lag-report.$$.txt"
{
  echo "CherryRed macOS graphics diagnostic"
  date
  echo
  echo "== macOS =="
  sw_vers
  uname -m
  echo
  echo "== Display and GPU =="
  system_profiler SPDisplaysDataType 2>/dev/null |
    grep -E 'Graphics/Displays:|Display:|Type:|Bus:|PCIe Lane Width:|VRAM|Vendor:|Device ID:|Revision ID:|Kernel Extension Info:|Resolution:|UI Looks like:|Framebuffer Depth:|Main Display:|Online:' || true
  echo
  echo "== GPU PCI and subsystem identity (IORegistry summary) =="
  ioreg -r -n GFX0 -l -w0 2>/dev/null |
    grep -E '"(vendor-id|device-id|revision-id|subsystem-vendor-id|subsystem-id|compatible)" =' || true
  echo
  echo "== Relevant loaded kernel extensions =="
  kmutil showloaded 2>/dev/null |
    grep -Ei 'AMDSupport|AMDRadeon|AMDFramebuffer|WhateverGreen|Lilu|Noot' || true
  echo
  echo "== Active IOAccelerator services (summary only) =="
  ioreg -r -c IOAccelerator -l 2>/dev/null |
    grep -E 'IOClass|IOProviderClass|IOName|CFBundleIdentifier|VRAM,totalMB|MetalPluginName|IOGLBundleName' |
    head -60 || true
  echo
  echo "== Metal device enumeration (public API) =="
  if command -v swift >/dev/null 2>&1; then
    swift "$ROOT/tools/diagnostics/macos_metal_probe.swift" 2>&1 ||
      echo "WARN: Metal probe failed; see Swift output above."
  else
    echo "SKIP: Swift is not available; Metal device enumeration was not run."
  fi
  echo
  echo "== WindowServer CPU sample =="
  ps -axo pid,pcpu,pmem,comm | awk 'NR == 1 || /WindowServer/'
  echo
  echo "== Current UI accessibility settings =="
  printf 'reduceMotion='
  defaults read com.apple.universalaccess reduceMotion 2>/dev/null || echo unknown
  printf 'reduceTransparency='
  defaults read com.apple.universalaccess reduceTransparency 2>/dev/null || echo unknown
  echo
  echo "== Recent WindowServer graphics warnings (last 5 minutes) =="
  log show --last 5m --style compact --predicate '(process == "WindowServer") AND (eventMessage CONTAINS[c] "GPU" OR eventMessage CONTAINS[c] "accelerat" OR eventMessage CONTAINS[c] "IOFB")' 2>/dev/null |
    tail -25 |
    sed -E 's/uuid:[[:xdigit:]-]{36}/uuid:[REDACTED]/g' || true
  echo
  echo "Diagnostic is read-only. Absence of a service does not identify every cause of lag."
} > "$OUT"
printf 'Report written to: %s\n' "$OUT"
cat "$OUT"
