# Firmware Bench Summary: Live Controls

Date: 2026-06-12
Commit: 40252655fd105e4da3c15dda69033acdfa7538a4
Commit short: 4025265
Firmware git_sha: 2ef8107
Firmware version: 0.0.0
Schema version: 6
Power profile: POWER_CHOKED_V1
Serial port: /dev/cu.usbmodem192460701
Host: Republican-Hivemind.local
Platform: darwin 25.5.0
Firmware env: teensy40_main
Runner: firmware/system_test/mn42_live_controls_runner.js
JSON report: /Users/bseverns/Documents/GitHub/benzknober/logs/live-controls-2026-06-12.json

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
- Note dynamics: velocity 0, probability 100 -> velocity -12, probability 83 -> restored velocity 0, probability 100
- Jitter: depth 1, smoothness 0.5 -> depth 0.25, smoothness 0.75 -> restored depth 1, smoothness 0.5
- Clock: follow_external true, clock_out false, bpm 120 -> follow_external false, clock_out true, bpm 123.5 -> restored follow_external true, clock_out false, bpm 120

## Config Stability

- Baseline normalized GET_CONFIG hash: `50f7898b5ba4306b5cad6e857337b9586bd07e3672e1fcf04d891722d95dcefc`
- Final normalized GET_CONFIG hash: `50f7898b5ba4306b5cad6e857337b9586bd07e3672e1fcf04d891722d95dcefc`
- Stable: yes

## Caveats

- This receipt proves firmware-side live control behavior directly over the serial/configurator lane.
- “Does not dirty staged config” is evidenced here by unchanged normalized `GET_CONFIG` state before/after the live-only commands.
- This receipt does not claim Bridge/App session behavior by itself.
