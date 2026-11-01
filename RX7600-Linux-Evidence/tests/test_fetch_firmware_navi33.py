#!/usr/bin/env python
"""Host test for tools/fetch-firmware-navi33.sh.

Runs on the host (no macOS, no linux-firmware checkout). Verifies:
  - the script's expected blob set matches the evidence manifest exactly,
  - the --dry-run mode runs cleanly and lists exactly those blobs.
This does NOT copy firmware or establish macOS loadability.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "fetch-firmware-navi33.sh"
MANIFEST = ROOT / "RX7600-Linux-Evidence" / "navi33-firmware-manifest.json"


def manifest_blobs():
    m = json.loads(MANIFEST.read_text(encoding="utf-8"))
    return {Path(f["path"]).name for f in m["firmware"] if f.get("required")}


def script_blobs():
    text = SCRIPT.read_text(encoding="utf-8")
    # The BLOBS heredoc: lines of "<name>  <pin>" between the opening and closing quotes.
    m = re.search(r'BLOBS="\n(.*?)\n"', text, re.S)
    if not m:
        raise AssertionError("BLOBS block not found in script")
    blobs = set()
    for line in m.group(1).splitlines():
        parts = line.split()
        if parts:
            blobs.add(parts[0] + ".bin")
    return blobs


def run_dry_run():
    # bash is required; on hosts without it, skip the subprocess check.
    for shell in ("bash", "/bin/bash"):
        try:
            r = subprocess.run([shell, str(SCRIPT), "--dry-run"],
                               capture_output=True, text=True, timeout=30)
            break
        except FileNotFoundError:
            continue
    else:
        return None
    return r


def main():
    failures = []

    if not SCRIPT.exists():
        print("FAIL: script missing:", SCRIPT)
        sys.exit(1)

    mb = manifest_blobs()
    sb = script_blobs()

    if mb != sb:
        failures.append(
            "blob set mismatch: missing=%s extra=%s"
            % (sorted(mb - sb), sorted(sb - mb))
        )

    # No Navi 48 / RDNA 4 blobs in the Navi 33 fetch set.
    for b in sb:
        if "12_0_1" in b or "14_0_3" in b or "sdma_7_0_1" in b:
            failures.append("Navi 48 blob in Navi 33 fetch set: %s" % b)

    # Dry-run runs cleanly and lists the expected blobs.
    r = run_dry_run()
    if r is not None:
        if r.returncode != 0:
            failures.append("dry-run exited %d: %s" % (r.returncode, r.stderr.strip()))
        else:
            for b in sb:
                if b not in r.stdout:
                    failures.append("dry-run output missing blob: %s" % b)
        print("dry-run rc=%d" % r.returncode)
    else:
        print("note: bash not available; skipped dry-run subprocess check")

    if failures:
        print("FETCH-NAVI33 TEST: FAIL")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print("FETCH-NAVI33 TEST: PASS (%d blobs match manifest)" % len(mb))


if __name__ == "__main__":
    main()
