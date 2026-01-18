# Post-release Wishlist

These are the "nice to have" riffs once the first demo ships. No blockers, just future mischief.

---

## Hardware / Mechanical

- [ ] Offer an optional breakout board so modders can hang more toys off the bus.  
  - Notes:  
    - Expose spare I²C, SPI, and a few GPIOs (with clear 5V/3V3 labeling) for sensor/footswitch/LED-strip experiments.  
    - Include silk for “safe” vs “risky” pads to keep new builders from nuking the bus.  
    - Firmware hooks: consider reserving a small “expansion device table” so new firmware modules can claim pins cleanly instead of hardcoding.

- [ ] Add USB-C connector instead of Teensy USB-A micro board stub (requires board cutout/castellation near bottom pads).  
  - Notes:  
    - Target: USB-C *device* only, no PD/alt-mode, just robust connector + strain relief.  
    - Coordinate mechanical clearances with case v2 so the panel opening and strain relief are future-proof.  
    - Verify bootloader and programming access are still reachable once Teensy pad castellations are used.

---

## Firmware / UX Roadmap

These are firmware-side riffs that build on the current architecture: envelope followers as first-class modulators, an arp that feels alive, and on-device mapping that doesn’t require a laptop.

### 1. Envelope Follower (EF) Enhancements

Make the six EF channels feel like a small modulation engine rather than a fixed effect.

- [ ] Add new EF filter shapes  
  _Goal:_ More ways to interpret the incoming amplitude beyond “plain low-pass.”  
  - Implement additional filter modes in `EnvelopeFollower` (e.g. RMS-style smoothing, notch/tilt, “peaky” transient mode).  
  - Wire new enums into ConfigManager’s EF settings and OLED labels so they show up as readable names.

- [ ] Implement per-channel noise floor / gate  
  _Goal:_ Stop tiny amounts of analog hiss from spitting MIDI jitter.  
  - Add a small “noise floor” threshold per EF; values below this read as zero.  
  - Expose gate threshold as a parameter (even if only via WebSerial at first) so different sources (noisy pedals vs clean DAW sends) can be tuned.

- [ ] Baseline auto-calibration and drift handling  
  _Goal:_ Let EF zero itself as the rig warms up and sources change.  
  - Build on the existing `CAL_ENVS` path: track long-term average level per EF and nudge baseline toward it when inputs are idle.  
  - Optionally auto-run a short calibration on boot (with a timeout) so “no signal” really means zero.  
  - Keep manual “hold-a-combo → recal EF” as a panic button for live use.

- [ ] Soft auto-scaling of EF gain  
  _Goal:_ Use the full 0–127 range without constant hand-tuning.  
  - Track recent peak levels per EF and gently adjust gain so they live in a healthy MIDI range.  
  - Make auto-scaling optional (per EF or global), so users who want strict, predictable scaling can turn it off.

- [ ] Multi-band / multi-flavor EF experiments (stretch goal)  
  _Goal:_ Let one audio feed drive multiple behaviors (e.g. lows vs highs).  
  - Prototype a digital band-split path: sample EF input at higher rate, run simple digital BPF/LPF/HPF, and derive multiple envelopes from one jack.  
  - Keep this as an optional “experimental mode” so the simple analogue-first path stays rock solid.

- [ ] EF visualization polish  
  _Goal:_ Help users see what the EF is really doing.  
  - Tighten the OLED bar-graph and LED feedback so it clearly shows gate threshold, current level, and maybe peak hold.  
  - Consider a dedicated EF diagnostic page that shows per-channel baseline, gain, and current value in a single glance.

---

### 2. Arpeggiator / Pattern Engine

Turn the arp into a tiny pattern brain that still feels immediate in live use.

- [ ] Add new arp pattern shapes (no refactor required)  
  _Goal:_ More musical variety with the existing “shape + length” grammar.  
  - Extend the pattern-shape enum and `noteOffset()` logic with a few new shapes (e.g. up-down, Euclidean-ish, “drunk walk”).  
  - Keep everything data-driven so shapes are just extra branches, not new systems.

- [ ] Implement swing / groove for internal clock  
  _Goal:_ Less metronome, more pulse.  
  - Add a swing parameter (e.g. 50–75%) that offsets every other step in the internal-clock path.  
  - Map swing to an existing control (e.g. hold a combo + twist a knob) and echo the value on OLED (“Swing: 58%”).  
  - Ensure external MIDI clock stays authoritative—swing only affects the internal clock.

- [ ] Tighten external MIDI clock handling  
  _Goal:_ Arp starts and stops in time with the rest of the rig.  
  - Explicitly handle MIDI Start/Stop/Continue: reset arp step on Start, pause on Stop, resume on Continue.  
  - Verify clock-tick handling in the 1 ms scheduler path is jitter-free and doesn’t get starved by LED/OLED updates.

- [ ] “Custom pattern” shape  
  _Goal:_ Let the user define a pattern instead of only choosing baked shapes.  
  - Add a `Custom` pattern type that uses an internal step array (e.g. on/off or simple offsets) rather than computed offsets.  
  - Provide a small API for setting that array from UI/WebSerial; treat storage as ephemeral at first (no EEPROM changes required yet).

- [ ] Grid-based pattern edit mode (stretch but juicy)  
  _Goal:_ Use the 7×6 button grid as a step editor.  
  - Add a “pattern edit” mode toggled via a control-button chord.  
  - In this mode, slot buttons map to steps; LEDs indicate active/inactive steps.  
  - OLED shows the slot being edited and basic pattern info (“Slot 12: Steps 8, Shape: Custom”).  
  - Keep this mode modal but shallow—easy to enter, easy to bail out without wrecking other mappings.

- [ ] Arp diagnostics / visualization  
  _Goal:_ Debug odd timing and patterns visibly.  
  - Add a diagnostic page that shows current pattern length, current step index, and clock source (INT vs EXT).  
  - Optionally show a one-line “timeline” on OLED with a marker for current step.

---

### 3. MIDI Handling & “Weird Alleys”

Use the existing flexible MIDI infrastructure to open up more advanced use without overcomplicating the surface.

- [ ] Add Polyphonic Aftertouch message type  
  _Goal:_ Round out the MIDI spec coverage.  
  - Extend the MIDI message enum + send path to support Poly AT (0xA0) using slot data1 as note number.  
  - Expose in WebSerial editor first; on-device mapping can come later.

- [ ] Simple MPE helper modes (lightweight first pass)  
  _Goal:_ Let power users get to MPE-ish behavior without a deep re-architecture.  
  - Add a new “MPE fan-out” slot type that sends the same CC or Pitch Bend across a defined channel range (e.g. 2–16) when that slot moves.  
  - Provide a config command or option in the editor to set lower/upper channel bounds.  
  - Document clearly as a convenience layer, not a full per-note MPE brain.

- [ ] MIDI clock & transport polish  
  _Goal:_ MN42 behaves like a good MIDI citizen in hybrid rigs.  
  - Confirm robust handling of MIDI Clock, Start, Stop, and Continue on both USB and DIN.  
  - Consider a small “clock status” indicator on OLED (INT, EXT, LOST) to help debug rigs.

- [ ] Additional MIDI utilities (later)  
  _Goal:_ Keep the door open for utility messages without clutter.  
  - Reserve room in MIDI handler for future: Song Select, MMC transport shims, lightweight routing/filtering (e.g. drop active sense, optionally drop clock).

---

### 4. On-Device Configuration & UX

Shrink the distance between “idea” and “remap” so users don’t always need the WebSerial editor.

- [ ] Quick channel/CC tweaks via button combos  
  _Goal:_ Fix obvious mapping mistakes without a laptop.  
  - Add combos like “Ctrl + button” to increment/decrement channel or CC for the active slot.  
  - After change, flash a tiny OLED status line: `Chan -> 7`, `CC -> 22`.  
  - Keep combos discoverable and documented in the cheat sheet.

- [ ] On-device type cycling (CC ↔ Note ↔ NRPN ↔ …)  
  _Goal:_ Let users rough-in a slot’s behavior on the hardware.  
  - Add a combo to cycle the active slot’s MIDI type through the supported set.  
  - Immediately reflect new type on OLED (e.g. `Type: CC`, `Type: Note`).

- [ ] Lightweight “Config Mode” on OLED (stretch)  
  _Goal:_ A shallow menu for per-slot edits that still feels like an instrument, not a DAW.  
  - Add a modal “config mode” that repurposes slot buttons to select which slot to edit and filter knobs to change type/CC/channel.  
  - Draw a simple 2–3 line view: `Slot 5  Type: CC  Ch: 2`, `CC#: 14`.  
  - Exit quickly via the same combo; autosave to EEPROM on exit.

- [ ] Extra diagnostic pages  
  _Goal:_ Make hardware/firmware edge cases legible.  
  - Extend the existing diagnostic carousel with:  
    - EF headroom/baseline view (per channel).  
    - Loop timing/latency snapshot.  
    - MIDI event counters (per port, per message family).

---

### 5. Mod Sources Beyond EF (LFO / Noise, later)

Formalize the “wiggle” helpers you’re already hinting at.

- [x] LFO module v1  
  _Goal achieved:_ Dual LFOs now run at 1 kHz with rate/shape/depth controls, internal routes to LEDs/arp/EF gain, MIDI/OSC paths, and profile persistence so every boot replays the same modulation state.  
  - The `LFO` class ticks via `Utility::schedulerHigh`, the manager reports normalized outputs, and Unity tests guard the shapes/clock math.  
  - Routes feed the OLED diagnostics, Web Config editor, and EEPROM-backed profiles so the UI and firmware paint the same modulation bus.

---

### Web Config Builder Follow-ups

- [ ] Capture the new staged diff panel, telemetry stack, and simulator workflow in `App/README` + docs once the hardware returns so the screenshots finally match the feature set.  
  _Goal:_ Make the browser’s security choreography and UI affordances visible in the same place as the instructions you read before you flash the firmware.
- [ ] Expand the runtime contract notes (schema-driven forms, `FormRenderer`, simulator transport, checksum rollback) inside `docs/WebSerial.md` or the Handbook so future contributors can grow the UI without reverse-engineering the kernel.

---

### 6. Testing, Docs, and Meta

Make sure future you (and future collaborators) don’t have to reverse-engineer 2025 you.

- [ ] Extend Unity / hardware test coverage as features land  
  _Goal:_ Keep the “test gauntlet” aligned with new behavior.  
  - Add tests for new EF modes (filter math, baseline/gain persistence).  
  - Add tests for new MIDI types and clock handling.  
  - Extend hardware smoke tests to include “EF gate” and “arp swing” sanity checks.

- [ ] Update docs as features ship  
  _Goal:_ Documentation that reads like a field manual, not a museum label.  
  - Add sections to firmware docs for each new EF mode, arp shape, and on-device combo.  
  - Include small “recipes” (e.g. “Sidechain your filter with EF3 + Arp swing”).

- [ ] Keep this TODO.md honest  
  _Goal:_ Let the roadmap breathe.  
  - Periodically prune, promote, or archive items based on real-world use and builder feedback.  
  - Mark the experiments that graduate from “mischief” to “core affordance.”
