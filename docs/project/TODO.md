# Demo-First TODO (MOARkNOBS-42)

This file is **demo-first**: everything above “Post-demo Mischief” exists to make the demo feel calm,
inevitable, and repeatable.

Target demo outcomes:

- WebSerial editor app
- EF / Arp / ARG
- Basic clock following
- Profiles

If a feature already exists in firmware, it still gets a checkbox here as **VERIFY**. Demo risk is not
“does code exist,” it’s “will it behave on demand.”

Last edited: 2026-08-07

---

## Recent implementation snapshot (2026-02-14)

- [ ] Remaining work is hardware/demo verification (soak, EXT clock starvation check, panic-path validation on real rig, and asset prep).

---

## Public README Photo Backlog

- [x] Assembled front panel with knobs, buttons, OLED, and LEDs installed.
- [x] Bench setup during first-power and rail-topology validation.
- [x] MIDI, USB, and WebSerial/bridge test session with the board connected.
- [ ] Required rework close-up, or a clear "no rework required" board close-up after validation.

---

## 0) The 10-minute demo (run-of-show)

1. Plug in MN42 → open WebSerial editor → Connect
2. Show identity + active profile + staged diff panel
3. Make one tiny edit → Apply → confirm change on device and/or MIDI monitor
4. EF: show reactive envelope + gate stability when quiet
5. Arp: enable → change shape → change swing
6. Clock follow: show EXT vs INT + Start/Stop behavior (basic)
7. Profiles: load Profile B → obvious behavior change → save + reload proof
8. Close: “This is a workshop-friendly instrument: fast remaps, profiles, and modulation brains.”

---

## 1) Demo Blockers (must be true Monday)

### 1.1 WebSerial Editor: connect → edit → apply → rollback

- [x] **VERIFY:** WebSerial Connect succeeds on first try (no mystery steps)
  - Acceptance: user sees a clear “Connected to MOARkNOBS-42” state within 10s.
- [x] **VERIFY:** Device identity is visible in the app
  - Acceptance: FW version + device name + active profile index/name shown somewhere obvious.
- [x] **VERIFY:** Config loads from device into UI correctly
  - Acceptance: active profile + slot grid/state matches what the device shows.
- [x] **VERIFY:** Staged diff is visible and trustworthy
  - Acceptance: UI clearly shows “dirty” changes vs device state (no silent edits).
- [x] **VERIFY:** Apply writes changes and device acknowledges success
  - Acceptance: one edited field changes behavior immediately; no power-cycle required.
- [x] **VERIFY:** Rollback / cancel staged changes restores the pre-edit state
  - Acceptance: you can safely “undo” before applying; device remains unchanged.

**One “hero edit” we will demo (pick exactly one):**

- [x] Change a slot’s Type (CC/Note/NRPN/etc)
- [x] Change a slot’s CC number or channel
- [x] Change an EF parameter (mode/gate/attack/release) and see effect

---

### 1.2 EF demo: “it moves, and it doesn’t jitter”

- [ ] **VERIFY:** EF reacts clearly to a real input source (phone audio, pedal send, DAW loop)
  - Acceptance: visible movement on OLED/LED and/or MIDI monitor.
- [ ] **VERIFY:** EF gate/noise floor prevents jitter when silent
  - Acceptance: with no input, EF output settles to near-zero and does not spam.
- [ ] **VERIFY:** EF calibration path is available if baseline drifts
  - Acceptance: you can trigger recalibration (on-device combo OR WebSerial action) in < 10 seconds.
- [ ] **VERIFY:** EF diagnostic view is legible (baseline/gain/value)
  - Acceptance: during demo you can point at a page and explain what’s happening.

**EF “one sentence explanation” (write it now):**

- [x] “EF turns incoming audio amplitude into stable MIDI modulation with gating and calibration.”

---

### 1.3 Arp demo: “it feels alive, and it follows the rig”

- [x] **VERIFY:** Arp enable/disable is reliable and obvious
  - Acceptance: no stuck notes; OLED/LED indicates arp state.
- [x] **VERIFY:** Pattern shapes are selectable and audibly/visibly distinct
  - Acceptance: you can switch shape live and the pattern changes immediately.
- [x] **VERIFY:** Swing is controllable and demonstrable
  - Acceptance: show 50% vs a swung preset (e.g. 58–65%) and feel changes.
- [x] **VERIFY:** Arp behaves when clock source changes (INT ↔ EXT)
  - Acceptance: no runaway timing or lockups.

**Arp “one sentence explanation” (write it now):**

- [x] “Arp is a pattern brain with swing that can run internally or lock to external clock.”

---

### 1.4 ARG demo: “two envelopes in, one modulation out”

**Definition (what ARG is):**  
ARG = **Advanced Relative Gain**. It turns an EF into a **two-input modulation blender**: pick an envelope
pair (A,B) from the six EF inputs, then combine/compare them using **14 math methods**
(`A+B`, `A/B`, `max(A,B)`, `A^B`, etc.). The result becomes the slot’s modulation value.

**On-device controls (must be demo-reliable):**

- **Prereq:** active slot already has an **EF assigned**
- **Enter ARG:** `Ctrl0 + Ctrl1` (with EF enabled on that slot)
- **Cycle ARG method:** press `Ctrl0 + Ctrl1` again (cycles through 14 methods)
- **Cycle envelope pair (A,B):** `Ctrl2 + Ctrl5` (walks all unique pair combos from the 6 EF inputs)
- OLED should show method + pairing (e.g. `EF 1: A3/B0`) when changed

**WebSerial hooks (nice, but demo can succeed without them):**

- `SET_ARGMETHOD <0-13>` / `GET_ARGMETHOD`
- Status payload includes: `argEnabled`, `argMethod`, `argPair`

#### Demo acceptance checks

- [ ] **VERIFY:** ARG can be entered and exited without breaking EF or Arp
  - Acceptance: enable EF → enter ARG → bail out and return to normal EF behavior.
- [ ] **VERIFY:** Method cycling is obvious
  - Acceptance: switching methods produces clearly different output (not “subtle maybe?”).
- [ ] **VERIFY:** Pair cycling is obvious
  - Acceptance: you can point at the OLED and say “this is input A vs input B,” and behavior changes when you switch pairs.
- [ ] **VERIFY:** ARG output is visible somewhere
  - Acceptance: either (a) MIDI CC stream in a monitor changes, or (b) OLED/LED diagnostics show the value moving.

#### The 30-second ARG demo script (recommended)

1. Assign EF to the active slot (show EF moving)
2. Enter ARG (`Ctrl0+Ctrl1`)
3. Pick a “high contrast” method:
   - `MAXX` (louder wins), `XABS` (difference), or `XORR` (obvious glitch)
4. Pick a pair you can describe in words (e.g. “kick vs pad”, “mic vs noise”)
5. Point to the output and say:
   - “ARG compares two envelopes and turns that relationship into modulation.”

#### Fallback plan (if ARG gets weird live)

- If pairing feels confusing → lock to `AVG` or `MAXX` and stop touching pairs
- If output is too subtle → switch to `XORR` (always obvious) or `DIVI` (dramatic)
- If anything fights clock/arp → demo EF + arp and mention ARG briefly as “deeper demo later”

---

### 1.5 Clock following: basic, not heroic

- [x] **VERIFY:** External MIDI clock can be followed (choose one path: USB OR DIN)
  - Acceptance: bpm follows DAW clock; timing doesn’t visibly drift during demo window.
- [x] **VERIFY:** Start/Stop works (Continue optional)
  - Acceptance: Start resets pattern coherently; Stop halts without stuck notes.
- [x] **VERIFY:** Clock status is visible somewhere (diagnostic page OK)
  - Acceptance: you can point and say “INT / EXT / LOST”.

**Clock “one sentence explanation” (write it now):**

- [x] “MN42 can follow external clock so it stays in time with the session.”

---

### 1.6 Profiles: prove persistence

- [x] **VERIFY:** Load Profile A vs Profile B shows an obvious difference
  - Acceptance: a mapping or behavior changes immediately and visibly.
- [x] **VERIFY:** Save profile works (from device or app, whichever is canonical)
  - Acceptance: after save + power cycle (or reload), the change persists.
- [x] **VERIFY:** Profile identity is visible (index/name)
  - Acceptance: you can say “Profile 2 is the Bridge demo layout” and it’s shown.

**Profiles “one sentence explanation” (write it now):**

- [x] “Profiles let us swap whole controller behaviors instantly for different classes or rigs.”

---

## 2) Demo Polish (strongly recommended)

### 2.1 UX clarity

- [x] Add/verify a simple “Connected to: MOARkNOBS-42 (FW x.y)” banner in WebSerial UI
- [x] Add/verify a “What to do if connect fails” helper (close other serial clients, replug, refresh)
- [x] Ensure OLED labels for EF mode / arp shape / swing are readable and not abbreviated weirdly

### 2.2 Reliability / performance

- [ ] Verify 5-minute soak test: no UI lockups, no serial stalls, no stuck notes
- [ ] Verify LED/OLED updates don’t starve MIDI/clock handling (esp. with EXT clock)
- [ ] Verify “panic exit” path: one combo that returns to safe baseline state
  - Implemented combo: `Ctrl0 + Ctrl1 + Ctrl2` now does panic-safe reset (stop arp, disable EF follow, reload active profile baseline).

### 2.3 Demo assets

- [x] Prepare one short audio loop for EF input (phone file OR DAW loop)
- [x] Prepare one DAW project (or MIDI clock source) for clock follow
- [x] Prepare two named profiles: `DEMO_A`, `DEMO_B` (or a clear index story)

---

## 3) Demo Rehearsal Checklist (night before / morning of)

- [ ] Run the full run-of-show once without stopping
- [ ] Time it (target 8–12 minutes)
- [ ] Confirm cable kit: USB, any DIN adapters, audio feed cable, phone dongle if needed
- [ ] Confirm fallback plan:
  - If clock follow is flaky → run INT clock and explain EXT support briefly
  - If ARG is risky → keep ARG minimal (one method, one pair) or skip and mention it
  - If WebSerial is flaky → use on-device edits + show WebSerial briefly as “next”

---

## 4) Post-demo Mischief (true wishlist)

### Hardware / Mechanical (belongs in your hardware repo; keep pointers here if you want)

- [ ] Optional breakout board for expansion (I²C/SPI/GPIO + “safe/risky” silk)
- [ ] USB-C connector (device-only) + mechanical/case alignment + bootloader access

### EF experiments (beyond what exists today)

- [ ] Add additional filter responses (notch/tilt/peaking) if worth the complexity
- [ ] Multi-band / band-split EF (experimental mode)
- [ ] EF visualization polish: gate threshold marker + peak hold + dedicated EF “glance” page

### Arp / pattern brain upgrades

- [ ] Custom pattern shape (editable step array)
- [ ] Grid-based pattern edit mode (7×6 step editor)
- [ ] Arp diagnostics page: step index + length + clock source + mini timeline

### MIDI weird alleys

- [ ] Polyphonic Aftertouch (0xA0)
- [ ] Lightweight MPE fan-out slot type
- [ ] Extra MIDI utilities: Song Select, MMC shims, routing/filter toggles

### On-device “Config Mode”

- [x] Shallow OLED config menu for per-slot edits (type/channel/CC) + autosave on exit
  - Combo: `Ctrl0 + Ctrl2 + Ctrl3 + Ctrl5`
  - Controls in mode: `Ctrl0/1` slot prev/next, `Ctrl2` type, `Ctrl3` channel, `Ctrl4` data1/CC, `Ctrl5` exit+save

### Docs / tests / meta

- [ ] Update screenshots once hardware returns so the docs match the UI
- [ ] Extend tests as new features land (Poly AT, custom patterns, MPE)
- [ ] Keep this file honest: prune quarterly, archive dead ideas

### Hardware

- [x] Properly feed both the logic and LED circuits from input
