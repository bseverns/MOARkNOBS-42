# Testing the Beast

This repo runs tests in layers, from polite unit checks to full-on hardware cage matches.

## Quick hit: `test.sh`

Too lazy to juggle commands? Slam `./test.sh` in the repo root and it will sniff for a Teensy on the
first `/dev/ttyACM*`, `/dev/ttyUSB*`, or macOS cousin. If it can't find your board—or you're juggling
more than one—yell the port name through `TEST_PORT`:

```bash
TEST_PORT=/dev/ttyACM0 ./test.sh
```

Either way, the run leaves bread crumbs in `logs/` so you can trace what blew up or brag about what
passed. The script blasts Unity firmware tests and bridge checks, and it writes JUnit and console
output to `logs/unity-test.xml`, `logs/unity-test.log`, and `logs/bridge-test.log`.

## Unity smoke tests (`firmware/test/`)

Unity tests are the quick-and-dirty pulse check for the firmware. They run on the Teensy board and
stub out anything that would otherwise demand real wires. Fire them up when you've tweaked core logic
or want receipts before you solder.

**Command**
```bash
cd firmware
pio test -e teensy40_unity
```

If you want receipts, the run spits a raw transcript to `logs/unity-test.log`.
Each test drops a line ending in `PASS` or `FAIL`; the file wraps with an `OK`
summary when every test behaves. Spot a `FAIL` or some grumpy final tally and
you've got work before you rock on.

**Environment**

`teensy40_unity` – Teensy 4.0 with the Unity harness. Plug the board in over USB; the tests scream back
over serial.

Run these anytime you touch code under `src/` or `lib/`. They catch dumb regressions before you waste
minutes flashing the full rig.

## Full-system trials (`bridge/test/` and future `system_test/`)

When you need to prove the whole stack plays nice, you move up to full-system tests. These expect real
hardware and the Node bridge to be awake.

### Bridge CLI sanity

`bridge/test/` holds a Node.js script that makes sure the bridge even starts. It won't mock your
hardware for you—it's just a smoke signal for the command-line parser.

**Command**

```bash
cd bridge
npm test
```

**Environment**

Node 18+, controller connected if you want to see real serial chatter. The test suite itself fakes a
missing port to make sure the bridge doesn't crash when the cable's yanked.

### Real hardware gauntlet

The future `system_test/` directory will host black-box tests that kick the actual controller and
listen for the right squeals. Expect scripts here to upload dedicated test firmware and then poke the
board through the bridge.

**Example flow**

```bash
cd firmware
pio run -e teensy40_full_system -t upload
cd ../bridge
node mn42_bridge.js --serial /dev/ttyACM0 --osc 9000
# ...run system_test scripts once the bridge is live...
```

**Environment**

- `teensy40_full_system` – flashes a firmware build that exposes every subsystem.
- Node 18+ with `mn42_bridge.js` talking to the board on `/dev/ttyACM0`.
- Whatever `system_test/` scripts land here will assume the controller and bridge are both lit up.

Run the full-system layer before releases or any time hardware or bridge changes. It's the last line
of defense before you haul gear on stage.

Bring a board, a cable, and zero fear. These tests waggle LEDs, trash EEPROM, and generally behave like they own the place.

