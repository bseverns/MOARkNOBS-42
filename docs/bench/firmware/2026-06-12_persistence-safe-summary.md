# Firmware Bench Summary: Safe Persistence

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
Runner: firmware/system_test/mn42_persistence_abuse_runner.js
JSON report: logs/persistence-abuse-safe-2026-06-12.json
Boot proof report: /Users/bseverns/Documents/GitHub/benzknober/logs/persistence-boot-proof-2026-06-12T00-57-45-480Z.json

## Result

PASS

## Proven

- Baseline config was read from the attached board.
- One safe config mutation was staged locally.
- The apply returned a matching checksum ACK.
- GET_CONFIG readback confirmed the applied mutation.
- The original value was restored.
- Final GET_CONFIG readback confirmed cleanup.

## Mutation Summary

- Path: `filter.idle_floor`
- Baseline value: 24
- Staged value: 25
- Readback after apply: 25
- Restored value: 24
- Apply checksum ACK: df1a3e6fed1e821684444405b75ed4bc390ca4bbf6b75a84cc7e5a28192c1364
- Restore checksum ACK: 0498d0d50a40a8dac584becbd78f2e9c8cb3abe53cb8b9a46d0f19a3b7599de2

## Step Log

- PASSED: Read baseline config — filter.idle_floor=24
- PASSED: Stage one safe config mutation — filter.idle_floor 24 -> 25
- PASSED: Apply and confirm checksum ACK — seq=1 checksum=df1a3e6fed1e821684444405b75ed4bc390ca4bbf6b75a84cc7e5a28192c1364
- PASSED: Read back and confirm mutation — filter.idle_floor=25
- PASSED: Restore original value — seq=2 checksum=0498d0d50a40a8dac584becbd78f2e9c8cb3abe53cb8b9a46d0f19a3b7599de2 filter.idle_floor=24
- PASSED: Read back and confirm cleanup — filter.idle_floor=24

## Caveats

- Non-destructive only. This receipt does not include raw EEPROM corruption, profile-slot destruction, or power-pull timing drills.
- Reset/power-cut timing remains manual until a safe hook exists.
