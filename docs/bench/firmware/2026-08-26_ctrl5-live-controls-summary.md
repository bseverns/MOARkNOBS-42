# Firmware Bench Summary: Live Controls

Date: 2026-08-27
Commit: 626e40906c377307940375a7a0a305030e7924cc
Commit short: 626e4090
Firmware git_sha: 626e4090
Firmware version: 0.0.0
Schema version: 9
Power profile: POWER_CHOKED_V1
Serial port: /dev/cu.usbmodem192460701
Host: Republican-Hivemind.local
Platform: darwin 27.0.0
Firmware env: teensy40_main
Runner: firmware/system_test/mn42_live_controls_runner.js
JSON report: logs/live-controls-2026-08-26-ctrl5-hil.json

## Result

PASS

## Proven

- USB MIDI output toggle round-trips through `GET_USB_MIDI` / `SET_USB_MIDI`.
- Note dynamics round-trips through `GET_NOTE_DYNAMICS` / `SET_NOTE_DYNAMICS`.
- Jitter round-trips through `GET_JITTER` / `SET_JITTER`.
- Clock round-trips through `GET_CLOCK` / `SET_CLOCK`.
- Normalized `GET_CONFIG` hash is unchanged before and after the live-only lanes.
- Live-only controls did not create a config diff on the firmware lane.

## Lane Summary

- USB MIDI: true -> false -> true
- Note dynamics: velocity 62, probability 100 -> velocity -12, probability 83 -> restored velocity 62, probability 100
- Jitter: depth 1, smoothness 0.5 -> depth 0.25, smoothness 0.75 -> restored depth 1, smoothness 0.5
- Clock: follow_external true, clock_out false, bpm 120 -> follow_external false, clock_out true, bpm 123.5 -> restored follow_external true, clock_out false, bpm 120

## Config Stability

- Baseline normalized GET_CONFIG hash: `a02ce6e0cbb04b4ae054c3f2286ea46f79b6d8f2523ad66335d38b0fae5f6b3c`
- Final normalized GET_CONFIG hash: `a02ce6e0cbb04b4ae054c3f2286ea46f79b6d8f2523ad66335d38b0fae5f6b3c`
- Stable: yes

## Caveats

- This receipt proves firmware-side live control behavior directly over the serial/configurator lane.
- “Does not dirty staged config” is evidenced here by unchanged normalized `GET_CONFIG` state before/after the live-only commands.
- This receipt does not claim Bridge/App session behavior by itself.

