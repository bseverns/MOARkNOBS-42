# MOARkNOBZ Firmware: Hardware Testing Suite

This project doubles as a proving ground and a punching bag. Two flavors of tests keep the firmware honest:

* `src/test_*.cpp` – full-system, manual machine tests you flash and prod like a lab rat.
* `test/test_*.cpp` – Unity smoke tests that run under `pio test` when you want receipts without solder burns.

You're in `firmware/test/`; bounce back to [../README.md](../README.md) for the grand tour of the firmware proper.
 
Two breeds of tests haunt this folder:

* `src/test_*.cpp` – dirt-under-the-nails sketches. Flash one, plug in the board, and mash buttons until it screams.
* `test/test_*.cpp` – buttoned-up Unity checks that run on your desk before you risk real hardware.

The hardware tests live under `src/` because we want full control. PlatformIO's test runner is fine for blinking LEDs and clapping for your own framework, but when you're pushing bytes over MIDI and chasing I2C ghosts, you need clean compile filters. Every sketch here flies the `test_*.cpp` flag so the build system knows exactly what mischief you're up to.

### Shared helpers

`TestHelpers.cpp` anchors the control-button matrix in one spot so every test riffs from the same pin map. Include `TestHelpers.h` and you're good to shred without duplicating arrays.

## Manual machine tests (`src/test_*.cpp`)

When you need to stare the hardware in the face, grab a `src/test_*.cpp` sketch and drive it yourself.

Example: run the unified gauntlet and see what smokes first.

1. `cd firmware`
2. `pio run -e teensy40_unified_test -t upload`
3. `pio device monitor`
4. Mash buttons, twist pots, and read the OLED like a fortune teller. Fix whatever flinches.

## Now with Unity smoke tests

We finally caved and wired up a few automated checks in `test/` for those lonely nights when you want proof without solder burns.

Quick-start:

1. `cd firmware`
2. `pio test -e teensy40_mainTEST`
3. Watch for PASS/FAIL and let the CI bots cry later.

### PlatformIO test environments

`platformio.ini` ships a roster of playgrounds so each test stays in its lane:

- `teensy40_mainTEST` – baseline harness; doubles as the Unity testbed.
- `teensy40_unified_test` – integration cage match.
- `teensy40_biquad_test` – DSP sanity check.
- `teensy40_eeprom_persistence` – power-cycle endurance trial.
- `teensy40_slot_verify` – EEPROM truth serum.

That `teensy40_unity` target keeps things virtual—compile, run, and bail out before you melt anything.

### test_envelope_follower.cpp
Snaps the EnvelopeFollower between low-pass and high-pass to make sure DC gets gutted on command.

### test_config_manager.cpp
Corrupts EEPROM headers on purpose and checks that the backup block rides to the rescue.

### test_button_manager.cpp
Fakes time itself to ensure long presses don't fire until 500ms has actually passed.

## File Descriptions

### test_main.cpp

Location: src/test_main.cpp

#### Purpose-built to verify all major subsystems individually:

LEDManager: one LED at a time, now including the new EF meters, control beacon and pot halos

ButtonManager: tests both the matrix-multiplexed buttons and the direct-wired control buttons

PotentiometerManager: sweeps the lone slot pot; filter knobs get read via ButtonManager

EnvelopeFollower: confirms dynamic envelope response

DisplayManager: shows static test data on all 3 lines

Run this and check it with your eyes. No automation. Human-in-the-loop sanity checks, every time.

Interaction: Step manually by hitting Enter in the serial monitor between stages.

### test_Unified.cpp

Location: src/test_Unified.cpp

#### This is the integration stress-test. All systems together, reacting to physical input. No waiting for user input via serial; it uses the actual button matrix for flow control. If something doesn't light up, react, or show data, you know exactly where to poke.

Used for field validation, QA benches, and righteous debugging rage.

Interaction: Uses real button presses (not keyboard input). Designed to be used with the assembled controller.

### test_biquadfilter.cpp

Location: src/test_biquadfilter.cpp

#### Tests the digital signal processing side of things. No LEDs. No buttons. Just math:

Verifies BiquadFilter's behavior for low-pass filters

Confirms correct coefficient updates and state handling

Useful for catching dumb mistakes in your DSP brain

Run this when your filter "sounds weird" and you're sure the hardware is fine.

### test_eeprom_persistence.cpp

Location: src/test_eeprom_persistence.cpp

#### Checks that configuration data survives a power cycle and that the backup EEPROM region can resurrect corrupted settings.

Expect to reboot the board a couple of times and watch the display for prompts. It's manual, messy, and exactly why this suite exists.

### test_verify_slots.cpp

Location: src/test_verify_slots.cpp

#### Blasts known values into every MIDISlot and slurps them back out to make sure EEPROM isn't lying to you.

Output scrolls by on Serial with PASS/FAIL verdicts. Trust, but verify.

## How to Build a Test

Each test is wired to its own PlatformIO environment in platformio.ini. The trick is to explicitly define which files you want to include. Here's an example for building `test_main.cpp`:

[env:teensy40_full_system]
extends = env:teensy40_base
build_src_filter =
    +<**/test_main.cpp>
    +<**/ButtonManager.cpp>
    +<**/ConfigManager.cpp>
    +<**/DisplayManager.cpp>
    +<**/EnvelopeFollower.cpp>
    +<**/LEDManager.cpp>
    +<**/MIDIHandler.cpp>
    +<**/PotentiometerManager.cpp>
    +<**/Utility.cpp>
    +<include/**.h>
    -<**/firmware_main.cpp>

Flash it with:

```bash
pio run -e teensy40_full_system -t upload
```

That `teensy40_full_system` build shoves the whole circus onto the board so you can poke every subsystem live.

Swap in `test_Unified.cpp`, `test_biquadfilter.cpp`, `test_eeprom_persistence.cpp`, or `test_verify_slots.cpp` depending on what you're shaking down today.

## Final Note

This isn't a test suite for a codebase. It's a test suite for a circuit. If you're not plugging in wires and getting your fingers zapped on that one cap you forgot was charged, you're doing it wrong or I did it wrong, building it for you. This repo is for makers, hackers, educators, and the electrically-inclined misfits who prefer flickering LEDs and buzzers over CI badges.

If you're here, you're one of us. Thank you for looking at a README this deep in the project. Let's test dirty.
