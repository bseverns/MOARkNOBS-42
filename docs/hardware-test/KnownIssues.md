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
- The package does not claim display performance tuning across every module variant.

## Telemetry And Runtime Notes

- WebSerial validation depends on the current firmware text-command handshake and the included browser configurator.
- Live telemetry is a bench aid, not a hard real-time measurement contract.
- The full-stack bridge runner is hardware-in-the-loop and can fail for host serial-port, permissions, or bench-cabling reasons outside the firmware itself.

## Current Automated-Test Gaps

- `npm --prefix bridge test` currently fails in `bridge/test/serial_close_reconnect.test.js` because the bridge does not reopen after the mocked close event as the test expects.
- `npm --prefix App test` currently has one failing Playwright case in `tests/native_transport.spec.js` covering bundled-schema fallback when the device schema is incompatible.

## Manufacturing Boundary

- Present hardware PDFs and BOM exports are prototype references.
- Any fabrication archive present in the repository should be treated as unverified review material unless separately validated.
