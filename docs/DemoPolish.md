# Demo Polish Runbook

Use this checklist to close out `docs/TODO.md` section 2 with repeatable steps.

## 1. Five-Minute Soak Test

1. Flash `teensy40_main` and open `App/index.html`.
2. Connect over WebSerial and keep the page open for 5 minutes.
3. During the run, repeatedly:
   - switch slots,
   - stage/apply one small edit,
   - toggle arp (`Ctrl2 + Ctrl4`),
   - trigger EF input with your demo audio loop.
4. Pass criteria:
   - no browser UI freeze,
   - no serial disconnect/reconnect loop,
   - no stuck notes after arp stop.

## 2. LED/OLED Starvation Check (EXT Clock)

1. Feed external MIDI clock from DAW (or hardware clock source).
2. While clock is running, trigger heavy UI churn:
   - rapid slot changes,
   - repeated profile load/save,
   - envelope activity across multiple EFs.
3. Watch diagnostics and behavior:
   - OLED still updates,
   - LEDs still animate,
   - arp timing remains locked,
   - no obvious timing stalls.

## 3. Panic Exit Verification

Panic combo is now: `Ctrl0 + Ctrl1 + Ctrl2`.

Expected result:

1. Arp stops.
2. EF follow toggles off.
3. Active profile reloads from EEPROM baseline.
4. OLED shows `Panic: Baseline`.

Pass criteria:

1. One combo reliably returns the rig to a safe baseline state.
2. No stuck note behavior after panic.

## 4. Demo Asset Pack

Prepare this folder locally before demo day:

1. `demo-assets/audio/ef-demo-loop.wav` (10-30 seconds, clear dynamics).
2. `demo-assets/clock/mn42-clock-source.als` (or your DAW project format).
3. Profiles:
   - `DEMO_A - Reactive Stack`
   - `DEMO_B - Clock Contrast`

The two demo profiles are preloaded in the WebSerial preset picker.
