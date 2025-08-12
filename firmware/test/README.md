# MOARkNOBZ Firmware: Hardware Testing Suite

This project doubles as a proving ground and a punching bag. Two flavors of tests keep the firmware honest:

* `src/*_t.cpp` – manual hardware gauntlets you flash and prod like a lab rat.
* `test/test_*.cpp` – the only Unity smoke tests, run under `pio test` when you want receipts without solder burns.

Only the `test/test_*.cpp` crowd plays nice with Unity. Anything in `src/*_t.cpp` won't budge until you upload it to real silicon.

You're in `firmware/test/`; bounce back to [../README.md](../README.md) for the grand tour of the firmware proper.

## Unity Output

`unity_config.cpp` is the trash-talking megaphone that lets Unity scream over Serial when tests run on real iron. The test rig
`[env:teensy40_unity]` flips on `UNITY_INCLUDE_CONFIG_H`, so that file rides along automatically—no extra `build_src_filter`
dance.

`UNITY_OUTPUT_START()` boots the chatter at 115200 on `Serial1` by default. If your hardware grooves at some other rate, crack
open `unity_config.h` and remix the macro or call `unityTestStart()` with your own tempo.

`unittest_transport.cpp` rides shotgun, providing the UART hooks PlatformIO's custom test transport expects. Both files
lean on the same wrappers so every rant that Unity spits makes it back to the host. Change one without the other and you'll be
debugging in the dark.

## Hardware Hit List

| File | What it beats on |
|------|------------------|
| `src/main_t.cpp` | LEDs, button matrix, slot pot, envelope followers, OLED display |
| `src/unified_t.cpp` | Whole rig at once: LEDs, buttons, pots, envelopes, display, config |
| `src/biquadfilter_t.cpp` | BiquadFilter DSP math only |
| `src/eeprom_persistence_t.cpp` | EEPROM, ConfigManager, slots, display, buttons, pots, envelopes, LEDs |
| `src/verify_slots_t.cpp` | MIDISlots and EEPROM integrity |
| `test/test_led_manager.cpp` | LEDManager brightness and colour knobs |
| `test/test_button_manager.cpp` | ButtonManager long‑press timing |
| `test/test_potentiometer_manager.cpp` | Pot channel + CC mapping |
| `test/test_display_manager.cpp` | DisplayManager update throttle |
| `test/test_envelope_follower.cpp` | EnvelopeFollower low‑pass/high‑pass flip |
| `test/test_config_manager.cpp` | ConfigManager EEPROM recovery |
| `test/test_midi_handler.cpp` | MIDIHandler program/aftertouch/pitch bend/NRPN/SysEx routing – fakes the pipes, so keep it off hardware |
| `test/test_arpeggiator.cpp` | Arpeggiator start/stop sanity |
| `test/test_biquad_filter.cpp` | BiquadFilter low‑pass vs high‑pass math |

### Picking the right test

Say the OLED ghosts you mid-jam:

1. Scan the table and spot `test/test_display_manager.cpp`.
2. Blast the whole Unity suite: `pio test -e teensy40_unity`. `test/test_mainUnity.cpp` will herd every test, including the display check. Want just one? comment out the `RUN_TEST` lines you don't care about and rerun.
3. If Unity shrugs, flash `src/main_t.cpp` (`pio run -e teensy40_full_system -t upload`) and watch the screen dance.
4. Still blank? time to chase solder joints.

### Shared helpers

`TestHelpers.cpp` anchors the control-button matrix in one spot so every test riffs from the same pin map. Include `TestHelpers.h` and you're good to shred without duplicating arrays.

## Manual machine tests (`src/*_t.cpp`)

When you need to stare the hardware in the face, grab a `src/*_t.cpp` sketch and drive it yourself.

Example: run the unified gauntlet and see what smokes first.

1. `cd firmware`
2. `pio run -e teensy40_unified_test -t upload`
3. `pio device monitor`
4. Mash buttons, twist pots, and read the OLED like a fortune teller. Fix whatever flinches.

## Now with Unity smoke tests

We finally caved and wired up a few automated checks in `test/test_*.cpp` for those lonely nights when you want proof without solder burns. Anything in `src/*_t.cpp` still demands real hardware and a steady trigger finger.

```bash
pio test -e teensy40_unity
```

That Unity env automatically defines `UNIT_TEST` *and* `USB_MIDI_STUB`. When those flags fly, `test/USB-MIDI.cpp` and its header hijack the usual Teensy globals and fake out `MIDI` and `usbMIDI`. Real hardware builds leave `USB_MIDI_STUB` undefined, so `MIDIHandler.cpp` skips the faux pipes and the legit USB stack owns the symbols—no linker brawls, no ghosts.

### Serial? fake it.

Vendor libs love to yammer over `Serial` even when there's no UART in sight. The host test rig doesn't drag in the Arduino core, so we slap down a bare-bones `SerialStub` in `test/SerialStub.h` and force it into every Unity compile via `-include`. It's just enough to keep `Adafruit BusIO` and friends from choking while the real hardware sleeps.

Unity is fussy and demands a `unity_config.h` to map its battle cries.
There's a lean version sitting in `../include/` that just sprays bytes
over `Serial`. If you need different output, crack that file open and
remix the macros.

That env sets `test_build_src = true`, so PlatformIO drags the project's core sources into the test build. If something in `src/`
won't compile, Unity will scream before you ever flash a board.

### test_envelope_follower.cpp
Snaps the EnvelopeFollower between low-pass and high-pass to make sure DC gets gutted on command.

### test_config_manager.cpp
Corrupts EEPROM headers on purpose and checks that the backup block rides to the rescue.

### test_button_manager.cpp
Fakes time itself to ensure long presses don't fire until 500ms has actually passed.

### test_led_manager.cpp
Makes sure the LED engine listens when we bark new brightness or colour orders.

### test_potentiometer_manager.cpp
Checks that channel and CC mapping stick for the first slot pot.

### test_display_manager.cpp
Pokes the update interval to prove the UI can chill when told.

### test_midi_handler.cpp

Shoots fake MIDI through stubbed veins to make sure routing doesn't flake out. The USB-MIDI impostors live in this folder and disappear on real silicon, so `teensy40_unified_test` leaves this test on the bench. Only the `teensy40_unity` rig builds with `UNIT_TEST` to conjure those impostors; every other env skips the flag, so anything leaning on the stub just naps.

The stub's `MidiType` enum mirrors the real deal. The opener is `SystemExclusive`, the curtain drop is `EndOfExclusive`, and a freshly-minted `Tick` rides 0xF8 so you can count the beat without pulling in the whole clock rig. If your tests still shout the old names, patch 'em—yesterday's API won't save today's jam.

### test_arpeggiator.cpp
Starts the riff machine, stops it, and double-checks it grabbed the right slot.

### test_biquad_filter.cpp
Runs a quick low-pass vs high-pass duel to catch any rogue DSP math.

## File Descriptions

### main_t.cpp

Location: src/main_t.cpp

#### Purpose-built to verify all major subsystems individually:

LEDManager: one LED at a time, now including the new EF meters, control beacon and pot halos

ButtonManager: tests both the matrix-multiplexed buttons and the direct-wired control buttons

PotentiometerManager: sweeps the lone slot pot; filter knobs get read via ButtonManager

EnvelopeFollower: confirms dynamic envelope response

DisplayManager: shows static test data on all 3 lines

Run this and check it with your eyes. No automation. Human-in-the-loop sanity checks, every time.

Interaction: Step manually by hitting Enter in the serial monitor between stages.

### unified_t.cpp

Location: src/unified_t.cpp

#### This is the integration stress-test. All systems together, reacting to physical input. No waiting for user input via serial; it uses the actual button matrix for flow control. If something doesn't light up, react, or show data, you know exactly where to poke.

Used for field validation, QA benches, and righteous debugging rage.

Interaction: Uses real button presses (not keyboard input). Designed to be used with the assembled controller.

### biquadfilter_t.cpp

Location: src/biquadfilter_t.cpp

#### Tests the digital signal processing side of things. No LEDs. No buttons. Just math:

Verifies BiquadFilter's behavior for low-pass filters

Confirms correct coefficient updates and state handling

Useful for catching dumb mistakes in your DSP brain

Run this when your filter "sounds weird" and you're sure the hardware is fine.

### eeprom_persistence_t.cpp

Location: src/eeprom_persistence_t.cpp

#### Checks that configuration data survives a power cycle and that the backup EEPROM region can resurrect corrupted settings.

Expect to reboot the board a couple of times and watch the display for prompts. It's manual, messy, and exactly why this suite exists.

### verify_slots_t.cpp

Location: src/verify_slots_t.cpp

#### Blasts known values into every MIDISlot and slurps them back out to make sure EEPROM isn't lying to you.

Output scrolls by on Serial with PASS/FAIL verdicts. Trust, but verify.

## How to Build a Test

Each test is wired to its own PlatformIO environment in platformio.ini. The trick is to explicitly define which files you want to include. Here's an example for building `main_t.cpp`:

[env:teensy40_full_system]
extends = env:teensy40_base
build_src_filter =
    +<**/main_t.cpp>
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

Swap in `unified_t.cpp`, `biquadfilter_t.cpp`, `eeprom_persistence_t.cpp`, or `verify_slots_t.cpp` depending on what you're shaking down today.

## Add Your Own Test

Got a new stunt that isn't in the roster? Spin up your own environment:

```ini
[env:your_new_test]
extends = env:teensy40_base
build_src_filter = +<**/your_new_test.cpp>
test_build_src = true
```

`build_src_filter` is your whitelist—only sources tagged with a `+` get invited to the party. Flip `test_build_src` to haul in everything under `src/` so Unity hammers the real firmware, not some cardboard cutout.

For a full-blown example, raid [platformio.ini](../platformio.ini) and riff off an existing `env:` block.

## Final Note

This isn't a test suite for a codebase. It's a test suite for a circuit. If you're not plugging in wires and getting your fingers zapped on that one cap you forgot was charged, you're doing it wrong or I did it wrong, building it for you. This repo is for makers, hackers, educators, and the electrically-inclined misfits who prefer flickering LEDs and buzzers over CI badges.

If you're here, you're one of us. Thank you for looking at a README this deep in the project. Let's test dirty.
