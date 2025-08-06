# MOARkNOBZ MN42 MIDI Controller

> Firmware for the unapologetically DIY, button-stabbin', knob-hackin', MIDI-mashin' controller you didn’t ask for but definitely need.

*Need a bird's-eye view of the whole project? Scoot up to the repo's [README](../README.md) for hardware notes and overall organization.*

## What's This?

The **MOARkNOBZ MN42** is not your average MIDI controller. This thing used to rock 42 real pots, but now it gets the job done with 3 control pots—one slot value pot and a pair for filter tuning—a bunch of buttons, and enough virtual slots to make your DAW weep.

Forget fragile GUIs and boutique workflows. This beast lives in the guts: built on a Teensy 4.0 MCU, button-bounced, EEPROM-backed, LED-synced firmware for live tweaking, studio sculpting, or performance chaos.

And driving the chaos? Six real-time **envelope followers**, each capable of modulating any control slot based on live input audio or CV (+5V). These EFs don't just track amplitude—they shape it through selectable filters, turning your input into living modulation.

## Directory Layout

- **src/** – core firmware sources.
- **include/** – headers and module docs.
- **test/** – manual and Unity-driven hardware checks.
- **App/** – WebSerial config page.
- **lib/** – vendored Arduino libs that keep the lights on.

## Key Features

- **42 Virtual MIDI Slots**: Store independent CC/channel pairs, slot types, and EF settings.
- **Supports Multiple MIDI Types**: CC, Note, Program Change, Aftertouch, Pitch Bend, NRPN, RPN, and SysEx.
- **Dynamic Envelope Modulation**: Shape CCs using audio input across 6 analog channels.
- **ARG Mode**: Blend/compare signals using programmable logic for creative chaos.
- **Arpeggiator Mode**: Repeats any MIDI slot type in tempo; filter pots set length and pattern.
- **Perlin-Spiced Randomness**: The "random" shape now rides lightweight Perlin noise, giving chaos a groove.
- **Per-EF Filter Selection & Real-Time Tuning**: Each envelope follower can be set to linear, opposite, exponential, random, low-pass, high-pass, or band-pass response. Two dedicated pots allow on-the-fly tuning of filter cutoff (frequency) and resonance (Q).
- **EEPROM Resilience**: Built-in config backup system with a `CONFIG_VERSION` tag and a CRC sniff-test. If the bytes smell wrong, the firmware torches the lot and boots clean.
- **Dual MIDI Output**: DIN blares from boot; USB stays mute until you mash Control Buttons **0+1+2** to arm it.
- **Idle Screensaver**: OLED enters low-power animations after inactivity.
- **Extensible Codebase**: Modular OOP C++ with task scheduler and serial debugging.
- **HTML-Based Editor**: View and update settings via WebSerial (USB).
- **WebSerial Telemetry**: Streams slot values and envelope levels so you can watch every tweak in a browser. See [../docs/WebSerial.md](../docs/WebSerial.md).

## Hardware Redefined

The original idea was simple: 42 knobs (built with inspiration the '60 Knobs' from Bastl Instruments [see link in HISTORY.md](../docs/HISTORY.md). But simplicity is for cowards(! they may be more reasonable, however), so here’s what it became:

* **1 slot pot**: total recall per slot.
* **2 filter-tuning pots**: dial in frequency and resonance.
* **42 virtual MIDI slots**: each one stores its own value, channel, MIDI protocol (CC, note on/off, program change, aftertouch, pitch bend, NRPN, RPN, or SysEx), and envelope interaction settings.
* **A grid of buttons**: short press, long press, combos. The button PCB (`BTN_42`) forms a 7×6 diode matrix read via two CD74HC4067s. Firmware uses a `setMux()` helper to toggle `MUXR1..4`/`MUXC1..4` and scan all 42 buttons through one analog input.
* **OLED Display + Addressable LEDs**: 52 WS2812s throw shade and light—42 for each of the virtual slots, six meter the envelope followers, one blinks at your control-button abuse, and three halo the pots.
* **6 Envelope Followers**: Each with selectable filter modes—**linear, opposite, exponential, random, low-pass, high-pass, or band-pass**—letting you shape how each EF responds to signal dynamics.
* **Live Filter Tuning**: Dedicated pots allow real-time control over frequency and resonance per EF. Sculpt reaction curves on the fly, no DAW needed.

### Hardware Assumptions the Firmware Leans On

Some hardware choices only come alive when the firmware plays along:

- **Internal pull-ups handle the column sense line.** The PCB leaves room for an external resistor, but at this point in the project the code sticks with the MCU's own pull-ups unless we see jitter.
- **Envelope followers baseline themselves.** On boot the firmware samples the mid-rail `VREF` pad and subtracts it so your envelopes start from zero, not from whatever offset the op-amps woke up with.
- **Only one analog read path.** We scan the button and pot multiplexers through a single ADC channel and sort out digital vs. analog thresholds in code—simpler wiring, firmware does the heavy lifting.

### Pin Map

Default pins and timing live in a `HardwareConfig` struct defined in `Globals.h`. Those numbers get loaded at startup and can be punked via the stubbed `include/hardware_config.h` or a tiny `/hardware_config.json` dropped next to the firmware. The table below shows the baked-in defaults.

| Constant | Pin(s) | Purpose |
|---------|-------|---------|
| Field | Pin(s) | Purpose |
|-------|-------|---------|
| `ledPin` | 6 | WS2812 data out (wired to `LED_DATA_PIN`) |
| `muxrPins` | 2,3,4,5 | Row select lines for the button matrix |
| `muxcPins` | 8,9,10,11 | Column select lines for the button matrix |
| `buttonMuxAnalogPin` | A4 | Shared button sense line |
| `potMuxAnalogPin` | A5 | Potentiometer MUX analog input |
| `CONTROL_PINS` | 12,13,14,15,24,25 | Direct control buttons |
| `statusLedPin` | 23 | Board status indicator mounted between the PWR and brain on the board |

Need different pins or scheduler ticks? Override the defaults with a header or drop a JSON sidecar. The repo already ships a no-op `include/hardware_config.h`; wire it up like this to drag the MIDI scheduler:

```cpp
// firmware/include/hardware_config.h
void applyHardwareConfigOverrides(HardwareConfig& cfg) {
    cfg.midiTaskInterval = 2;  // slow the MIDI tick for experiments
}
```

```json
{
  "MIDI_TASK_INTERVAL": 2
}
```

The LED matrix is more hardheaded for a multitude of reasons. FastLED demands its data pin up front, so we hard-code it with `LED_DATA_PIN` via `platformio.ini` (defaults to 6). Want the glow on another GPIO? Change that build flag and rebuild—runtime pin shenanigans are history.

## Firmware Module Cheat Sheet

| Module | What it wrangles |
|-------|------------------|
| [Arpeggiator](include/Arpeggiator/README.md) | Spits out repeating patterns so you can noodle hands‑free. |
| [BiquadFilter](include/BiquadFilter/README.md) | Lightweight filter used by the envelope followers. |
| [ButtonManager](include/ButtonManager/README.md) | Scans the 7×6 button matrix, debounces it, and dishes out events. |
| [ConfigManager](include/ConfigManager/README.md) | Saves to EEPROM, restores from backup when things go sideways. |
| [DisplayManager](include/DisplayManager/README.md) | Talks to the OLED and makes pixels dance. |
| [EnvelopeFollower](include/EnvelopeFollower/README.md) | Converts audio/CV into modulation curves with selectable filters. |
| [LEDManager](include/LEDManager/README.md) | Paints 52 WS2812s and that lone status LED with righteous fury. |
| [MIDIHandler](include/MIDIHandler/README.md) | Speaks MIDI over USB and DIN, mirroring every message. |
| [PotentiometerManager](include/PotentiometerManager/README.md) | Reads the three analog pots and smooths their jittery souls. |
| `Globals` | Shared constants and state that keep the gang in sync. |
| `Utility` | Misc helpers—because even chaos needs some glue. |

```
[Buttons/Pots] -> [Managers] -> [MIDIHandler] -> [USB & DIN]
                   |-> [LEDManager] (bling)
                   |-> [DisplayManager] (OLED)
                   |-> [ConfigManager] (EEPROM)
                   |-> [EnvelopeFollower] (modulation)
```

## Button Mayhem
Buttons are scanned continuously using `setMux()` which sets the row and column addresses before each read.

Need to peek under the hood? `ButtonManager::scanControlInputs()` is fair game for granular debug.
It sniffs the control pots and buttons without dragging the rest of the matrix along for the ride.
Still, the grown-up move is to call `processButtons()` and let it wrangle everything.

Each control button can do several things depending on how you hit it:

| Button | Short Press         | Long Press                    | Double Press                      |
| ------ | ------------------- | ----------------------------- | --------------------------------- |
| #0     | Toggle EF           | Calibrate & save EF baseline  | Cycle EF Filter (forward)         |
| #1     | Next Slot           | Cycle MIDI Type (CC/Note/etc) | Cycle EF Filter (backward)        |
| #2     | Cycle EF assignment | Toggle Slot Active            | —                                 |
| #3     | Cycle MIDI Channel  | Reset EEPROM                  | —                                 |
| #4     | Cycle CC Number     | Save config                   | Reload profile from EEPROM        |
| #5     | Tap BPM             | —                             | —                                 |

**Slot Buttons (0–41):**
- **Short Press:** Pick the slot you want to mangle.
- **Long Press:** Assign or cycle the Envelope Follower for that slot and kick EF ON.
- **Double Press:** Flip that slot’s EF filter to the next flavor.

And yes, combo presses are supported:

| Combo   | Action                                      |
| ------- | ------------------------------------------- |
| #0 + #1 | Cycle EF ARG mode method                    |
| #2 + #3 | Cycle LED light display modes               |
| #4 + #5 | Enable EF and randomize settings            |

*Additional combos implemented in firmware:*

| Combo      | Action                                   |
|------------|------------------------------------------|
| #0 + #1 + #2 | Toggle USB MIDI output                   |
| #0 + #4    | Set slot to MIDI Note mode               |
| #0 + #5    | Set slot to Program Change               |
| #1 + #4    | Set slot to Aftertouch                   |
| #1 + #5    | Set slot to Pitch Bend                   |
| #2 + #4    | Set slot to NRPN                         |
| #0 + #3    | Set slot to SysEx                        |
| #1 + #2    | Toggle MIDI clock output                 |
| #2 + #5    | Cycle ARG envelope pair                  |
| #3 + #4    | Bump arpeggiator base note               |
| #3 + #5    | Toggle Arpeggiator mode                  |
| #0 + #2    | Cycle configuration profiles             |

RPN slots are supported too—assign them via WebSerial or `hardware_config` until a front-panel combo joins the party.

## Profile Controls

Profiles are the controller's second brain. They stash the whole CC+EF circus so you can yank it back mid-set without booting a laptop. Swap from a bass patch to a lead scream on stage, or flip a chill studio layout into a live-wired noise wall in seconds.

- **Save:** Long-press **Control Button #4** to dump the current configuration into EEPROM.
- **Load:** Double-tap **Control Button #4** to resurrect the last saved profile.
- **Cycle:** Mash **Control Buttons #0 and #2** together to hop to the next profile slot when you've hoarded more than one.

Profiles live in EEPROM, so the chaos survives power cycles. Kill the power, plug back in, and you're right where you left off.

### OLED Feedback Cheat Sheet

>Typical screen messages from the firmware’s `DisplayManager` include:
>
>* `Active Slot=<n>` when you select a slot button.
>* `EF: ON/OFF` when toggling envelope following with Control Button #0.
>* `Slot <n> -> EF <m>` when assigning an EF (long press on a slot or short press on Control Button #2).
>* `Slot <n> => <FILTER>` whenever the filter type is changed via double‑press.
>* `Tapped BPM=<value>` after hitting Control Button #5 to set tempo.

>Turning the **main pot** updates the active slot’s value (the OLED keeps showing slot/channels/EF status). Twisting the **filter-tuning pots** pops up a two-line readout with `Freq` and `Q` from `showFilterTuning()` so you can dial in cutoff and resonance.

---

## ARG Mode

### What Is ARG Mode?

ARG (Advanced Relative Gain) mode lets you break free from single-source modulation. Instead of just one audio signal driving an Envelope Follower, ARG lets you **combine or compare two**. It supports 7 expressive modulation algorithms (like `A+B`, `A-B`, `B-A`, `A*B`, etc.) for glitchy, reactive, or chaotic behaviors.

This ain't your mom’s envelope follower.

### How to Activate ARG Mode

You need to already have an **Envelope Follower assigned** to the active slot. To do that:

1. **Press Control Button #0** to toggle EF mode **ON** (green LED will confirm).
2. **Press Control Button #0 + Control Button #1** at the same time to enter **ARG mode** for the assigned EF.

### Cycling ARG Methods

With ARG mode active and an EF already assigned:

* Press **Ctrl #0 + Ctrl #1** again to **cycle through methods**:

  * `PLUS` – A + B
  * `MIN` – A - B
  * `PECK`, `SHAV`, `SQAR`, `BABS`, `TABS` – creative algorithmic transforms and distortions

#### ARG Method Reference

| Method | Formula (A,B) | Description |
| ------ | ------------- | ----------- |
| `PLUS` | `A + B` | Sum of the two envelope levels. |
| `MIN`  | `A - B` | Subtract B from A for a unipolar difference. |
| `PECK` | `B - A` | Invert the subtraction (B minus A). |
| `SHAV` | `(A - B) / 10` | Scaled difference for subtle movement. |
| `SQAR` | `sqrt(A*A + B*B)` | Vector magnitude style blend. |
| `BABS` | `A / abs(B)` | Ratio of A over the absolute of B. |
| `TABS` | `(10 * A) / abs(B)` | BABS with a ×10 boost. |

### Assigning Envelope Pairs for ARG

Once you're in ARG mode:

* Press **Control Button #2 + Control Button #5** together
* This will cycle through all combinations of the 6 envelope inputs (A0, A1, A2, A3, A6, A7)
* Each time, it pairs a new (A, B) set and assigns them to the active EF
* The OLED will display the current pairing: `EF 1: A3/B0`

This allows reactive modulation—i.e., *side-chaining*, *comparative analysis*, or *musical sabotage*—by letting one signal influence another.

### Pro Tips

* LED color will shift in response to filter type + ARG mode
* Use this to chain bass envelope to pad CC, or voice amplitude to delay feedback
* It’s experimental by nature. Push it too far. Then back off just enough to groove.

## Arpeggiator Mode

`Ctrl #3 + Ctrl #5` toggles an arpeggiator for the active slot. It works with
**any MIDI type** (CC, Note, Aftertouch, etc.). While active, the filter knobs
repurpose themselves:

* **Freq Pot** → length of each step (80–800 ms)
* **Q Pot** → selects the pattern (Up, Down, Up&Down, Random)

The arpeggiator repeatedly sends the slot's current value based on control input or EF, according to the selected pattern.
Each tick it grabs the slot's pot value as the root, parks that in `arpNote`, and hammers out `root + offset` for notes.
You can still hijack the base via `setBaseNoteSource()` or a callback if you want an envelope follower or ARG steering the riff.

### Arpeggiator Offsets

The arpeggiator ditched lookup tables. `noteOffset(shape, step, patternLen)` now
calculates the semitone hop for each tick. `patternLen` sets how high the ladder
goes—set it to 4 and you're working with offsets 0–3. Shapes pick the route:
`UP` climbs, `DOWN` dives, `UPDOWN` bounces off the top, and `RANDOM` wanders via
Perlin noise so it remembers where it came from. Example: `patternLen=5` with
`DOWN` spits **4,3,2,1,0** before looping.


## Filter Controls

Each envelope follower features a full DSP filter section with **7 selectable modes**:
- **Linear**
- **Opposite Linear**
- **Exponential**
- **Random**
- **Low-pass (LPF)**
- **High-pass (HPF)**
- **Band-pass (BPF)**

#### Filter Behavior

* **Linear** – direct gain scaling; `Freq` acts as a multiplier.
* **Opposite Linear** – inverts the response so high input yields low output.
* **Exponential** – emphasizes extremes; `Q` controls curve steepness.
* **Random** – introduces jitter based on `Freq` (probability) and `Q` (range).
* **Low-pass** – smooths fast changes; `Freq` is cutoff and `Q` is resonance.
* **High-pass** – emphasizes sharp transients; cutoff and resonance as above.
* **Band-pass** – isolates a band around the chosen frequency with given `Q`.

Switch filter type via double-presses on control buttons. The currently assigned filter is shown on the OLED.

When LPF/HPF/BPF is selected, **two dedicated tuning pots** become active for that EF, letting you adjust:
- **Frequency**: Cutoff/center frequency (20–5000Hz)
- **Resonance (Q)**: 0.5–4.0 (slope/sharpness)
---

## Envelope Follower Calibration

Each follower learns where "silence" lives and dumps that baseline into EEPROM.
On boot those offsets get slurped back so your modulation starts from zero
instead of the hum of your studio fridge. If an offset is missing, the rig
auto-calibrates and saves it.

### Re-calibrating on the fly

- **Per slot:** long‑press Control Button **#0**. The assigned follower sniffs the
  input, writes the new baseline to EEPROM, and the OLED flashes its approval.
- **All at once:** crack open WebSerial and shout `CAL_ENVS`. Every follower
  re-calibrates and the new baselines are burned in.

### Filter Selection Pro Tip

> **To change the filter type** (linear, exponential, low-pass, high-pass, etc.) for a slot’s Envelope Follower, simply:
>
> - **Double-press Control Button #0** to cycle the filter type **forward**.
> - **Double-press Control Button #1** to cycle the filter type **backward**.
>
> The OLED will display the new filter type (e.g., `Slot 5 => BANDPASS`). This works for the EF assigned to the current slot.

---

All changes are visualized in real time on the display.

## LEDs + Display

LED colours follow the states defined in `LEDManager::update()`. The strip now hosts 52 diodes: six gauge the raw strength of each envelope follower, a single control-button sentinel blasts full white for 750 ms then idles at half power for another 1.25 s, and three pot halos mirror the value of the currently selected slot. They provide at-a-glance feedback while you twist and mash buttons:

- **Red** – the currently active pot/slot.
- **Green** – envelope mode enabled for that slot.
- **Blue** – ARG mode is engaged.
- **Yellow** – flashes during MIDI updates.
- **White** – temporary feedback (also used for the startup sweep).

On power‑up the LEDs perform a short white sweep animation and then restore the saved brightness level. Brightness itself is stored in EEPROM and can be tweaked in the firmware.

The OLED shows:
  - Slot info (CC, Channel, Value)
  - EF status and assignment
  - Envelope bars and filter info
  - MIDI messages as they occur
  - Animated fades and idle screensaver after 90s

## Saving and Loading

Your configuration is stored in EEPROM. Manual save required.

* **Button #3 (long press)** nukes your config.
* **Button #4 (long press)** saves the current setup.
* **Button #4 (double press)** reloads the profile from EEPROM.
* A backup copy is also maintained and auto-restored if needed.

## Test Philosophy (and Real Talk)

Some checks need hot solder and a human in the loop; others just need to prove they boot without catching fire.

**Hardware jam sessions.** Hand-rolled test sketches live in `src/` and get flashed with `pio run -e <env>` (the usual suspect is `teensy40_full_system`). They demand real LEDs, real knobs, and a willing operator.

**Unity smoke rituals.** Quick sanity tests camp out in `firmware/test/` and run with `pio test -e teensy40_unity`. They make sure the code still starts up before we plug in anything expensive.

Test sketches used in the development of this project include:

* `mainTEST.cpp`: step-by-step validation of buttons, LEDs, display, and CC slots.
* `unified.cpp`: full integration test—just power it on and watch the magic.
* `test_biquadfilter.cpp`: mathematically verify the BiquadFilter’s low-pass behavior, coefficient updates, and state handling—without any LEDs or buttons—to catch DSP mistakes when the filter “sounds weird.” - this is for the possibly over-caffeinated audio enthusiast who measures weekend fun by how precisely they can fine-tune a low‑pass filter, and whose idea of a thrilling achievement is confirming the Biquad’s coefficients haven’t mysteriously shifted in the night.
* `eeprom_persistence.cpp`: multi-stage exercise that saves a full configuration, forces a reboot, then purposely corrupts the primary EEPROM copy to ensure the backup restore logic works.
* `verify_slots.cpp`: writes known data to every slot, reads it back and prints PASS/FAIL for each one—useful for sanity‑checking EEPROM integrity.

### Configuration Persistence

Every save tacks on a 16-bit `version` tag and a matching 16-bit `crc`.
The version lets the firmware evolve without bricking old configs, while
the CRC sniffs out corruption.  If either check fails on boot, we torch
the junk and fall back to factory defaults.

## MIDI: The Lifeblood

The MN42 is first and foremost a MIDI generator.  Every pot twist and
envelope movement ultimately ends up as a MIDI message that is pushed to
**both** the 5‑pin DIN jack and the USB port at the same time.  The
firmware uses a hardware serial instance for traditional DIN MIDI and the
`usbMIDI` stack for modern computer connections.  Whatever leaves one
interface is mirrored on the other so you can drive hardware synths and a
DAW concurrently with zero configuration.

### MIDI Message Examples

Want to flip patches, squish aftertouch, or yank pitch? Here's how the rig does it:

```cpp
MIDIHandler midi;
midi.begin();
midi.sendProgramChange(10, 1);   // jump to patch 11 on channel 1
midi.sendAftertouch(127, 1);     // mash the key harder than the keyboard ever could
midi.sendPitchBend(2048, 1);     // nudge the note a little sharp
midi.sendNRPN(0x1234, 0x5678, 1); // tweak a deep-cut parameter
midi.sendRPN(0x0001, 0x0040, 1); // spec-sanctioned pitch range tweak
uint8_t dump[] = {0xF0, 0x7D, 0x01, 0x02, 0xF7};
midi.sendSysEx(dump, sizeof(dump)); // sling a bare-bones SysEx
```

Incoming Program Change, Aftertouch, and Pitch Bend now get mirrored over both DIN and USB so the whole chain feels the twist.

### Supported Message Types

Each of the 42 virtual slots can transmit any of the following MIDI
messages, with the channel and data byte stored per slot:

* **Control Change** – standard CC messages with values 0–127.
* **Note** – sends Note On and automatically issues a Note Off shortly
  after, using envelope level (if available) as velocity.
* **Program Change** – select patches or presets on your synths.
* **Channel Aftertouch** – channel pressure values derived from the control pot
  or an envelope follower.
* **Pitch Bend** – full 14‑bit bend range mapped from the control pot.
* **NRPN** – 14-bit Non-Registered Parameter Numbers for secret-sauce controls.
* **RPN** – spec-approved Registered Parameter Numbers for things like pitch range.
* **SysEx** – raw byte dumps for when CCs just won't cut it.

The Control Buttons let you cycle the message type, channel (1–16) and data values in
real time.  All assignments persist in EEPROM -if you save them- so your setup survives a
power cycle.

### Incoming MIDI and Clock Sync

The firmware listens on both USB and DIN. Incoming bytes are parsed and can
trigger on‑screen feedback or internal actions. MIDI Clock messages advance
the beat counter and, when you feel like being the metronome, the box can spit
them back out. Slam Control #1 + #2 to arm or kill clock out. External clock
always rules; if it ghosts you for two seconds, the tapped BPM rises from the
grave and keeps everything stomping in time.

### High‑Resolution Modulation

Envelope followers and the main control potentiometer send updates on a 1 ms
schedule.  CCs or other parameters can therefore react smoothly to audio
input or manual tweaks.  LED animations and the OLED display mirror this
activity so you always see what is being transmitted.

In short, the MN42 speaks fluent MIDI on all fronts—USB and DIN, outgoing
and incoming—and gives every slot the flexibility to send exactly the
messages your rig requires.

## Task Flow & Timing

The firmware juggles work with three cooperative schedulers so the Teensy never misses a beat:

- **High Priority – 1 ms**: MIDI parsing, the internal clock failover, and the arpeggiator's relentless tick.
- **Mid Priority – 5–10 ms**: Serial command digestion and envelope followers tracking incoming audio or CV.
- **Low Priority – 50–100 ms**: LED animations, filter/arp tuning, and OLED refreshes.

The `loop()` function ticks these schedulers in order and then polls buttons and pots every pass. Tasks yield quickly—no preemption, just disciplined cooperation so UI and MIDI stay tight.

> **House Rule:** scheduler callbacks get rounded up before they run. Don't mosh the task list from inside a callback—queue new gigs after `update()` finishes or risk a scheduler bar fight.

=======
## Build It (PlatformIO or Bust)

All builds happen inside this `firmware/` directory. Fire up
PlatformIO like you mean it:

```bash
pio run -e teensy40_main
```

Expected noise in your terminal:

```text
Processing teensy40_main (platform: teensy; board: teensy40; framework: arduino)
...snip...
========================= [SUCCESS] Took XX.XX seconds =========================
```

Craving button gossip over serial? Build with `BUTTON_MANAGER_DEBUG=1` to
unleash verbose ButtonManager logs:

```bash
pio run -e teensy40_main -D BUTTON_MANAGER_DEBUG=1
```

Leave it off and the firmware keeps its mouth shut.

Want to poke the main test rig instead? Use the machine-test sandbox and point it at a test file:

```bash
pio run -e teensy40_full_system
```

Which usually ends with:

```text
Processing teensy40_full_system (platform: teensy; board: teensy40; framework: arduino)

...snip...
========================= [SUCCESS] Took XX.XX seconds =========================
```

Other test flavors are available for deeper debugging:
`teensy40_unified_test`, `teensy40_biquad_test`,
`teensy40_eeprom_persistence`, and `teensy40_slot_verify`.

Run any of them with `pio run -e <env>` and bask in the compile-time glory.

### Calibration & EEPROM sanity check

Envelope followers need to know what "silence" smells like before they can
ride your signal. Do this little dance:

1. Boot with every audio/CV input quiet so the MCU can sniff `VREF`.
2. Map a slot to an envelope follower.
3. While it’s still quiet, mash **Control Button 0**. The follower samples the
   moment, writes the baseline to EEPROM, and flashes `EF Calibrated` for style.
4. Power cycle. Those offsets reload on boot, no questions asked.

Wanna double-check the EEPROM isn’t gaslighting you? Run the persistence test:

- `pio run -e teensy40_eeprom_persistence -t upload`
- **Stage 1**: writes known bytes, then nags you to reset.
- **Stage 2**: verifies the save, trashes the primary header, and asks for one
  more reboot.
- **Stage 3**: loads from the backup and shouts PASS if everything survived.

See three green lights? Your EEPROM is road‑ready.

### Serial MIDI Sniffer

Sometimes you just want to watch the bytes scream. Crack open `platformio.ini` and
uncomment the `MIDI_DEBUG` flag under `build_flags`:

```ini
build_flags =
    -D USB_MIDI_SERIAL
    -D FASTLED_ALLOW_INTERRUPTS=0
    -D LED_DATA_PIN=6
    ; -D MIDI_DEBUG        ; uncomment to spit MIDI logs over Serial
```

Rebuild and the firmware will spew every handled message over the USB Serial
console. Comment it back out when the noise gets old.

### Button Matrix Racket

Need to watch the button grid rat itself out? The `ButtonManager` can shout
every press and release over Serial when you flip on its debug flag. By default
the header keeps things quiet, but you can crank the volume like this:

```ini
build_flags =
    -D USB_MIDI_SERIAL
    -D FASTLED_ALLOW_INTERRUPTS=0
    -D LED_DATA_PIN=6
    -D BUTTON_MANAGER_DEBUG=1  ; unleash the chatter
```

Rebuild and open a serial monitor. The console will scroll with button
state changes so you can chase down flaky switches or just admire the chaos.
Dial it back to `0` when your investigation is over.

### USB Serial & OLED Interface

Once the MN42 is flashed you can jaw at it over USB like it's your favorite
noisy synth. Crack open a terminal with PlatformIO:

```bash
pio device monitor
```

The `monitor_speed` is baked into `platformio.ini` (115200 baud), but any
serial program that speaks 115200‑8‑N‑1 works in a pinch.

Fire commands line‑by‑line:

- `HELLO` – handshake and flip on WebSerial streaming.
- `GET_SCHEMA` – dump the JSON schema describing pots and slots.
- `SET_POT <pot,channel,cc>` – e.g. `SET_POT 0,1,74` maps pot 0 to CC 74 on
  MIDI channel 1.
- `GET_ALL` – spit every pot’s channel/CC plus the LED config.

While the terminal spits data, the buttons drive the OLED menus. Tap a slot
button and it flashes `Active Slot=<n>`. Long‑press the same button to marry an
envelope follower (`Slot <n> -> EF <m>`). Double‑press control buttons to flip
filter types and the screen shouts `Slot <n> => BANDPASS` so you know what just
happened.

Example session:

```text
$ pio device monitor
> HELLO
{"hello":"mn42"}
> SET_POT 0,1,74
Pot configuration updated!
```

While those lines scroll by, the OLED pops `Active Slot=0` and then
`Slot 0 ch1 CC74` before fading back to its idle vibe. Trust the terminal for
truth; use the screen to make sure your button mashing landed where it should.

## Getting Started

1. Plug it in.
2. Use a DAW or synth.
3. Watch LEDs. Twist knob. Push buttons.
4. Reconfigure until satisfied—or mildly horrified. The web editor might help those that seek some simplicity.

## Web Editor

Use the included HTML editor (`benzknobz.html`) in Chrome or Edge:

* Assign CCs visually
* Set envelope pairings
* Tweak filter types, EF settings, and ARG pairings
* Save back to EEPROM over WebSerial

### Filter & ARG WebSerial Commands

The browser flings a couple of plain-text orders over WebSerial and the
firmware salutes:

| Command | What it does |
|---------|--------------|
| `GET_FILTER` | Returns `type,freq,q` for the active envelope filter. |
| `SET_FILTER <type,freq,q>` | Stores filter shape, cutoff and Q into EEPROM. |
| `GET_ARGPAIR` | Spits back the two envelope indices blended in ARG mode. |
| `SET_ARGPAIR <a,b>` | Persists a new envelope follower duo for ARG shenanigans. |

## Development Timeline

Check out the project evolution in the main repo's
[HISTORY.md](../docs/HISTORY.md).

## Support

This isn’t a normal plug-and-play piece gear. It’s for builders, hackers, and those who edit INIs on purpose.

For firmware help: check this repo.

For personal catharsis:
**[support@bseverns.me](mailto:support@bseverns.me)**

## Redistribution

Passing binaries or pre-flashed boards around? Include this directory's `LICENSES/` bundle and point to the EEPROM guts at https://github.com/PaulStoffregen/cores/tree/master/teensy4. That keeps the LGPL-2.1 demons at bay.

Build bold. Tweak louder. Modulate everything.
