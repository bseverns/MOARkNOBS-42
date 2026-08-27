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

## Failure

- Error: Operation not permitted, cannot open `/dev/cu.usbmodem192460701`.
- No device manifest or live-control lane result was captured.

## Evidence Status

This failed attempt does not prove firmware identity, live-control behavior, configuration stability, or cleanup.

## Caveats

- Retain this only as a diagnostic record of the failed attempt.
- Rerun with an attached board before citing current-HEAD HIL confidence.
