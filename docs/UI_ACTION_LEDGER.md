# UI Action Ledger

**Detailed behavioral ledger for all button gestures and their side effects.**

The machine-readable source for gesture identity, control family, interaction layer, reversibility, generated quick reference, and OLED chord-help strings is [`reference/on_device_control_registry.json`](reference/on_device_control_registry.json). This ledger remains the detailed side-effect view and is checked against that registry.

This document captures every user gesture, its conditions, resulting actions, OLED feedback, LED behavior, and whether it triggers persistent storage writes or WebSerial events.

## Legend

| Column                | Description                                            |
| --------------------- | ------------------------------------------------------ |
| **Gesture**           | Button press pattern (Short/Double/Long+Confirm/Combo) |
| **Condition**         | When this gesture is active (mode, state)              |
| **Action**            | What the firmware does                                 |
| **OLED Text**         | Exact text shown on 128x64 display                     |
| **LED Behavior**      | Visual feedback pattern                                |
| **Persistent Write?** | EEPROM/LittleFS write triggered                        |
| **WebSerial Event**   | JSON patch emitted to browser                          |

---

## Virtual Buttons (Slots 0-41)

### Slot Selection & EF Assignment

| Gesture                    | Condition             | Action                 | OLED Text                 | LED Behavior                    | Persistent Write? | WebSerial Event              |
| -------------------------- | --------------------- | ---------------------- | ------------------------- | ------------------------------- | ----------------- | ---------------------------- |
| Short Press (Btn 0-41)     | Always                | Select slot as active  | `Active Slot=N` (1s)      | Trigger control button LED      | No                | No                           |
| Double Press (Btn 0-41)    | Slot has EF assigned  | Cycle filter type +1   | `Slot N => TYPE` (1.5s)   | None                            | Yes (EF settings) | `slot_patch`, `filter_patch` |
| Long+Confirm (Btn 0-41)    | Always                | Arm EF assignment mode | `CONFIRM\nTap again` (1s) | Warning animation (if Ctrl btn) | No                | No                           |
| → Tap Ctrl0-5 (after long) | EF assign armed       | Assign EF 0-5 to slot  | `Slot N -> EF M` (1.5s)   | None                            | Yes (EF settings) | `envelope_assignment`        |
| → Timeout (no tap)         | EF assign armed (>3s) | Cancel assignment      | `EF assign timeout` (1s)  | Clear warning                   | No                | No                           |

---

## Control Buttons (Ctrl0-Ctrl5)

### Ctrl0 — EF Toggle / Calibrate

| Gesture      | Condition           | Action                | OLED Text                   | LED Behavior | Persistent Write? | WebSerial Event              |
| ------------ | ------------------- | --------------------- | --------------------------- | ------------ | ----------------- | ---------------------------- |
| Short Press  | Always              | Toggle EF mode ON/OFF | `EF: ON` / `EF: OFF` (1.5s) | None         | No                | No                           |
| Double Press | Active slot has EF  | Cycle filter +1       | `Slot N => TYPE` (1.5s)     | None         | Yes (EF settings) | `slot_patch`, `filter_patch` |
| Long+Confirm | EF assigned to slot | Calibrate EF baseline | `EF Calibrated` (1.5s)      | None         | Yes (baseline)    | No                           |

### Ctrl1 — Next Slot / Profile Reset

| Gesture      | Condition          | Action                     | OLED Text               | LED Behavior | Persistent Write? | WebSerial Event              |
| ------------ | ------------------ | -------------------------- | ----------------------- | ------------ | ----------------- | ---------------------------- |
| Short Press  | Always             | Advance to next slot       | `Next Slot=N` (1.5s)    | None         | No                | No                           |
| Double Press | Active slot has EF | Cycle filter -1            | `Slot N => TYPE` (1.5s) | None         | Yes (EF settings) | `slot_patch`, `filter_patch` |
| Long+Confirm | Always             | Reload profile from EEPROM | `Profile Reset!` (1.5s) | None         | No (read-only)    | No                           |

### Ctrl2 — EF Cycle / ARP / Swing

| Gesture                 | Condition   | Action                  | OLED Text                        | LED Behavior | Persistent Write? | WebSerial Event       |
| ----------------------- | ----------- | ----------------------- | -------------------------------- | ------------ | ----------------- | --------------------- |
| Short Press             | EF mode ON  | Cycle EF assignment     | `Slot N -> EF M` (1.5s)          | None         | Yes (EF settings) | `envelope_assignment` |
| Short Press             | EF mode OFF | Show error              | `EF is OFF` (1s)                 | None         | No                | No                    |
| Double Press            | Always      | Cycle MIDI message type | `Slot N Type N` (1.5s)           | None         | Yes (slot type)   | `slot_patch`          |
| Long+Confirm (w/ Ctrl4) | ARP active  | Toggle ARP edit mode    | `Arp Edit` / `Arp Edit Off` (1s) | None         | No                | No                    |
| Long+Confirm (w/ Ctrl3) | Always      | Cycle swing presets     | `Swing: N%` (1s)                 | None         | No                | No                    |

### Ctrl3 — Channel / EEPROM / Panic

| Gesture          | Condition  | Action                     | OLED Text                | LED Behavior        | Persistent Write? | WebSerial Event |
| ---------------- | ---------- | -------------------------- | ------------------------ | ------------------- | ----------------- | --------------- |
| Short Press      | Always     | Cycle MIDI channel 1-16    | `Slot N => Ch N` (1.5s)  | None                | Yes (channel)     | `slot_patch`    |
| Long+Confirm     | Always     | Reload config from EEPROM  | `Config Reloaded` (1.5s) | Destructive warning | No (read-only)    | No              |
| Combo (w/ Ctrl0) | Always     | Set slot to SysEx          | `Slot N => SYSEX` (1.5s) | None                | Yes (slot type)   | `slot_patch`    |
| Combo (w/ Ctrl1) | Always     | Set slot to RPN            | `Slot N => RPN` (1.5s)   | None                | Yes (slot type)   | `slot_patch`    |
| Combo (w/ Ctrl2) | ARP active | Increment ARP base note    | `ARP NOTE N` (1s)        | None                | Yes (arpNote)     | `slot_patch`    |
| Combo (w/ Ctrl4) | Always     | Cycle LED modes            | `LightMode=N` (1.5s)     | Changes LED pattern | Yes (LED mode)    | No              |
| Combo (w/ Ctrl5) | Always     | Set slot to Program Change | `Slot N => PROG` (1.5s)  | None                | Yes (slot type)   | `slot_patch`    |

### Ctrl4 — CC Number / Save / Light

| Gesture          | Condition        | Action                  | OLED Text                   | LED Behavior        | Persistent Write?  | WebSerial Event       |
| ---------------- | ---------------- | ----------------------- | --------------------------- | ------------------- | ------------------ | --------------------- |
| Short Press      | CC/NRPN/RPN mode | Cycle CC/NRPN number    | `Slot N => CC N` (1.5s)     | None                | Yes (CC number)    | `slot_patch`          |
| Long+Confirm     | Always           | Save config to profile  | `Config Saved` (1.5s)       | None                | Yes (full profile) | No                    |
| Combo (w/ Ctrl0) | Always           | Randomize EF assignment | `Slot N->RandomEF N` (1.5s) | None                | Yes (EF settings)  | `envelope_assignment` |
| Combo (w/ Ctrl1) | Always           | Set slot to Aftertouch  | `Slot N => AFTER` (1.5s)    | None                | Yes (slot type)    | `slot_patch`          |
| Combo (w/ Ctrl2) | Always           | Toggle ARP on/off       | `ARP ON` / `ARP OFF` (1s)   | None                | No                 | No                    |
| Combo (w/ Ctrl3) | Always           | Cycle LED modes         | `LightMode=N` (1.5s)        | Changes LED pattern | Yes (LED mode)     | No                    |
| Combo (w/ Ctrl5) | Always           | Set slot to Note mode   | `Slot N => NOTE` (1.5s)     | None                | Yes (slot type)    | `slot_patch`          |

### Ctrl5 — BPM / Diagnostics

| Gesture          | Condition           | Action                 | OLED Text                 | LED Behavior          | Persistent Write? | WebSerial Event |
| ---------------- | ------------------- | ---------------------- | ------------------------- | --------------------- | ----------------- | --------------- |
| Short Press      | Diagnostic mode OFF | Tap BPM                | `Tapped BPM=N.N` (1.5s)   | None                  | No                | No              |
| Short Press      | Diagnostic mode ON  | Exit diagnostic mode   | `Diag Off` (1s)           | Clear diagnostic LEDs | No                | No              |
| Long+Confirm     | Diagnostic mode OFF | Enter diagnostics      | `Diagnostics` (1s)        | Diagnostic mode LEDs  | No                | No              |
| Long+Confirm     | Diagnostic mode ON  | Cycle diagnostic page  | `Diag Page` (1s)          | Diagnostic mode LEDs  | No                | No              |
| Combo (w/ Ctrl0) | Always              | Set slot to Pitch Bend | `Slot N => BEND` (1.5s)   | None                  | Yes (slot type)   | `slot_patch`    |
| Combo (w/ Ctrl1) | Always              | Toggle MIDI clock out  | `CLK OUT ON` / `OFF` (1s) | None                  | No                | No              |
| Combo (w/ Ctrl2) | Always              | Set slot to NRPN       | `Slot N => NRPN` (1.5s)   | None                  | Yes (slot type)   | `slot_patch`    |

---

## Multi-Button Combos (Settle Window: 80ms)

| Gesture     | Condition   | Action                       | OLED Text                                             | LED Behavior        | Persistent Write?       | WebSerial Event             |
| ----------- | ----------- | ---------------------------- | ----------------------------------------------------- | ------------------- | ----------------------- | --------------------------- |
| Ctrl0+1+2   | Always      | Panic reset to baseline      | `Panic: Baseline` (1.5s)                              | None                | No (reload)             | No                          |
| Ctrl0+1     | ARG enabled | Cycle ARG method             | `Slot N ARG=METHOD` (1.5s)                            | None                | Yes (ARG settings)      | `slot_patch`, `arg_patch`   |
| Ctrl0+2     | Always      | Cycle ARG envelope pair      | `Slot N: EF N+EF N` (1.5s)                            | None                | Yes (ARG settings)      | `slot_patch`, `arg_patch`   |
| Ctrl0+3     | Always      | Set slot to SysEx            | `Slot N => SYSEX` (1.5s)                              | None                | Yes (slot type)         | `slot_patch`                |
| Ctrl0+4     | Always      | Randomize EF assignment      | `Slot N->RandomEF N` (1.5s)                           | None                | Yes (EF settings)       | `envelope_assignment`       |
| Ctrl0+5     | Always      | Set slot to Pitch Bend       | `Slot N => BEND` (1.5s)                               | None                | Yes (slot type)         | `slot_patch`                |
| Ctrl1+2     | Always      | Cycle profiles A-D           | `PROFILE X` (1.5s)                                    | None                | No (reload)             | No                          |
| Ctrl1+3     | Always      | Set slot to RPN              | `Slot N => RPN` (1.5s)                                | None                | Yes (slot type)         | `slot_patch`                |
| Ctrl1+4     | Always      | Set slot to Aftertouch       | `Slot N => AFTER` (1.5s)                              | None                | Yes (slot type)         | `slot_patch`                |
| Ctrl1+5     | Always      | Toggle MIDI clock out        | `CLK OUT ON` / `OFF` (1s)                             | None                | No                      | No                          |
| Ctrl1+4+5   | Always      | Toggle clock source          | `CLK SRC EXT` / `CLK SRC INT` (1.2s)                  | None                | No                      | No                          |
| Ctrl0+1+3   | Always      | Toggle LFO quick-tune mode   | `LFO Tune ON` / `LFO Tune OFF` (1s)                   | None                | No                      | No                          |
| Ctrl0+1+4   | Always      | Toggle live LFO 1 slot lane  | `LFO1 LIVE ON` / `LFO1 LIVE OFF` (1.2s)               | None                | Yes (slot LFO lane)     | `slot_patch`                |
| Ctrl2+3     | ARP active  | Increment ARP base note      | `ARP NOTE N` (1s)                                     | None                | Yes (arpNote)           | `slot_patch`                |
| Ctrl2+4     | Assigned slot or active ARP | Toggle ARP on/off | `ARP ON` / `ARP OFF` / `ARP UNASSIGNED` | None | No | No |
| Ctrl2+5     | Always      | Set slot to NRPN             | `Slot N => NRPN` (1.5s)                               | None                | Yes (slot type)         | `slot_patch`                |
| Ctrl3+4     | Always      | Cycle LED modes              | `LightMode=N` (1.5s)                                  | Changes LED pattern | Yes (LED mode)          | No                          |
| Ctrl3+5     | Always      | Set slot to Program Change   | `Slot N => PROG` (1.5s)                               | None                | Yes (slot type)         | `slot_patch`                |
| Ctrl4+5     | Always      | Set slot to Note mode        | `Slot N => NOTE` (1.5s)                               | None                | Yes (slot type)         | `slot_patch`                |
| Ctrl3+4+5   | Always      | Toggle USB MIDI out          | `USB MIDI ON` / `OFF` (1.5s)                          | None                | No                      | No                          |
| Ctrl0+2+3+5 | Always      | Toggle on-device config mode | `Config Mode ON` / `Config Saved` / `Config Mode OFF` | None                | Exit autosaves if dirty | `slot_patch` (during edits) |

---

## Control Pot Gestures (LFO Quick-Tune Mode)

| Gesture         | Condition            | Action                                          | OLED Text               | LED Behavior | Persistent Write? | WebSerial Event |
| --------------- | -------------------- | ----------------------------------------------- | ----------------------- | ------------ | ----------------- | --------------- |
| Adjust CtrlPot0 | LFO tune mode active | Set selected LFO free-run frequency             | `LFOn Hz N.NN` (short)  | None         | No                | No              |
| Adjust CtrlPot1 | LFO tune mode active | Set selected LFO depth                          | `LFOn D N.NN` (short)   | None         | No                | No              |
| Adjust CtrlPot2 | Sync ON              | Set selected LFO sync ratio (quantized 1/1..x4) | `LFOn Sync X` (short)   | None         | No                | No              |
| Adjust CtrlPot2 | Sync OFF             | Set selected LFO polarity                       | `LFOn Bipolar/Unipolar` | None         | No                | No              |

---

## Control Pot Gestures (Jitter Mode)

| Gesture         | Condition          | Action                   | OLED Text              | LED Behavior | Persistent Write? | WebSerial Event |
| --------------- | ------------------ | ------------------------ | ---------------------- | ------------ | ----------------- | --------------- |
| Hold Ctrl0+3+4  | Always             | Enter jitter tuning mode | `Jitter Mode` (1s)     | None         | No                | No              |
| Adjust CtrlPot0 | Jitter mode active | Set jitter depth         | `Jitter: N.NN` (short) | None         | No                | No              |
| Adjust CtrlPot1 | Jitter mode active | Set jitter smoothness    | `Smooth: N.NN` (short) | None         | No                | No              |

---

## Filter Tuning Persistence Policy

**Live update:** Immediate (every loop while control pots move)
**Persistence trigger:** 900ms idle + 500ms minimum interval
**WebSerial emission:** Only on persistence (not on every live update)

This prevents flash wear during continuous tuning gestures while maintaining responsive UI feedback.

---

## Chord Settle Timing

**Settle window:** 80ms
**Rationale:** Accommodates human finger spread variance without introducing noticeable lag

Multi-button combos must remain stable for 80ms before firing. This prevents partial chord assemblies from triggering unintended actions.

---

## OLED Text Compression Standards

All OLED messages follow these constraints:

- **Line 1:** 16 characters max (title/context)
- **Line 2:** 16 characters max (detail/value)
- **Line 3:** 16 characters max (secondary detail)

**Compression patterns:**

- `Freq` → `F`
- `Gate` → `G`
- `Octave` → `O`
- `Velocity` → `V`
- `Probability` → `P`
- `Arp Edit` → `Arp`
- `Note Dyn` → `Note`

**Ritual text over prose:** Short glyphs and numbers beat sentences on 128x64.

---

## OLED Dominance Order

When multiple display producers are active at once, OLED ownership resolves in this order:

1. Startup animation
2. Modal edit views (`on-device config`, `LFO tune`, `jitter tune`, `diagnostics`)
3. Held-control contextual command palette
4. Status overlays / temporary messages (only when no modal edit view is active)
5. Control overlays (`filter`, `arp`, `arp edit`, `note dynamics`)
6. Screensaver
7. Baseline context view

This is enforced in the low-priority display scheduler path.

---

## Diagnostic Polarity

**Button matrix display:** `#` = pressed, `.` = released

The scan path normalizes active-low hardware to active-high logic. Diagnostic display uses `#` to match mental model (filled = active).

---

## Protocol Discipline

**All WebSerial output is JSON.** No exceptions.

Plain-text debug logs (e.g., `Tasks per second: 523`) are converted to structured JSON:

```json
{ "type": "diag", "metric": "tasks_per_second", "value": 523 }
```

This ensures the browser-side configurator can parse all output without landmines.

---

## Last Updated

2026-05-05 — Display Priority + Modal Feedback Audit
