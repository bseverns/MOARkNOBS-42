# Known Issues

## Current Package Limits

- No verified Gerber and NC-drill archive is claimed by this package yet.
- The hardware reference bundle is for review and testing only, not fabrication sign-off.
- A signed bridge installer is not included.
- Browser validation is scoped to the included `App/` assets and a local host flow, not a broad browser-support claim.
- The bridge validation scope is bench-focused and does not claim verified compatibility across every OSC host or DAW.

## Display And OLED Notes

- The current OLED expectation is address `0x3C`.
- OLED validation is based on the included hardware-test lanes, not a generic bus-scanner utility.
- Some bench receipts have observed `display_init_failed` during boot. Current firmware treats that as a controlled degraded mode instead of a boot blocker.
- When OLED bring-up fails, `HELLO`, `GET_MANIFEST`, `GET_SCHEMA`, `GET_CONFIG`, and `SET_ALL` remain available so the App and Bridge can still inspect or restore the board.
- `GET_MANIFEST` and WebSerial diagnostics now expose `display_present`, `display_ok`, `display_init_failures`, and `display_status` for machine-readable operator evidence.
- The firmware performs a safe periodic re-init attempt for a missing or failed OLED, but this is still not a claim of universal compatibility across every SSD1306 module variant.
- The package does not claim display performance tuning across every module variant.

## Telemetry And Runtime Notes

- WebSerial validation depends on the current firmware text-command handshake and the included browser configurator.
- Live telemetry is a bench aid, not a hard real-time measurement contract.
- The full-stack bridge runner is hardware-in-the-loop and can fail for host serial-port, permissions, or bench-cabling reasons outside the firmware itself.

## Automated-Test Verification Snapshot (2026-04-23)

The previously documented bridge/app test failures are no longer reproducible in current runs.

- Verified passing: `npm --prefix bridge test`
- Verified passing: `npm --prefix App test`
- Verified passing targeted native transport case:
  `npm --prefix App test -- tests/native_transport.spec.js`

No active automated-test failures are currently tracked in this document. Add new failures only with a reproducible command and failing test path.

## Manufacturing Boundary

- Present hardware PDFs and BOM exports are prototype references.
- Any fabrication archive present in the repository should be treated as unverified review material unless separately validated.
