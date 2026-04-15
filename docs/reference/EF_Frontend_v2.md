# EF Frontend v2

This document tracks the Envelope Follower controls exposed to frontends and patch tooling.

## Routing Model (`efSlots`)

Follower routing is represented at top-level as `efSlots` (one entry per follower):

- Preferred shape: `{"slots":[0,7,12]}` for multi-slot targets.
- Legacy shape: `{"slot":7}` for a single target (still accepted).

Frontends should normalize to `slots` arrays when staging edits. Firmware and runtime still read
legacy `slot` values for backwards compatibility.

## EF Modes

Each slot can select an EF mode at runtime. The firmware ships four modes:

- **Peak**: half-wave rectified input with RC-style attack/release smoothing.
- **RMS-ish**: leaky-integrator RMS estimate for sustained energy.
- **Gate**: thresholded output with hysteresis.
- **Follower**: fast attack/release envelope with minimal extra shaping.

Mode settings are stored per slot and persist in EEPROM with the rest of the slot configuration.

### Mode Settings (per slot)

Use these fields inside the slot `ef` object when sending config blobs or patches:

- `mode`: `PEAK`, `RMS`, `GATE`, or `FOLLOWER` (or numeric enum 0-3).
- `attack_ms` / `release_ms`: attack/release timing for Peak/Follower.
- `rms_ms`: RMS window length for RMS-ish.
- `gate_threshold` / `gate_hysteresis`: gate thresholds in 0-127.
- `activity_threshold`: baseline/gain activity threshold in 0-127.
- `auto_baseline`: boolean; enable idle baseline tracking.
- `auto_gain`: boolean; enable auto gain.
- `baseline_tau_ms` / `gain_tau_ms`: time constants for auto baseline/gain.
- `gain_target`: auto-gain target in 0-127 (default ~80% FS).

### Example Payload

```json
{
  "slots": [
    {
      "ef": {
        "mode": "RMS",
        "rms_ms": 60,
        "auto_baseline": true,
        "auto_gain": true,
        "gain_target": 102
      }
    }
  ]
}
```
