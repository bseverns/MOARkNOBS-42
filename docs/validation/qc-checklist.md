# MOARkNOBS-42 — QC & Demo Checklist (v0.3.0-portfolio-2025)

## Pre-power

- [ ] Visual inspect: solder joints clean; no bridges; connectors keyed.
- [ ] Panel: knob pointers aligned; tactile index markers present on key knobs.
- [ ] Ground: star ground verified; analog/digital return separation per layout notes.

## Power-on (bench)

- [ ] Firmware shows BUILD/FW version on serial: v0.3.0-portfolio-2025.
- [ ] Idle noise: ADC std-dev ≤ **[2 LSB]**; no cross-talk > **[-60 dB]** (log attached).
- [ ] Latency: knob→MIDI median **[3–5 ms]**, 95th **[≤10 ms]** (log attached).

## Messaging

- [ ] Single pot sweep emits CCs monotonic; no wrap; endpoints clamp as spec.
- [ ] Encoder at max spin: rate ≤120 msgs/s; host remains responsive.
- [ ] 14-bit pair recognized by DAW (where applicable).
- [ ] OSC path mirrors values; `/mn42/hello` reports mapping hash.

## Mapping & Persistence

- [ ] Remap one control; save; power-pull; mapping persists.
- [ ] Corrupt write simulation → device rolls back to last-known-good.

## Safety & Accessibility

- [ ] No exposed mains; enclosure grounded; strain relief present.
- [ ] High-contrast labels legible at 1 m; tactile index confirmed.

## Sign-off

Tech: **\_\_** Date: **\_\_** Unit ID: **\_\_** Logs: latency.csv, noise.csv
