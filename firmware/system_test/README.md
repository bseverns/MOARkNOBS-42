# System Test Pit

Welcome to the gauntlet. This folder is where the firmware proves it can survive a night on stage.
These are **hardware-in-the-loop** tests — they don't fake the MCU or the peripherals.
We flash the real board and make sure the buttons, LEDs, EEPROM and friends actually do their job.

## OSC↔firmware rodeo (pulled from [`docs/TESTING.md`](../../docs/TESTING.md))

`docs/TESTING.md` keeps teasing the "future black-box trials" that lean on OSC + WebSerial, so we sketched the drill before
you wire up scripts. Think of these as end-to-end stories the automated runner will rehearse:

1. **Handshake & heartbeat** – `mn42_bridge.js` should auto-fire `HELLO`, wait for the board’s `{"hello":"mn42"}` reply, and
   stream `/mn42/slots` + `/mn42/envelopes` snapshots. No stream, no party.
2. **OSC command loop** – sling `/mn42/cmd` with `{ "cmd": "SET_SLOT_VALUE", "slot": N, "value": V }`. The bridge must validate
   the JSON, translate it to the firmware text command `SET_SLOT_VALUE,<slot>,<value>`, and the firmware should report the new
   value in the next `/mn42/slots` burst.
3. **Keep-alive sanity** – once streaming, periodic snapshots should keep landing without re-sending `HELLO`. If packets stop,
   the bridge should whine in its stdout and attempt a reconnect.
4. **Storage smoke pass** – when explicitly armed, the runner closes the bridge phase, opens the serial lane directly, and
   proves `SAVE_PROFILE` / `LOAD_PROFILE` / `RESET_PROFILE`, `SAVE_MACRO_SLOT` / `RECALL_MACRO_SLOT`, and
   `SAVE_SCENE` / `GET_SCENES` / `RECALL_SCENE` against a real board.

These checkpoints match the spirit of the "Real hardware gauntlet" section: the scripts aren't just poking APIs, they're
proving the board, bridge, and OSC stack stay in lockstep.

## What's inside

- `mn42_fullstack_runner.js` – Node-based system smoke test that orchestrates the OSC ↔ bridge ↔ firmware handshake above, and
  can optionally run the destructive EEPROM storage smoke pass after the bridge phase.
- `test_*.cpp` – legacy manual sketches you can still flash for subsystem debugging until every edge case is automated.
- `TestHelpers.cpp` – shared glue for those sketches.

## Running the full-stack script

1. Flash the hardware with the full system firmware:

   ```bash
   pio run -d firmware -e teensy40_full_system -t upload
   ```

2. Install bridge deps if you haven’t already:

   ```bash
   npm --prefix bridge ci
   ```

3. Fire the runner (it spawns the bridge for you). Override ports via flags or env vars if needed:

   ```bash
   node firmware/system_test/mn42_fullstack_runner.js \
     --serial /dev/ttyACM0 \
     --osc-out 10000 \
     --osc-in 10001 \
     --report logs/system-test.json | tee logs/system-test.log
   ```

   It exits non-zero if any OSC↔firmware scenario craters. Reports capture both the story (JSON) and the raw log so CI can hoard
   artifacts.

4. If you want the storage/recovery pass too, use explicit sacrificial slots:

   ```bash
   node firmware/system_test/mn42_fullstack_runner.js \
     --serial /dev/ttyACM0 \
     --exercise-storage \
     --profile-slot 3 \
     --scene-slot 5 \
     --pot-index 0 \
     --report logs/system-test-storage.json | tee logs/system-test-storage.log
   ```

   This intentionally overwrites:

- the selected profile slot
- the single macro snapshot
- the selected scene slot

   The runner restores the active live profile and rewrites the selected profile slot if it had an existing stored payload, but it
   cannot non-destructively back up the macro snapshot or an occupied scene slot. Pick bench-only storage targets.

## When to reach for the older sketches

Those `test_*.cpp` binaries still have a place when you’re chasing electrical gremlins—buttons, LEDs, or the envelope follower
baseline. Use `pio run -d firmware -e teensy40_eeprom_persistence` and friends for deep dives, then pivot back to the automated
runner once the solder smoke clears.

Rock it, break it, then make it better.
