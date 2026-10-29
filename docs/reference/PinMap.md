# Bench Pin Map — Rev A / `POWER_CHOKED_V1`

This is the **assembled-board** contract reconciled against the 2026-08-13 schematics. It is intentionally narrower than the host configuration model: the 42 virtual slot mappings remain valid even where the physical board has no ADC path for a control.

| Teensy pin | Net / role | Firmware behavior |
|---:|---|---|
| 0 / 1 | MIDI RX / TX | DIN and TRS MIDI interface |
| 2–5 | `MUXR1`–`MUXR4` | row-mux address outputs |
| 6 | `LED` | WS2812 data |
| 7 | `MRC` | digital input from the column/control mux common; **not an ADC input** |
| 8–11 | `MUXC1`–`MUXC4` | column/control-mux address outputs |
| 14–17, 20–21 | `E1`–`E6` | envelope ADC inputs A0–A3, A6–A7 |
| 18 / 19 | `SDA` / `SCL` | OLED I²C; never used as mux ADC inputs |
| 22 / A8 | `MRR` | output driving the selected button-matrix row; never used as VREF ADC input |
| 23 | status LED | indicator output |

## Input topology

- The 42-button panel is a 7×6 matrix. `MRR` selects/drives one row at a time; `MRC` reads a selected column. Matrix buttons are active-low, with column pull-ups shown on sheet 1.
- `CTRL1`–`CTRL6` occupy column/control-mux channels 6–11 and are active-high, with 10 kΩ pulldowns shown on sheet 2.
- `MAIN`, `FP1`, and `FP2` occupy channels 12–14 of that mux. Their shared `MRC` route reaches pin 7, which is digital-only. Firmware therefore does **not** sample these physical pots and must not synthesize MIDI movement from them.
- The 42 virtual slot controls and their saved MIDI mappings remain host/protocol concepts. The assembled Rev A board has no verified ADC-routed 42-slot-pot mux path.
- `VREF` is used by the envelope circuits but has no verified dedicated ADC route to the Teensy. Firmware retains its calibrated/default reference rather than sampling A8/MRR as VREF.

`MRC` includes R95 (220 Ω) in series and C77 (4.7 nF) to ground. Allow the existing 5 µs mux settling delay for digital reads; this document does not claim analogue settling performance for the unavailable pot path.

## Sources and limits

Evidence: supplied schematic sheets `SCH_MOAR_Schematic_1-PWR-BUTTON-LED_2026-08-13.png` and `SCH_MOAR_Schematic_2-INTERFACE-MIDI-CNTRL_2026-08-13.png`. Exact PCB connectivity remains subject to low-current bench confirmation. Do not change `POWER_CHOKED_V1` or increase LED limits based on this document.
