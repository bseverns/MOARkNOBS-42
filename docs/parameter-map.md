# MOARkNOBS-42 — Parameter Map (v0.3.0-portfolio-2025)

> One row per control. Keep ranges explicit; note curves and targets.

| Control ID | Panel Label | Type   | Output (MIDI/OSC)         | Range / Resolution        | Curve (lin/exp/log) | Default | Target (DAW/Synth param) | Notes |
|------------|-------------|--------|----------------------------|---------------------------|---------------------|---------|---------------------------|-------|
| K01        | Cutoff      | Pot    | MIDI CC 074                | 0–127 (7-bit)             | lin                 | 64      | LPF cutoff                | Fine mode doubles resolution via 14-bit pair CC 74/106 |
| K02        | Resonance   | Pot    | /mn42/res float            | 0.00–1.00 (OSC float)     | exp (1.8)           | 0.25    | Filter Q                  | Anti-zipper deadband ±0.5% |
| E01        | Rate        | Encoder| MIDI CC 014 (14-bit MSB/LSB)| 0–16383                   | lin                 | 8192    | LFO rate                  | Accelerated step on fast turn |
| S01        | Shift       | Switch | MIDI NoteOn ch1 note 36    | 0/127                     | n/a                 | off     | Mode shift                | Latching |

**Global mapping notes**
- **Deadband:** ±0.5% on pots to prevent chatter; encoders rate-limit at ≤120 msgs/s.
- **Resolution:** Default MIDI 7-bit; 14-bit available where marked; OSC path mirrors 0.0–1.0.
- **Curves:** Log/exp curves indicated per control; see firmware `mapping_curves.h`.
- **Discoverability:** Send `/mn42/hello` on USB connect with firmware version and mapping hash.
