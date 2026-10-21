# RX 7600 Research Readiness and Reproducibility Audit

**Milestone:** M2.3 — readiness, reproducibility, and documentation audit  
**Audit date:** 2026-10-20  
**Scope:** Offline repository inspection and synthetic parser tests only.
This audit does not establish RX 7600 hardware or macOS compatibility.

## Git state

At the pre-edit check, Git reported:

- Branch: `main`
- HEAD: `d1f7bad8408a0a2c016b067c477422975a59de44`
- Upstream: `origin/main`
- HEAD and `origin/main` resolve to the same commit; merge base is also that
  commit; divergence count is `0 0`.
- No staged or tracked changes.
- Fourteen pre-existing untracked paths, all under `tools/ipdisc/`, including
  the two existing Python cache files. No paths outside that directory were
  reported.

After this report was created and documentation updated, the new report is
also untracked. The final status is recorded in `RESUME.md` and the M2.3
worklog entry.

## Files and commands inspected

Read `RESUME.md` first, then `WORKLOG.md`, `README.md`,
`RX7600_RESEARCH.md`, `RX7600_IP_FIRMWARE_MAP.md`, and
`RX7600_EVIDENCE_GAPS.md`. Also reviewed parser behavior notes,
`ipdiscovery.py`, `mkfixture.py`, and `test_ipdiscovery.py`.

Repository inspection was limited to Cobalt64. Git status, branch, HEAD,
upstream ref, merge base and divergence were checked. The previous M2.2
evidence inventory recorded only shader binaries and synthetic fixture input;
no physical device or unrelated user directory was accessed.

The prescribed test command was run from `tools/ipdisc/`:

```text
python -B test_ipdiscovery.py
```

The output listed 22 test methods, each `ok`, followed by:

```text
Ran 22 tests in 0.331s
OK
```

Exit code was 0: 22 passed, 0 failed, 0 skipped.

## Reproducibility and documentation review

- **VERIFIED:** The documented command runs from the documented directory
  without third-party dependencies, using the Python standard library.
- **VERIFIED:** Tests construct synthetic data through `FixtureGenerator` and
  use temporary files for CLI checks. No test probes hardware or uses an RX
  7600 dump as its oracle.
- **VERIFIED:** Existing tests assert structure, parser errors, compatibility
  expectations, deterministic fixture output and CLI results. No test claims
  that a parse result establishes RX 7600 or macOS support.
- **VERIFIED:** Strict mode is documented as the default structural validator.
  Compatibility mode preserves observed C++ parser behavior and is explicitly
  not validation for untrusted input. Binary header versions 0–2 and IP
  Discovery versions 1–4 with 32-bit bases are supported; v4 64-bit bases and
  unsupported formats are rejected.
- **VERIFIED:** Documentation distinguishes synthetic parser coverage and
  pinned Linux dispatch/firmware request paths from physical board evidence.
  No successful RX 7600 macOS initialization, build, or hardware test is
  established by evidence reviewed here.
- **VERIFIED:** Seven local Markdown links across the technical README,
  research report, IP/firmware map, evidence-gap report, this audit, worklog
  and recovery checkpoint resolved in the final check. External links in the
  research map are pinned source references; this audit did not re-fetch them.
- **Reproducibility issues found:** None that prevents reproducing the stated
  offline parser test command. This is not a claim of complete driver build or
  target reproducibility.
- **Minor state-reporting detail:** An initial `git rev-list` probe used two
  non-range tips and printed `0 17`, which was not a valid divergence result.
  It was not relied upon. Exact HEAD/upstream OIDs, merge base, status, and
  `git rev-list --left-right --count HEAD...refs/remotes/origin/main` were
  checked; they agree at identical refs and `0 0`.
- **Document update:** Linked this audit from `README.md`; no parser or
  implementation changes were made.

## Confirmed facts and unresolved questions

### Confirmed

- **VERIFIED:** The project-local parser tests run offline on generated
  synthetic data.
- **VERIFIED:** The RX 7600 reports in the project state that were read
  consistently say no trustworthy board-specific IP Discovery artifact is
  available.
- **VERIFIED:** The pinned Linux source provides version-based dispatch and
  conditional firmware request behavior; this describes Linux source paths,
  not measurements of the user's board.
- **VERIFIED:** Existing Cobalt64 hardware initialization and documented
  development path are Navi 48-specific. The parser tests do not exercise
  driver initialization.

### Unresolved

- The exact binary and IP Discovery table versions, die records, IP versions,
  instance numbers, harvest state and register bases for the user's physical
  RX 7600.
- The exact retail board, subsystem IDs, VBIOS, memory topology and display
  wiring associated with that device.
- Firmware versions/hashes, provenance, licensing, and any macOS loading
  constraints for firmware selected from real target IP records.
- macOS build, initialization, display, acceleration and Metal behavior.

No value is inferred from a synthetic fixture or a Linux version switch.
`gfx1102` remains a user-supplied label, not an independently verified
observation.

## Limitations and next gate

This audit validates only offline parser reproducibility and documentation.
It does not prove that a byte stream is authentic, belongs to an RX 7600 board,
is complete, or is compatible with Cobalt64 or macOS. It does not prove
successful macOS compilation, initialization, acceleration, or Metal support.

Before driver-source changes are considered, obtain an existing
provenance-documented artifact, establish board attribution independently,
parse it strictly without weakening the parser, record its hash and all
discovered records, cross-check each observed IP version against pinned source,
and resolve firmware provenance/licensing. Any later implementation requires
a separate explicit authorization, a separate Navi 33 initialization boundary,
specified files/tests/failure containment, and an isolated target recovery
plan. Do not add `1002:7480` to the Navi 48 match or initialization path.

**Single safest next action:** If a trusted, board-attributed RX 7600 IP
Discovery artifact already exists, submit it for offline strict parsing;
otherwise preserve the unresolved version mapping. No physical GPU probing is
authorized by this audit.
