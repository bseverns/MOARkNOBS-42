# MOARkNOBZ MN42 MIDI Controller

> Firmware for the unapologetically DIY, button-stabbin', knob-hackin', MIDI-mashin' controller you didn’t ask for but definitely need.

## What's This?

The **MOARkNOBZ MN42** is not your average MIDI controller. This thing used to rock 42 real pots, but now it gets the job done with a single pot, a bunch of buttons, and enough virtual slots to make your DAW weep.

Forget fragile GUIs and boutique workflows. This beast lives in the guts: hand-coded, button-bounced, EEPROM-backed, LED-synced firmware for live tweaking, studio sculpting, or performance chaos.

## Hardware Redefined

The original idea was simple: 42 knobs. But simplicity is for cowards. So here’s what it became:

* **1 physical pot**: total recall per slot.
* **42 virtual CC slots**: each one stores its own value, channel, CC number, and envelope settings.
* **A grid of buttons**: short press, long press, combos, the works.
* **OLED Display + Addressable LEDs**: full visual feedback like a punk rock spaceship control panel.

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
