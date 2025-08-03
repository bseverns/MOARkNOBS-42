# MOARkNOBZ Firmware: Hardware Testing Suite

This project contains a set of low-level, unapologetically manual tests for the MOARkNOBZ firmware. 

These test files are not placed in the conventional /test folder, but directly in the src/ directory. Why? Because we want full control. PlatformIO's test runner is fine for blinking LEDs and clapping for your own test framework, but when you're pushing bytes over MIDI and debugging weird I2C flickers, you need direct access and clean compile filters. Every sketch here flies the `test_*.cpp` flag so the build system knows exactly what mischief you're up to.

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

[env:teensy40_mainTEST]
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

Swap in `test_Unified.cpp`, `test_biquadfilter.cpp`, `test_eeprom_persistence.cpp`, or `test_verify_slots.cpp` depending on what you're shaking down today.

## Final Note

This isn't a test suite for a codebase. It's a test suite for a circuit. If you're not plugging in wires and getting your fingers zapped on that one cap you forgot was charged, you're doing it wrong or I did it wrong, building it for you. This repo is for makers, hackers, educators, and the electrically-inclined misfits who prefer flickering LEDs and buzzers over CI badges.

If you're here, you're one of us. Thank you for looking at a README this deep in the project. Let's test dirty.
