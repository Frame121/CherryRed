# RX 7600 / Navi 33 IP Dispatch Specification Review

**Review date:** 2026-10-21
**Reviewed document:** `RX7600_NAVI33_IP_DISPATCH_SPEC.md` (review only; it was
not edited)
**Reference revision:** `torvalds/linux@d0c647a6aae2ce577372594a176c82733e1ed267`

## [Correct]

- **Pinned revision is reproducible.** The spec identifies the full Linux
  commit SHA and links to that exact commit (spec lines 5, 75-77). The reviewed
  links use the full SHA rather than a floating branch. This review freshly
  retrieved pinned `amdgpu_discovery.c`, `psp_v13_0_4.c`, `amdgpu_psp.c`, and
  `gfx_v11_0.c` from raw URLs at that SHA. The PCI name reference is also
  commit-pinned.
- **Board attribution is clearly disclaimed.** Spec lines 10-23 and 27-28
  say the table is conditional, that reported versions are unknown, and that
  generic dispatch is not a `1002:7480` mapping. This cannot reasonably be
  read as a confirmed RX 7600 inventory.
- **Main dispatch examples checked against the pinned source are conditional
  Linux behavior.** `amdgpu_discovery.c` dispatches on reported IP versions:
  GC v11 and MES v11 cases are in
  [`amdgpu_discovery.c`](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2434-L2480)
  and [MES dispatch](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2671-L2687).
  UVD 4.0.3 dispatches to VCN/JPEG 4.0.3 in
  [the multimedia switch](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2600-L2668).
  These are version branches, not proof those versions occur on the subject
  board.
- **PSP 13.0.4 example is conditional and source-consistent.** The generic
  discovery switch associates MP0 13.0.4 with the specialized PSP block
  ([`amdgpu_discovery.c`, lines 2218-2286](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/amdgpu_discovery.c#L2218-L2286)).
  `psp_v13_0_4_init_microcode()` decodes MP0 and, for 13.0.4, calls the TOC
  helper and then the TA helper, returning on errors
  ([`psp_v13_0_4.c`, lines 31-50](https://github.com/torvalds/linux/blob/d0c647a6aae2ce577372594a176c82733e1ed267/drivers/gpu/drm/amd/amdgpu/psp_v13_0_4.c#L31-L50)).
  This establishes source behavior only, not RX 7600 firmware selection.

## [Needs citation]

- **Several rows need precise line links.** The spec's evidence table links
  `amdgpu_discovery.c` without line fragments for most GC/GMC/IH/PSP/SMU/SDMA
  dispatch claims (spec lines 83-90). Add links to the exact switches, e.g.
  common/GMC 2054-2166, IH 2171-2215, PSP 2218-2286, SMU 2289-2352, GC
  2434-2480, SDMA 2494-2544, and multimedia 2600-2668. These ranges are
  documented in the existing
  [`RX7600_REMAINING_SOURCE_CLAIMS.md`](RX7600_REMAINING_SOURCE_CLAIMS.md),
  but direct links in the spec would improve independent auditability.
- **IH 6.1.x detail needs a direct case citation.** The spec states OSSSYS
  6.1.0-6.1.1 selects IH v6.1 (line 34), but the corresponding evidence row
  names only the IH v6.0 source file (line 86), and the existing narrow source
  report records the 6.0.0-6.0.2 cases rather than the 6.1.x range. Link the
  exact `amdgpu_discovery.c` switch range and `ih_v6_1.c` implementation or
  narrow the claim to the already-cited cases.
- **PSP request helper details need their direct shared-helper citations.**
  The PSP firmware table says the helpers make required requests and TA
  naming may vary with kicker mode (spec line 58), but its evidence row cites
  `amdgpu_psp.c` lines 3516-3524 only (spec line 87), the TOC helper. The
  TA helper is separately located around lines 3967-3980 in this pinned file
  according to the existing source-claim report. Link both exact helper
  implementations and distinguish firmware declarations from actual request
  call paths.
- **GFX firmware qualifiers need exact per-component citations.** The row
  mentions SR-IOV, required requests, RLC, kicker, and backdoor-auto TOC
  behavior (spec line 56), while the evidence link gives the broad range
  `gfx_v11_0.c` 682-800 (spec line 84). Add function-level fragments for each
  relevant branch; do not imply RLC is requested on the SR-IOV VF path.

## [Ambiguous]

- **Firmware evidence category is not explicit enough for a future capture.**
  The spec correctly labels its rows source-conditional, but should explicitly
  distinguish (a) a request observed in this board's logs, (b) firmware
  reported loaded/running, (c) a request expected from source conditions, and
  (d) unverified. No row in this document is category (a) or (b); the examples
  are category (c), conditional on observed IP versions and code path.
- **Licensing paragraph mixes file-level notices with kernel/module policy.**
  `amdgpu_discovery.c`, `psp_v13_0_4.c`, `amdgpu_psp.c`, and `gfx_v11_0.c`
  have permissive AMD file headers in the pinned contents. That is distinct
  from the Linux kernel's module-license/exported-symbol rules and any
  GPL-only kernel interfaces. Neither source-file MIT notices nor the local
  NOTICE's GPL reference-only statement determines which kernel symbols an
  out-of-tree module may use. Keep Linux code reference-only as instructed,
  and make no module-link compatibility inference.
- **Firmware licenses and the spec's own license are separate questions.**
  The spec correctly says firmware blob terms are unknown (spec lines 65-71).
  The repository root `LICENSE` is MIT, while the Navi48 subcomponent carries
  a separate BSD-3 license. The new spec does not state its own licensing
  scope. Do not assume source-code MIT headers license firmware blobs or
  settle whether this document is governed by the root license.
- **“Requests selected instance firmware” could be read as observed behavior.**
  The SDMA and VCN rows (spec lines 57 and 60) describe source logic, not
  requests observed on a board. Keep “expected/inferred from source” explicit
  in all later evidence tables; a missing log is not proof of no request.

## [Blocker]

- **No board-attributed evidence exists.** The spec itself says no
  provenance-documented IP Discovery artifact, subsystem/VBIOS attribution,
  or physical-card report was available (lines 18-23 and 100-112). Thus every
  version and firmware example remains conditional and unverified for
  `1002:7480`; Phase 4 dispatch implementation is not justified.
- **This Windows host cannot perform the requested Linux hardware check.**
  `lspci`, `dmesg`, and `/sys` capture must not be simulated or inferred from
  missing Windows tools. The device's absence is not established. Phase 3
  requires an authorized, suitable Linux host with the target present and
  read-only capture access, or an already-existing provenance-documented
  capture supplied for offline review.

No changes to the dispatch spec, driver code, or tests were made during this
review. The review report is not a substitute for target evidence or a
firmware/license determination.
