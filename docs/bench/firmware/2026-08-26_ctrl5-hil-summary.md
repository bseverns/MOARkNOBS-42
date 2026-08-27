# Firmware Bench Summary: Ctrl5 HIL Rerun

Date: 2026-08-26 (America/Chicago)
Commit: 626e40906c377307940375a7a0a305030e7924cc
Firmware git_sha: 626e4090
Firmware version: 0.0.0
Schema version: 9
Power profile: POWER_CHOKED_V1
Serial port: /dev/cu.usbmodem192460701
Firmware env: teensy40_main

## Result

PASS for all automated production-firmware HIL lanes run. The board was returned to `teensy40_main` standalone firmware after the checks and re-enumerated as `MN42 MIDI`.

## Executed Lanes

- Production firmware upload completed successfully with firmware SHA `626e4090`.
- Boot/configurator contract passed: live `HELLO`, configurator handoff, manifest/schema/config hydration, verified `SET_ALL` ACK, authoritative readback, and cleanup.
- Live-control round-trip passed for USB MIDI output, note dynamics, jitter, and clock; each value was restored.
- The normalized configuration hash remained `a02ce6e0cbb04b4ae054c3f2286ea46f79b6d8f2523ad66335d38b0fae5f6b3c` before and after the live-control lane.
- Structured Bridge session passed all six scenarios: server start, hardware session readiness, websocket `device.ready`, stage, verified Apply/ACK, and cleanup.
- Final cleanup restored `filter.idle_floor` to `24`.

## Artifacts

- `logs/boot-contract-2026-08-26-ctrl5-hil.json`
- `logs/live-controls-2026-08-26-ctrl5-hil.json`
- `logs/bridge-session-2026-08-26-ctrl5-hil.json`
- `docs/bench/firmware/2026-08-26_ctrl5-live-controls-summary.md`

## Observations

- Teensy Loader intermittently found HalfKay but failed its first write attempt. An immediate retry completed programming and booted normally both times this occurred.
- The first post-flash attach-live `HELLO` timed out. A retry with longer post-reboot timeouts completed the entire contract.
- The final device enumeration was `/dev/cu.usbmodem192460701`; no process owned the serial port when checked.

## Caveats

- These automated lanes prove that the updated production image runs on the attached board and that firmware/Bridge contracts remain healthy.
- They do not physically actuate Ctrl5. The new immediate tap-tempo behavior and `Ctrl0+Ctrl1+Ctrl4` LFO toggle still require a short human-in-the-loop button check on the assembled control surface.
- No destructive profile, macro, scene, corruption-injection, power-cut, LED stress, or thermal test was run.
