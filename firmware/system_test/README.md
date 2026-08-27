# System Test Pit

Welcome to the gauntlet. This folder is where the firmware proves it can survive a night on stage.
These are **hardware-in-the-loop** tests — they don't fake the MCU or the peripherals.
We flash the real board and make sure the buttons, LEDs, EEPROM and friends actually do their job.

## OSC↔firmware rodeo (from [`docs/validation/TESTING.md`](../../docs/validation/TESTING.md))

`docs/validation/TESTING.md` defines the OSC + WebSerial hardware-in-the-loop lane. Think of these as end-to-end stories
the automated runner rehearses:

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
- `mn42_bridge_session_runner.js` – Node-based bridge-session proof for the upgraded browser-console path:
  `/api/connect`, `/api/device/session`, `/api/device/stage`, `/api/device/apply`, cleanup apply, and `/ws/events`.
- `mn42_boot_contract_runner.js` – Node-based direct-serial proof for the exact `teensy40_main` boot path: standalone boot
  banner, `ENTER_CONFIG_MODE` reboot, configurator handshake, one staged `SET_ALL` apply/ACK, and cleanup back to the original
  config.
- `mn42_persistence_abuse_runner.js` – Evidence wrapper for persistence abuse. Safe by default: it runs the non-destructive
  boot/apply/readback proof and only runs destructive profile/macro/scene storage checks when `--exercise-storage` is passed.
- `test_*.cpp` – hardware-oriented subsystem tests, including Ctrl3/Ctrl4 double-press timing, immediate Ctrl5 tap tempo, and the `Ctrl0+Ctrl1+Ctrl4` live LFO 1 chord.
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

## Running the structured bridge-session proof

Use this when you want hardware evidence for the modern bridge runtime rather than the older raw OSC heartbeat story.

1. Put `teensy40_main` into USB configurator mode first. The simplest repeatable lane is:

   ```bash
   node firmware/system_test/mn42_boot_contract_runner.js \
     --serial /dev/cu.usbmodemXXXX \
     --attach-live \
     --report logs/boot-contract-attach-live.json
   ```

2. Run the bridge-session proof:

   ```bash
   node firmware/system_test/mn42_bridge_session_runner.js \
     --serial /dev/cu.usbmodemXXXX \
     --http-port 8791 \
     --report logs/bridge-session-test.json
   ```

The runner proves:

- the browser-console server starts
- `/api/connect` starts the bridge against the real USB serial port
- `/api/device/session` reaches `ready`
- `/ws/events` emits `device.ready`
- `/api/device/stage` marks the session dirty
- `/api/device/apply` returns an ACK/checksum and promotes staged config to live
- cleanup apply restores the original live config

## Running the persistence abuse wrapper

Use this when you want one JSON receipt that separates:

- safe automated HIL proof
- opt-in destructive storage proof
- Unity-covered corruption scenarios
- manual-only fault-injection cases

Safe default run:

```bash
node firmware/system_test/mn42_persistence_abuse_runner.js \
  --serial /dev/cu.usbmodemXXXX \
  --report logs/persistence-abuse-safe.json
```

The safe run writes two artifacts:

- the requested JSON report
- a dated Markdown receipt at `docs/bench/firmware/YYYY-MM-DD_persistence-safe-summary.md`

Destructive storage run on sacrificial slots:

```bash
node firmware/system_test/mn42_persistence_abuse_runner.js \
  --serial /dev/cu.usbmodemXXXX \
  --exercise-storage \
  --profile-slot 3 \
  --scene-slot 5 \
  --pot-index 0 \
  --report logs/persistence-abuse-storage.json
```

The wrapper does not fake real-board corruption. It reports primary/backup/default recovery from the Unity suite and leaves
power-cut or raw-corruption fault injection in the manual bench lane until there is an explicit safe hook.

## When to reach for the older sketches

Those `test_*.cpp` binaries still have a place when you’re chasing electrical gremlins—buttons, LEDs, or the envelope follower
baseline. Use `pio run -d firmware -e teensy40_eeprom_persistence` and friends for deep dives, then pivot back to the automated
runner once the solder smoke clears.

Rock it, break it, then make it better.

## Running the production boot contract runner

Use this when you need evidence for the real `teensy40_main` boot/configurator handoff instead of the bridge demo lane.

1. Install bridge deps if you have not already:

   ```bash
   npm --prefix bridge ci
   ```

2. Run the boot contract proof against the attached board. Add `--flash` when you want the runner to upload `teensy40_main`
   first:

   ```bash
   node firmware/system_test/mn42_boot_contract_runner.js \
     --serial /dev/cu.usbmodemXXXX \
     --flash \
     --report logs/boot-contract.json | tee logs/boot-contract.log
   ```

   If the board is already running the current firmware and you cannot reliably attach before the one-shot standalone boot
   banner, use the attach-live lane instead:

   ```bash
   node firmware/system_test/mn42_boot_contract_runner.js \
     --serial /dev/cu.usbmodemXXXX \
     --attach-live \
     --report logs/boot-contract-attach-live.json | tee logs/boot-contract-attach-live.log
   ```

The runner proves:

- standalone boot emits the boot banner plus `{"type":"boot_mode","mode":"standalone_runtime"}`
- direct serial answers `HELLO` while the standalone runtime is alive
- `ENTER_CONFIG_MODE` reboots into `{"type":"boot_mode","mode":"usb_configurator"}`
- configurator mode completes `HELLO` -> `GET_MANIFEST` -> `GET_SCHEMA` -> `GET_CONFIG`
- one small `SET_ALL` change receives a matching ACK/checksum and readback
- cleanup restores the original config unless `--skip-cleanup` is set

In `--attach-live` mode, the runner still proves the real `HELLO` -> `ENTER_CONFIG_MODE` -> configurator handshake ->
`SET_ALL`/ACK/cleanup path, but it does not require capturing the cold-boot standalone banner from byte zero.

Note: after the configurator-mode proof, the board remains in USB configurator mode until the next manual reset or power cycle.
