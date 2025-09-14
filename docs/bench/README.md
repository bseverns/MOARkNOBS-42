# Bench Logs Cheat Sheet

This folder is our crash pad for measurement runs. Latency and noise CSVs land
here via the `SerialToCsv` logger. Fork it, tweak it, but keep the vibe: small
rigs, quick runs, reproducible numbers.

## Layout
- `latency/` — scan\u2192send timing captures
- `noise/` — ADC raw counts while the board chills
- `environment/` — ambient conditions, PSU notes, moon phase
- `firmware/` — build stamp so we know what bits were on the board

## Flow
1. Flash firmware with logging flags on.
2. Use the Processing logger to snag CSV lines.
3. Stash the ambient info in `environment/conditions.json`.
4. Drop the firmware version in `firmware/build.txt`.
5. Crunch numbers however you want.

Stay loud, stay precise.
