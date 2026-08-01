# MOARkNOBZ Firmware: Hardware Testing Suite

This project doubles as a proving ground and a punching bag. Two flavors of tests keep the firmware honest:

* `src/*_t.cpp` – manual hardware gauntlets you flash and prod like a lab rat.
* `test/test_*.cpp` – the only Unity smoke tests, run under `pio test` when you want receipts without solder burns.

Only the `test/test_*.cpp` crowd plays nice with Unity. Anything in `src/*_t.cpp` won't budge until you upload it to real silicon.

You're in `firmware/test/`; bounce back to [../README.md](../README.md) for the grand tour of the firmware proper.

## Unity Output

`unity_output.cpp` is the trash‑talking megaphone that lets Unity scream over `Serial1` when tests run on real iron. The test
rig `[env:teensy40_unity]` flips on `UNITY_INCLUDE_CONFIG_H`, so `unity_config.h` wires Unity's macros straight into those
hooks—no extra `build_src_filter` dance.

`UNITY_OUTPUT_START()` boots the chatter at 115200 on `Serial1` by default. If your hardware grooves at some other rate, crack
open `unity_config.h` and remix the macro.

`GlobalsStub.cpp` seeds `g_tappedBPM` plus the other globals the unit suite
needs, so the linker chills without hauling in `Globals.cpp` and its SD baggage.

## Custom Runner

PlatformIO's stock Unity runner speaks over the default `Serial` port, which is useless for this rig. Our tests scream down
`Serial1`, so we roll a tiny Python runner that just parses Unity's output and calls it a day.

To take it for a spin from repo root:

```bash
pio test -d firmware -e teensy40_unity -vvv
```

Or, if you're already inside `firmware/`:

```bash
pio test -e teensy40_unity -vvv
```

Drop new flashing logic into `stage_uploading()` if your CI needs to lob firmware at some remote hardware. The rest of the
runner is pure line parsing, so hack away.

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
| `test/demo_button_ef_usb_midi.cpp` | Punk-simple USB MIDI hardware demo: one button, one envelope follower |

### Picking the right test

Say the OLED ghosts you mid-jam:

1. Scan the table and spot `test/test_display_manager.cpp`.
2. Blast the whole Unity suite: `pio test -d firmware -e teensy40_unity -vvv` (or `pio test -e teensy40_unity -vvv` from `firmware/`). `test/test_mainUnity.cpp` will herd every test, including the display check. Want just one? comment out the `RUN_TEST` lines you don't care about and rerun.
3. If Unity shrugs, flash `src/main_t.cpp` (`pio run -e teensy40_full_system -t upload`) and watch the screen dance.
4. Still blank? time to chase solder joints.

### Shared helpers

`TestHelpers.cpp` anchors the control-button matrix in one spot so every test riffs from the same pin map. Include `TestHelpers.h` and you're good to shred without duplicating arrays.

#### Hardware I/O fakes with a safety switch

`Hardware/IO.h` is the new patch bay for `analogRead()` and `digitalRead()`. The default passthrough still hits the real ADC/GPIO on hardware, but tests can hijack the lines with `hardware::setAnalogReadProvider()`/`setDigitalReadProvider()` or the friendlier `Scoped…` wrappers. Feed it a lambda, spill out whatever fake voltage or button state you need, and Unity will believe you. Check `test/test_hardware_io.cpp` for the full recipe, including a tiny sequence generator that loops canned ADC readings while ButtonManager chews through its matrix. Use it to slam extreme values, bounce simulations, or timing edge cases without touching copper.

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
pio test -d firmware -e teensy40_unity -vvv
```

### Native-host seam

`native_biquad` is the first hardware-free firmware lane. It tests the
Arduino-independent `BiquadFilter` coefficient and response behavior on the
CI host:

```bash
pio test -d firmware -e native_biquad -vvv
```

`native_transport` executes the Arduino-independent modulation token-bucket
policy on the CI host, including cold-reset timing, refill/capacity behavior,
32-bit clock wraparound, Note priority, rotating fairness, and failed-attempt
token preservation:

```bash
pio test -d firmware -e native_transport -vvv
```

`native_modulation` executes slot-value composition without the Arduino HAL,
including EF/LFO ordering, all lane modes, clamping, sanitization, and the
legacy replacement curve:

```bash
pio test -d firmware -e native_modulation -vvv
```

`native_persistence` exercises the extracted storage-map and migration-layout
arithmetic plus profile-modulation packing, sanitization, and CRC semantics:

```bash
pio test -d firmware -e native_persistence -vvv
```

It complements, rather than replaces, the Teensy Unity/HIL environment. New
native tests should stay behind a deliberate portable boundary; do not pull
Arduino, display, SD, or USB transport dependencies into this lane.

 The `teensy40_unity` rig only flips on `UNIT_TEST`. If you also define `USB_MIDI_STUB`, `test/usb_midi.cpp` and pals hijack the usual Teensy globals and fake out `MIDI` and `usbMIDI`. Instead of playing macro shell games, we drop in a skinny `usb_midi_class` that exposes the same face as the real deal. Any code shouting for `usbMIDI` ends up talking to our stub, the core header never loads, and the linker goes back to sleep. A tiny `MIDI.h` shim rides shotgun so the `midi` namespace exists even when the heavyweight library sits out. Hardware builds leave that flag off so `MIDIHandler.cpp` sticks with the legit USB stack—no linker brawls, no ghosts.

  Tests never include `<Arduino.h>` directly anymore. Drop `#include "unity_config.h"` at the top and it drags in Arduino and our usbMIDI doppelganger. The `teensy40_unity` env rewrites the core's `usbMIDI` symbols at build time so the stub keeps center stage and the real header stays buried. Skip the shim and the compiler will howl about dueling `usb_midi_class` defs.

### Serial? fake it.

Vendor libs love to yammer over `Serial` even when there's no UART in sight. Host-side builds don't drag in the Arduino core, so `test/SerialStub.h` fakes just enough of `Print` and `HardwareSerial` to keep `Adafruit BusIO` and friends from choking. When you compile for a Teensy, the stub backs off and the real UARTs take over.

Unity is fussy and demands a `unity_config.h` to map its battle cries.
There's a lean version in `test/unity_config.h` that just sprays bytes
over `Serial1`. If you need different output, crack that file open and
remix the macros.

That env sets `test_build_src = true`, but we keep the haul lean with a `build_src_filter`. Only the bits we actually test hitch a ride—`test_mainUnity.cpp`, `Arpeggiator.cpp`, `MIDIHandler.cpp`, plus `GlobalsStub.cpp` and `Utility.cpp` so the clock math and tapped BPM global keep time. `DisplayManager::registerInteraction()` gets faked in `test/DisplayManagerStub.cpp` so the UI stays out of the link step. If anything in that pile won't compile, Unity will scream before you ever flash a board.

### VS Code's flaky build staging

If VS Code yells about `firmware/test/DisplayManagerStub.cpp` and a missing `.sconsign311.dblite`, that's SCons trying to log its dependency cache before PlatformIO finished birthing the `.pio/build/<env>` directory. We now strong-arm that folder into existence in `firmware/scripts/deprecated_copy_flag.py` (peek [`firmware/scripts/README.md`](../scripts/README.md) for the gritty details), but if you see the error again, nuke `.pio/` and rerun `pio test -d firmware -e teensy40_unity -vvv` so the script can repave the path.

### test_envelope_follower.cpp
Snaps the EnvelopeFollower between low-pass and high-pass to make sure DC gets gutted on command.

### test_config_manager.cpp
Corrupts EEPROM headers on purpose and checks that the backup block rides to the rescue.

### test_button_manager.cpp
Fakes time itself to ensure long presses don't fire until 500ms has actually passed and that a follow-up tap is needed to seal the deal.

### test_led_manager.cpp
Makes sure the LED engine listens when we bark new brightness or colour orders.

### test_potentiometer_manager.cpp
Checks that channel and CC mapping stick for the first slot pot.

### test_display_manager.cpp
Pokes the update interval to prove the UI can chill when told.

### test_midi_handler.cpp

Shoots fake MIDI through stubbed veins to make sure routing doesn't flake out. The usb_midi impostors live in this folder and disappear on real silicon, so `teensy40_unified_test` leaves this test on the bench. Only the `teensy40_unity` rig builds with `UNIT_TEST` to conjure those impostors; every other env skips the flag, so anything leaning on the stub just naps.

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

### demo_button_ef_usb_midi.cpp

Location: test/demo_button_ef_usb_midi.cpp

This is the "show it to the synth club" sketch. It ditches the whole firmware stack and just wires a single control button (pin 12) and one envelope follower input (A0) straight into USB MIDI. Flash it when you want to prove the analog front-end works without hauling the full UI along.

Upload it like so:

```bash
pio run -d firmware -e teensy40_button_ef_demo -t upload
```

What you get once it boots:

* Serial spits a quick setup spiel while it samples a noise-floor baseline.
* Button presses blast middle C on channel 1.
* The envelope jack maps to MIDI CC 21 with a fast smoothing tail. Wiggle your source and watch MIDI Monitor light up.

Pro tip: keep the rig quiet for a hot second after reset so the baseline stays classy. After that, mash away.

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

Expect to reboot the board a couple of times and watch the display for prompts. The sketch now hollers over both `Serial` and `Serial1`, so pick your poison. It's manual, messy, and exactly why this suite exists.

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
