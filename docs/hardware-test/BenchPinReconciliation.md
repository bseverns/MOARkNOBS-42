# Bench Pin Reconciliation — 2026-08-13 Board

## Confirmed physical signal map

| Function | Board evidence | Firmware contract |
|---|---|---|
| matrix row common | `MRR` → Teensy 22/A8 | output, held low while the addressed row is scanned |
| matrix/control common | original board route: `MRC` → Teensy 7 | rework target: Teensy 24/A10; 60 µs settle after every mux-address change |
| matrix | seven `ROW` nets × six `COL` nets | active-low button inputs |
| direct controls | `CTRL1`–`CTRL6`, mux channels 6–11 | active-high inputs |
| OLED | Teensy 18/19 `SDA`/`SCL` | I²C only |
| envelope | `E1`–`E6` | A0–A3, A6–A7 |

## Corrected discrepancies

- Firmware no longer reads A4/A5 (OLED I²C) as mux ADC inputs.
- Firmware no longer samples A8/MRR as VREF.
- Before rework, matrix scanning drives pin 22 and reads pin 7 digitally; direct controls use the same pin-7 mux input with their actual active-high polarity.
- The 42 virtual slot values remain protocol/configuration state, but their unverified physical ADC scan is disabled.
- The pin-24/A10 firmware waits 60 µs after every mux-address change and enables `MAIN`, `FP1`, and `FP2`. Their MRC route includes R95 (220 Ω) and C77 (4.7 nF); pin 7 cannot provide their analogue readings.

## MRC analog-input correction

Firmware is prepared for `MRC` on Teensy 4.0 pin **24 / A10**. Do not flash it until this board work passes continuity checks.

1. Power off and unplug USB and external supplies. Photograph both sides of the installed Teensy and carrier.
2. Confirm continuity from `MRC` to Teensy pin 7. If absent, stop: this is a different board revision.
3. Confirm pin 24 / A10 has no carrier-board connection and is physically reachable. It is an underside Teensy pad.
4. With the original route intact, verify from the schematic and careful live measurement that MAIN, FP1, and FP2 stay within 0–3.3 V. Never apply an unknown or >3.3 V signal to Teensy 4.0.
5. Power off again. Isolate the carrier's `MRC` route from Teensy pin 7 at a continuity-verified accessible location. Do not choose a cut from artwork alone.
6. Confirm `MRC`→pin 7 is open, `MRC` still reaches the mux common, and pin 24 remains isolated from every other carrier net.
7. Install one short insulated jumper from a verified `MRC` pad to Teensy pin 24 / A10. Prefer a component or pad endpoint over a scraped trace.
8. Confirm `MRC`→pin 24 continuity, `MRC`→pin 7 open circuit, and no short from `MRC` or pin 24 to 3.3 V or ground.
9. Flash the matching firmware. First power from a current-limited source when practical; check the 3.3 V rail and for heating before operating controls.
10. Verify all 42 matrix buttons and six control buttons, then confirm MAIN/FP1/FP2 each sweep substantially across 0–1023 without moving the other two.

If pin 24 is inaccessible, stop rather than substitute a convenient pin. Pin 23 / A9 is ADC-capable but belongs to the status-LED circuit and requires a separate isolation plan.

## Remaining limits and assumptions

- The sheet images establish the named net connections above; continuity testing is still required before treating every trace as electrically proven.
- No dedicated Teensy VREF ADC connection was found. The current 1.65 V default/calibrated envelope reference remains in use.
- Physical-pot operation depends on completing and validating the pin-24/A10 reroute above.
- `POWER_CHOKED_V1` remains mandatory. This work makes no rail, LED-current, or power-routing claim.

## Low-current validation sequence — do not flash from this checklist

1. With power removed, confirm continuity: pin 22↔`MRR`, pin 24/A10↔`MRC`, pins 18/19↔OLED I²C; confirm pin 7↔`MRC` is open and no short exists between `MRC` and 3V3/GND.
2. Power at the existing low-current-safe configuration. Confirm the status LED and USB serial only; do not enable high-current LED tests.
3. Use a logic analyzer or serial diagnostic build to confirm row addresses 0–6 and column/control addresses 0–11 settle before digital reads; verify analog channels 12–14 are stable before sampling.
4. Confirm one matrix press per row and simultaneous presses in distinct columns; idle matrix inputs should read high and pressed selected inputs low.
5. Confirm each `CTRL1`–`CTRL6` produces an active-high press and that an unpressed control reads low.
6. Confirm MAIN, FP1, and FP2 independently traverse most of 0–1023, then confirm host/App/Bridge slot traffic remains functional.
7. Record any mismatch as a board-revision or rework blocker; do not remap firmware pins without a measured physical connection.
