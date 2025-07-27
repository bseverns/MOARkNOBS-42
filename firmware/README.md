# MOARkNOBZ MN42 MIDI Controller

> Firmware for the unapologetically DIY, button-stabbin', knob-hackin', MIDI-mashin' controller you didn’t ask for but definitely need.

*Need a bird's-eye view of the whole project? Scoot up to the repo's [README](../README.md) for hardware notes and overall organization.*

## What's This?

The **MOARkNOBZ MN42** is not your average MIDI controller. This thing used to rock 42 real pots, but now it gets the job done with a 3 control pots, a bunch of buttons, and enough virtual slots to make your DAW weep.

Forget fragile GUIs and boutique workflows. This beast lives in the guts: built on a Teensy 4.0 MCU, button-bounced, EEPROM-backed, LED-synced firmware for live tweaking, studio sculpting, or performance chaos.

And driving the chaos? Six real-time **envelope followers**, each capable of modulating any control slot based on live input audio or CV (+5V). These EFs don't just track amplitude—they shape it through selectable filters, turning your input into living modulation.

## Key Features

- **42 Virtual MIDI Slots**: Store independent CC/channel pairs, slot types, and EF settings.
- **Supports Multiple MIDI Types**: CC, Note, Program Change, Aftertouch, Pitch Bend.
- **Dynamic Envelope Modulation**: Shape CCs using audio input across 6 analog channels.
- **ARG Mode**: Blend/compare signals using programmable logic for creative chaos.
- **Arpeggiator Mode**: Repeats any MIDI slot type in tempo; filter pots set length and pattern.
- **Per-EF Filter Selection & Real-Time Tuning**: Each envelope follower can be set to linear, opposite, exponential, random, low-pass, high-pass, or band-pass response. Two dedicated pots allow on-the-fly tuning of filter cutoff (frequency) and resonance (Q).
- **EEPROM Resilience**: Built-in config backup system with auto-recovery from corruption.
- **Dual MIDI Output**: Send messages via USB and classic 5-pin DIN simultaneously.
- **Idle Screensaver**: OLED enters low-power animations after inactivity.
- **Extensible Codebase**: Modular OOP C++ with task scheduler and serial debugging.
- **HTML-Based Editor**: View and update settings via WebSerial (USB).

## Hardware Redefined

The original idea was simple: 42 knobs (built with inspiration the '60 Knobs' from Bastl Instruments). But simplicity is for cowards(!), so here’s what it became:

* **1 physical control pot**: total recall per slot.
* **2 more physical pots**: for filter tuning.
* **42 virtual CC slots**: each one stores its own value, channel, MIDI protocol (note on/off, CC, prog change, pitch bend, aftertouch), and envelope interaction settings.
* **A grid of buttons**: short press, long press, combos. The button PCB (`BTN_42`) forms a 7×6 diode matrix read via two CD74HC4067s. Firmware uses a `setMux()` helper to toggle `MUXR1..4`/`MUXC1..4` and scan all 42 buttons through one analog input.
* **OLED Display + Addressable LEDs**: full visual feedback like a punk rock spaceship control panel.
* **6 Envelope Followers**: Each with selectable filter modes—**linear, opposite, exponential, random, low-pass, high-pass, or band-pass**—letting you shape how each EF responds to signal dynamics.
* **Live Filter Tuning**: Dedicated pots allow real-time control over frequency and resonance per EF. Sculpt reaction curves on the fly, no DAW needed.

### Pin Map

The constants below come from `Globals.h` and define how the Teensy 4.0 is wired.

| Constant | Pin(s) | Purpose |
|---------|-------|---------|
| `LED_PIN` | 6 | WS2812 data out |
| `MUXR_PINS` | 2,3,4,5 | Row select lines for the button matrix |
| `MUXC_PINS` | 8,9,10,11 | Column select lines for the button matrix |
| `buttonMuxAnalogPin` | A4 | Shared button sense line |
| `potMuxAnalogPin` | A5 | Potentiometer MUX analog input |


## What It Does

* Navigate 42 MIDI slots.
* Dynamically modulate CCs or other MIDI parameters with audio or CV-driven (+5vdc) envelope followers.
* Select filter mode for each envelope follower (with visual OLED feedback).
* Adjust filter frequency and resonance per EF live, using dedicated knobs.
* Store/reload settings in EEPROM (with backup integrity checking).
* Send CCs, notes, pitch bend, aftertouch, and program change via USB and DIN.
* Use a Web Serial editor to configure settings over USB.

## Test Philosophy (and Real Talk)

We don’t run tests in `/test`. That folder’s dead to us. Our hardware tests live right in `src/` where the real work happens.

Why? Because PlatformIO’s unit test runner is a pain when your test requires poking real LEDs or twisting actual knobs. We write direct test files and compile each one as a standalone firmware. It’s brute-force testing—manual, visual, deliberate verification makes the dream work.

Test files used in the development of this project include:

* `mainTEST.cpp`: step-by-step validation of buttons, LEDs, display, and CC slots.
* `unified.cpp`: full integration test—just power it on and watch the magic.
* `test_biquadfilter.cpp`: mathematically verify the BiquadFilter’s low-pass behavior, coefficient updates, and state handling—without any LEDs or buttons—to catch DSP mistakes when the filter “sounds weird.” - this is for the possibly over-caffeinated audio enthusiast who measures weekend fun by how precisely they can fine-tune a low‑pass filter, and whose idea of a thrilling achievement is confirming the Biquad’s coefficients haven’t mysteriously shifted in the night.
* `eeprom_persistence.cpp`: multi-stage exercise that saves a full configuration, forces a reboot, then purposely corrupts the primary EEPROM copy to ensure the backup restore logic works.
* `verify_slots.cpp`: writes known data to every slot, reads it back and prints PASS/FAIL for each one—useful for sanity‑checking EEPROM integrity.

## Button Mayhem
Buttons are scanned continuously using `setMux()` which sets the row and column addresses before each read.

Each control button can do several things depending on how you hit it:

| Button | Short Press         | Long Press                    | Double Press                      |
| ------ | ------------------- | ----------------------------- | --------------------------------- |
| #0     | Toggle EF           | —                             | Cycle EF Filter (forward)         |
| #1     | Next Slot           | Cycle MIDI Type (CC/Note/etc) | Cycle EF Filter (backward)        |
| #2     | Cycle EF assignment | Toggle Slot Active            | —                                 |
| #3     | Cycle MIDI Channel  | —                             | —                                 |
| #4     | Cycle CC Number     | Reset EEPROM                  | Save config                       |
| #5     | Tap BPM             | —                             | —                                 |

**Slot Buttons (0–41):**
- **Long Press:** Assign or cycle the Envelope Follower for that slot and toggle EF ON.

And yes, combo presses are supported:

| Combo   | Action                                      |
| ------- | ------------------------------------------- |
| #0 + #1 | Cycle EF ARG mode method                    |
| #2 + #3 | Cycle LED light display modes               |
| #4 + #5 | Enable EF and randomize settings            |

*Additional combos implemented in firmware:*

| Combo      | Action                                   |
|------------|------------------------------------------|
| #0 + #4    | Set slot to MIDI Note mode               |
| #0 + #5    | Set slot to Program Change               |
| #1 + #4    | Set slot to Aftertouch                   |
| #1 + #5    | Set slot to Pitch Bend                   |
| #2 + #5    | Cycle ARG envelope pair                  |
| #3 + #5    | Toggle Arpeggiator mode                  |


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

LED colours follow the states defined in `LEDManager::update()`. They provide at-a-glance feedback while you twist and mash buttons:

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

* **Button #4 (long press)** resets config.
* **Button #4 (double press)** stores current setup.
* A backup copy is also maintained and auto-restored if needed.

## MIDI: The Lifeblood

The MN42 is first and foremost a MIDI generator.  Every pot twist and
envelope movement ultimately ends up as a MIDI message that is pushed to
**both** the 5‑pin DIN jack and the USB port at the same time.  The
firmware uses a hardware serial instance for traditional DIN MIDI and the
`usbMIDI` stack for modern computer connections.  Whatever leaves one
interface is mirrored on the other so you can drive hardware synths and a
DAW concurrently with zero configuration.

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

The Control Buttons let you cycle the message type, channel (1–16) and data values in
real time.  All assignments persist in EEPROM -if you save them- so your setup survives a
power cycle.

### Incoming MIDI and Clock Sync

The firmware listens on both USB and DIN.  Incoming bytes are parsed and
can trigger on‑screen feedback or internal actions.  MIDI Clock messages
are recognised to advance the internal beat counter.  If no external
clock is seen for two seconds the MN42 falls back to its own tempo based
on the tapped BPM, keeping modulation and display animations in time.

### High‑Resolution Modulation

Envelope followers and the main control potentiometer send updates on a 1 ms
schedule.  CCs or other parameters can therefore react smoothly to audio
input or manual tweaks.  LED animations and the OLED display mirror this
activity so you always see what is being transmitted.

In short, the MN42 speaks fluent MIDI on all fronts—USB and DIN, outgoing
and incoming—and gives every slot the flexibility to send exactly the
messages your rig requires.

## Getting Started

1. Plug it in.
2. Use a DAW or synth.
3. Watch LEDs. Twist knob. Push buttons.
4. Reconfigure until satisfied—or mildly horrified. The web editor might help those that seek some simplicity.

## Web Editor

Use the included HTML editor (`benzknobz.html`) in Chrome or Edge:

* Assign CCs visually
* Set envelope pairings
* Tweak filter types, EF settings
* Save back to EEPROM over WebSerial

## Build Notes

This repo ships with a small patch to the FastLED library.  The file
`src/platforms/arm/mxrt1062/clockless_arm_mxrt1062.h` is stored under
`lib/FastLED` and is pulled in via `platformio.ini`.  PlatformIO will use
this version instead of the default package so the firmware builds without
spurious warnings on Teensy 4.x.

## Development Timeline

Check out the project evolution in the main repo's
[HISTORY.md](../HISTORY.md).

## Support

This isn’t a normal plug-and-play piece gear. It’s for builders, hackers, and those who edit INIs on purpose.

For firmware help: check this repo.

For personal catharsis:  
**[support@bseverns.me](mailto:support@bseverns.me)**

Build bold. Tweak louder. Modulate everything.
