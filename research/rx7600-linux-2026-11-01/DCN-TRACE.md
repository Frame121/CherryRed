# DCN display-engine trace observations

**Capture date:** 2026-11-01
**Trace duration:** 14.997 seconds
**Display engine:** DCN 3.2.1 / Display Core 3.2.392

## Display snapshot

- DP-1 connected and enabled at 1920x1080, approximately 180 Hz.
- Preferred timing: 179.998267 Hz; pixel clock 415.580 MHz.
- DP-2, DP-3 and HDMI-A-1 were disconnected at capture.
- Link status: Good; DPMS: On. Maximum bpc capability 8–16; current bpc 10.
- VRR capability reported unavailable (`vrr_capable=0`).
- DMCUB runtime version: `0x07003600`, loaded through PSP.

## Trace event counts

| Event | Count |
|---|---:|
| `atomic_check_begin` | 6,441 |
| `atomic_check_finish` | 6,441 |
| `atomic_commit_tail_begin` | 2,685 |
| `atomic_commit_tail_finish` | 2,685 |
| `drm_vblank_event` (CRTC 0) | 2,695 |
| DMUB high IRQ | 0 |
| DC clocks state | 0 |
| DC pipe state | 0 |

CRTC 0 sequence advanced from 572176 to 574875. Median interval was 5.555 ms, corresponding to approximately 180.018 Hz.

## Interpretation and caveats

The captured vblank cadence is consistent with the active 180 Hz display mode. One sequence discontinuity was observed, but this trace alone is insufficient to classify it as a missed frame. This was a live, non-modeset trace. It does not capture connector mode changes, link retraining, DMCUB command contents, AUX transactions, PHY state or raw hardware registers. Event counts are specific to this capture.

Raw traces and EDID data are retained locally and are not published because they may contain process metadata, kernel pointers, timing details or monitor-identifying information.
