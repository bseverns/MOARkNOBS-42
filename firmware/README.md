# MOARkNOBZ MN42 MIDI Controller

> Firmware for the unapologetically DIY, button-stabbin', knob-hackin', MIDI-mashin' controller you didn’t ask for but definitely need.

## What's This?

The **MOARkNOBZ MN42** is not your average MIDI controller. This thing used to rock 42 real pots, but now it gets the job done with a single pot, a bunch of buttons, and enough virtual slots to make your DAW weep.

Forget fragile GUIs and boutique workflows. This beast lives in the guts: hand-coded on a Teensy4.0 MCU, button-bounced, EEPROM-backed, LED-synced firmware for live tweaking, studio sculpting, or performance chaos.

And driving the chaos? Six real-time **envelope followers**, each capable of modulating any CC slot based on live input audio. These EFs don't just track amplitude—they shape it through selectable filters, turning your input into living modulation.

## Hardware Redefined

The original idea was simple: 42 knobs. But simplicity is for cowards. So here’s what it became:

* **1 physical control pot**: total recall per slot.
* **42 virtual CC slots**: each one stores its own value, channel, CC number, and envelope settings.
* **A grid of buttons**: short press, long press, combos, the works.
* **OLED Display + Addressable LEDs**: full visual feedback like a punk rock spaceship control panel.
* **6 Envelope Followers**: Each with selectable filter modes—low-pass, high-pass, or band-pass—letting you shape how each EF responds to signal dynamics.
* **Live Filter Tuning**: Dedicated pots allow real-time control over frequency and resonance per EF. Sculpt reaction curves on the fly, no DAW needed.

## What It Does

* Navigate 42 virtual CC slots.
* Send MIDI CCs over USB and DIN simultaneously.
* Dynamically modulate CCs with audio-driven envelope followers.
* Store/reload settings in EEPROM.
* Tune envelope filters live with two dedicated knobs.

## Test Philosophy (and Real Talk)

We don’t run tests in `/test`. That folder’s dead to us. Our hardware tests live right in `src/` where the real work happens.

Why? Because PlatformIO’s unit test runner is a pain when your test requires poking real LEDs or twisting actual knobs. We write direct test files and compile each one as a standalone firmware. It’s brute-force testing—manual, visual, deliberate.

Test files include:

* `mainTEST.cpp`: step-by-step validation of buttons, LEDs, display, and CC slots.
* `unified.cpp`: full integration test—just power it on and watch the magic.
* `test_biquadfilter.cpp`: for the nerds tuning their DSP coefficients in the dead of night.

## Button Mayhem

Each button can do several things depending on how you hit it:

| Button | Short Press         | Long Press   | Double Press           |
| ------ | ------------------- | ------------ | ---------------------- |
| #0     | Toggle EF           | Assign EF    | Cycle EF Filter (fwd)  |
| #1     | Next Slot           |              | Cycle EF Filter (back) |
| #2     | Cycle EF assignment |              |                        |
| #3     | Cycle MIDI Channel  |              |                        |
| #4     | Cycle CC Number     | Reset EEPROM | Save config            |
| #5     | Tap BPM             |              |                        |

And yes, combo presses are supported:

| Combo   | Action                           |
| ------- | -------------------------------- |
| #0 + #1 | Cycle EF ARG mode method         |
| #2 + #3 | Cycle light modes                |
| #4 + #5 | Enable EF and randomize settings |

## ARG Mode

### What Is ARG Mode?

ARG (Advanced Relative Gain) mode lets you break free from single-source modulation. Instead of just one audio signal driving an Envelope Follower, ARG lets you **combine or compare two**. It supports 7 expressive modulation algorithms (like `A+B`, `A-B`, `B-A`, `A*B`, etc.) for glitchy, reactive, or chaotic behaviors.

This ain't your dad’s envelope follower.

### How to Activate ARG Mode

You need to already have an **Envelope Follower assigned** to the active slot. Then:

1. **Press Control Button #0** to toggle EF mode **ON** (green LED will confirm).
2. **Press Control Button #0 + Control Button #1** at the same time to enter **ARG mode** for the assigned EF.

### Cycling ARG Methods

With ARG mode active and an EF already assigned:

* Press **Ctrl #0 + Ctrl #1** again to **cycle through methods**:

  * `PLUS` – A + B
  * `MIN` – A - B
  * `PECK`, `SHAV`, `SQAR`, `BABS`, `TABS` – creative algorythmic transforms and distortions

Each press shifts to the next method; the OLED will display something like: `EF 2 => SHAV`.

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

## Filter Controls

When EF is on and you’re running LPF/HPF/BPF, the two tuning pots come alive:

* **Freq Pot**: Sets cutoff
* **Resonance Pot**: Sharpens or smooths the effect

Visual feedback is instant. Tweaks are live. Nothing is safe.

## LEDs + Display

* **Red**: Current slot
* **Green**: EF is active
* **Blue**: Configuration mode / ARG wizardry

OLED tells you what slot you’re on, what it’s sending, and what it’s feeling.

## Saving and Loading

Your configuration is stored in EEPROM. Manual save required. Button #4 (long press) handles resets. Button #4 (double press) stores config.

## MIDI: The Lifeblood

* **USB MIDI**: works with anything modern.
* **DIN MIDI**: hardware junkies rejoice.
* **Both at once**: of course.

## Getting Started

1. Plug it in.
2. Use a DAW or synth.
3. Watch LEDs. Twist knob. Push buttons.
4. Reconfigure until satisfied—or mildly horrified.

## Web Editor (Coming Soon)

There’s an HTML-based editor coming to make all of this even more volatile. You'll be able to:

* Assign CCs visually
* Change colors
* Set filters and EF types

## Support

This isn’t plug-and-play consumer gear yet. It’s for builders, hackers, and those who edit INIs on purpose.

For firmware help: check this repo.

For personal catharsis:
**[support@bseverns.me](mailto:support@bseverns.me)**

Build bold. Tweak louder. Modulate everything.
