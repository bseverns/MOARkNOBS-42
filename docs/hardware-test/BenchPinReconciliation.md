# Bench Pin Reconciliation — 2026-08-13 Board

## Confirmed physical signal map

| Function | Board evidence | Firmware contract |
|---|---|---|
| matrix row common | `MRR` → Teensy 22/A8 | output, held low while the addressed row is scanned |
| matrix/control common | `MRC` → Teensy 7 | digital input; 5 µs settle after mux address changes |
| matrix | seven `ROW` nets × six `COL` nets | active-low button inputs |
| direct controls | `CTRL1`–`CTRL6`, mux channels 6–11 | active-high inputs |
| OLED | Teensy 18/19 `SDA`/`SCL` | I²C only |
| envelope | `E1`–`E6` | A0–A3, A6–A7 |

## Corrected discrepancies

- Firmware no longer reads A4/A5 (OLED I²C) as mux ADC inputs.
- Firmware no longer samples A8/MRR as VREF.
- Matrix scanning drives pin 22 and reads pin 7 digitally; direct controls use the same pin-7 mux input with their actual active-high polarity.
- The 42 virtual slot values remain protocol/configuration state, but their unverified physical ADC scan is disabled.
- `MAIN`, `FP1`, and `FP2` are disabled rather than read through pin 7. Their MRC route includes R95 (220 Ω) and C77 (4.7 nF); pin 7 cannot provide their analogue readings.

## Remaining limits and assumptions

- The sheet images establish the named net connections above; continuity testing is still required before treating every trace as electrically proven.
- No dedicated Teensy VREF ADC connection was found. The current 1.65 V default/calibrated envelope reference remains in use.
- No firmware change resolves physical-pot operation. It requires rerouting the mux common to an ADC-capable Teensy pin or a board revision.
- `POWER_CHOKED_V1` remains mandatory. This work makes no rail, LED-current, or power-routing claim.

## Low-current validation sequence — do not flash from this checklist

1. With power removed, confirm continuity: pin 22↔`MRR`, pin 7↔`MRC`, pins 18/19↔OLED I²C; confirm no short between `MRC` and 3V3/GND.
2. Power at the existing low-current-safe configuration. Confirm the status LED and USB serial only; do not enable high-current LED tests.
3. Use a logic analyzer or serial diagnostic build to confirm row addresses 0–6 and column/control addresses 0–11 settle before reading.
4. Confirm one matrix press per row and simultaneous presses in distinct columns; idle matrix inputs should read high and pressed selected inputs low.
5. Confirm each `CTRL1`–`CTRL6` produces an active-high press and that an unpressed control reads low.
6. Confirm host/App/Bridge slot traffic remains functional with physical slot scanning disabled.
7. Record any mismatch as a board-revision or rework blocker; do not remap firmware pins without a measured physical connection.
