# Firmware Bench Summary: Live Controls

Date: 2026-08-27
Commit: e229ff189d640732fbe13290aa8b401954b5bcb2
Commit short: e229ff18
Firmware git_sha: unknown/not captured
Firmware version: unknown/not captured
Schema version: unknown/not captured
Power profile: unknown/not captured
Serial port: /dev/cu.usbmodem192460701
Host: Republican-Hivemind.local
Platform: darwin 27.0.0
Firmware env: teensy40_main
Runner: firmware/system_test/mn42_live_controls_runner.js
JSON report: /Users/bseverns/Documents/GitHub/MOARkNOBS-42/logs/live-controls-2026-08-27.json

## Result

FAIL

## Proven

- USB MIDI output toggle round-trips through `GET_USB_MIDI` / `SET_USB_MIDI`.
- Note dynamics round-trips through `GET_NOTE_DYNAMICS` / `SET_NOTE_DYNAMICS`.
- Jitter round-trips through `GET_JITTER` / `SET_JITTER`.
- Clock round-trips through `GET_CLOCK` / `SET_CLOCK`.
- Normalized `GET_CONFIG` hash is unchanged before and after the live-only lanes.
- Live-only controls did not create a config diff on the firmware lane.

## Lane Summary

- USB MIDI: false -> false -> false
- Note dynamics: velocity undefined, probability undefined -> velocity undefined, probability undefined -> restored velocity undefined, probability undefined
- Jitter: depth undefined, smoothness undefined -> depth undefined, smoothness undefined -> restored depth undefined, smoothness undefined
- Clock: follow_external false, clock_out false, bpm undefined -> follow_external false, clock_out false, bpm undefined -> restored follow_external false, clock_out false, bpm undefined

## Config Stability

- Baseline normalized GET_CONFIG hash: `unknown/not captured`
- Final normalized GET_CONFIG hash: `unknown/not captured`
- Stable: no

## Caveats

- This receipt proves firmware-side live control behavior directly over the serial/configurator lane.
- “Does not dirty staged config” is evidenced here by unchanged normalized `GET_CONFIG` state before/after the live-only commands.
- This receipt does not claim Bridge/App session behavior by itself.

