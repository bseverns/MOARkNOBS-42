# Testing the Beast

This repo runs tests in layers, from polite unit checks to full-on hardware cage matches. Every layer has a job: make sure the firmware math is sane, prove the bridge still talks, and keep you from hauling a broken controller to a gig. Read this as a teaching map, not just a checklist—if you know why a test exists, you know when to lean on it.

## Layer cheat sheet

| Layer | Command | Hardware needed | What it proves |
|-------|---------|-----------------|----------------|
| Full battery | `./test.sh` | Teensy 4.0 for Unity, host machine for Node | Runs everything below, writes clean logs, perfect for CI and pre-commit rituals. |
| Unity smoke tests | `pio test -e teensy40_unity` | Teensy 4.0 with USB cable | Exercises firmware logic with Unity harness and Serial1 shim. |
| Manual firmware sketches | `pio run -e teensy40_unified_test -t upload` (and friends) | Fully assembled controller | Human-driven end-to-end testing of LEDs, pots, EEPROM, etc. |
| Bridge CLI sanity | `npm --prefix bridge test` | Host machine only (Node ≥ 18) | Keeps the OSC bridge CLI parsing and error handling sharp. |
| Future black-box trials | `system_test/*` (coming soon) | Controller + bridge talking | Automated “kick the hardware” flows when the scripts land. |

## Preflight kit

Before you run anything, make sure you actually have the tools:

- **PlatformIO Core 6+** in your `$PATH`. We call `pio` directly.
- **Node.js 18 or newer** plus npm for the bridge sanity checks.
- **A Teensy 4.0 wired up** with a USB cable that actually carries data.
- **Dialed-in udev/serial permissions** so `/dev/ttyACM*` or `/dev/cu.usbmodem*` is readable.
- **A `logs/` directory**—`test.sh` creates it, but manual runs should too if you want artifacts.
- Optional but smart: put the controller on a powered hub so repeated flashes don’t brown it out.

## Quick hit: `test.sh`

If you want everything in one punch, run the orchestrator from the repo root:

```bash
./test.sh
```

### What it orchestrates

- Sweeps the build tree (`pio run -d firmware -t clean`) so Unity tests start fresh.
- Attempts to auto-detect a Teensy serial port, unless you yell one through `TEST_PORT`.
- Runs the Unity suite **without re-uploading firmware** (`--without-uploading`). That means you should have flashed `teensy40_unity` at least once beforehand, or the board will still be running whatever you last put on it.
- Greps the Unity output to make sure PlatformIO didn’t sneak in its own transport shim.
- Launches the Node-based bridge tests via `npm --prefix bridge test`.

### Dependencies

- PlatformIO must be installed with the Teensy platform and custom Unity runner available. The script bails on missing executables.
- `bridge/node_modules` should exist. If not, run `npm install` in `bridge/` once before relying on the script.
- Teensy board with the Unity test firmware already on it if you expect hardware tests to actually run; otherwise the script will log that Unity was skipped.

### Port juggling

The script looks for the first match among `/dev/ttyACM*`, `/dev/ttyUSB*`, `/dev/cu.usbmodem*`, and `/dev/cu.usbserial*`. Override it if you have multiple boards plugged in:

```bash
TEST_PORT=/dev/ttyACM1 ./test.sh
```

You can also export `TEST_PORT` once in your shell profile if that board never moves.

### Log haul

After a successful swing you’ll have:

- `logs/unity-test.log` – raw Unity chatter. Look for `[==========]` headers and a final `OK`.
- `logs/unity-test.xml` – JUnit XML, perfect for CI badge voodoo.
- `logs/bridge-test.log` – npm’s test output, including stderr if the bridge freaks out.

### When things explode

- **“Skipping Unity tests”** – no Teensy port was found. Flash the board, double-check cables, or pass `TEST_PORT` explicitly.
- **“Autogen Unity transport detected”** – PlatformIO regenerated its default Unity transport. Clean your `.pio` tree (`pio run -d firmware -t clean`) and make sure you didn’t delete `firmware/test/unity_output.cpp` or `firmware/test/unittest_transport.cpp`.
- **Unity timeouts** – usually the board wasn’t flashed with the Unity firmware. Run `pio test -e teensy40_unity` without `--without-uploading` once to seed it.
- **Bridge test failures** – run `npm --prefix bridge test -- --watch` locally and fix whatever CLI regression the suite is screaming about.

```mermaid
flowchart TD
  T["`test.sh`"] --> U["Unity tests\n(pio test -e teensy40_unity)"]
  T --> B["Bridge checks\n(npm test)"]
  U --> UL["logs/unity-test.xml\nlogs/unity-test.log"]
  B --> BL["logs/bridge-test.log"]
```
*Alt text: Flowchart of `test.sh` hammering Unity firmware tests and bridge JS checks, each spraying their own log files.*

## Unity smoke tests (`firmware/test/`)

Unity tests are the quick-and-dirty pulse check for the firmware. They run on the Teensy board and stub out anything that would otherwise demand real wires. Fire them up when you’ve tweaked core logic or want receipts before you solder.

### First-time setup

1. Plug the Teensy in and make sure `pio device list` shows the port you expect.
2. From the repo root: `cd firmware`.
3. Seed the board with the Unity runner so `test.sh` has something to talk to:
   ```bash
   pio test -e teensy40_unity
   ```
   This compiles, uploads, and executes the suite in one shot. You’ll see Unity scroll by in your terminal. Subsequent invocations via `test.sh` can skip uploading.

The `teensy40_unity` environment speaks over `Serial1` at 115200 baud and leans on the custom `unity_output.cpp` + `unity_config.h` duo in `firmware/test/`. If you need to route output elsewhere (e.g., different UART pins), edit those files—not the PlatformIO defaults.

### Running by hand after setup

Once the firmware is already flashed, you can grab fresh logs without re-uploading:

```bash
cd firmware
pio test -e teensy40_unity --without-uploading --test-port /dev/ttyACM0 -vvv --junit-output ../logs/unity-test.xml | tee ../logs/unity-test.log
```

### Reading the results

- Passing runs finish with `OK` and per-test `PASS` tags.
- Failures leave a final summary line like `Errors: 1 Failures: 0 Ignored: 0`. Scroll up to the preceding stack trace for context.
- If the runner hangs, unplug/replug the Teensy and re-run with `-vvv` for extra chatter.

### Narrowing focus

The Unity harness pulls in everything under `firmware/test/test_*.cpp`. To isolate a single test:

1. Edit `firmware/test/test_mainUnity.cpp` and comment out the `RUN_TEST` macros you don’t need.
2. Re-run `pio test -e teensy40_unity`.
3. Restore the macros before you commit so CI sees the full suite.

For a deep dive into what each Unity or manual sketch checks, crack open [`firmware/test/README.md`](../firmware/test/README.md). It catalogs every test file with the subsystem it pokes.

### Manual hardware rituals (`src/*_t.cpp`)

Unity can only fake so much. When you need to watch real LEDs blink or feel a pot fight back, flash one of the manual sketches:

| Sketch | PlatformIO env | What you verify |
|--------|----------------|-----------------|
| `src/main_t.cpp` | `teensy40_full_system` | Step through LEDs, buttons, envelope followers, OLED—all with keyboard prompts. |
| `src/unified_t.cpp` | `teensy40_unified_test` | Full integration test controlled entirely by the real button matrix. |
| `src/biquadfilter_t.cpp` | `teensy40_biquad_test` | Pure DSP math, no external hardware. |
| `src/eeprom_persistence_t.cpp` | `teensy40_eeprom_persistence` | Ensures EEPROM writes survive reboots. |
| `src/verify_slots_t.cpp` | `teensy40_slot_verify` | Pounds on MIDISlot storage and reads it back. |

Run them with `pio run -e <env> -t upload`, then open a serial monitor or watch the device directly. These tests need a human watching and pushing buttons; log what you see if something twitches.

## Bridge CLI sanity (`bridge/test/`)

The bridge is the glue between the controller and the outside world. Its tests keep the CLI parser and error handling ruthless.

### Setup

```bash
cd bridge
npm install
```

Do this once per machine so `npm test` has everything it needs.

### Run the suite

```bash
npm test
```

The tests intentionally mock a missing serial port to prove the CLI doesn’t explode when the controller isn’t connected. When they pass, you’ll see Jest exit cleanly with green checkmarks. Failures print stack traces in the terminal and bubble all the way up to `test.sh`.

Want to iterate quickly? `npm test -- --watch` reruns on file changes.

### DAW loopback sanity check

Round-trip latency is the one metric your fancy synth professor will ask about first. Once you capture loopback impulses from the DAW rig, fold them into the test ritual:

```bash
python tools/rtl_analyzer.py \
  --group baseline=docs/bench/latency/baseline \
  --group tuned=docs/bench/latency/tuned \
  --buffer baseline=256 --buffer tuned=64 \
  --max-ms 10 \
  --export-json logs/rtl.json
```

- Toss this command into your bench notebook or CI job once audio captures are in place.
- The script throws a non-zero exit code if the measured round-trip latency blows past `--max-ms`, so you can gate merges on real numbers.
- JSON output keeps receipts—archive it next to the Unity and bridge logs so reviewers can reproduce your math.

Stay honest about the buffer math: PlatformIO doesn’t know your DAW, so this script is how we keep firmware changes and host tweaks from secretly increasing the tactile lag.

## Real hardware gauntlet (`bridge/` + future `system_test/`)

When you need to prove the whole stack plays nice, you move up to full-system tests. These expect real hardware **and** the Node bridge to be awake.

### Today’s manual flow

```bash
cd firmware
pio run -e teensy40_full_system -t upload
cd ../bridge
node mn42_bridge.js --serial /dev/ttyACM0 --osc 9000
# ...run system_test scripts once the bridge is live...
```

While the `system_test/` directory is still a sketchpad, this is how you can dry-run upcoming flows: flash the full-system firmware, light up the bridge on the right serial port, then execute whichever scripts you’re building. Capture console output and, if possible, video of the hardware reaction—future QA scripts will rely on that data.

### Environment expectations

- `teensy40_full_system` – firmware build that exposes every subsystem and respects the USB MIDI + Serial combo used in production.
- Node 18+ with `mn42_bridge.js` talking to the board on `/dev/ttyACM0` (or your platform equivalent).
- Once `system_test/` lands, assume it will call into the running bridge via OSC or WebSerial to poke the device.

Run the full-system layer before releases or any time hardware or bridge changes. It’s the last line of defense before you haul gear on stage. Bring a board, a cable, and zero fear. These tests waggle LEDs, trash EEPROM, and generally behave like they own the place.

