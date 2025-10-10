# WebSerial Configuration App

`index.html` (still answering to the name `benzknobz.html`) is the browser-based patch bay for the MOARkNOBS-42 controller. The page is now split between a tiny runtime “kernel” and a BenzKnobz-specific view layer:

- `runtime.js` – owns WebSerial, schema validation (AJV), state diffing, checksum/rollback handling, throttling, and the simulator transport.
- `views/benzknobz.js` – renders the current layout, wires UI controls to the runtime API, and keeps the hardware muscle-memory alive.
- `benzknobz.css` – ships the design tokens as CSS custom properties so dark/light themes are a fast follow.
- `.eslintrc.json` / `.prettierrc.json` – browser-friendly lint + format defaults so contributions stay tidy.
- `config_schema.json` – the JSON Schema (draft 2020-12) that the runtime enforces before an Apply is ever allowed.

The repo deliberately feels like half studio notebook, half field guide. Snag the runtime for another layout later; the protocol stays centralized and testable.

## Quickstart

1. Flash the firmware and plug the controller into USB.
2. From this directory run a quick server (WebSerial refuses to jam over `file://`):
   ```bash
   python3 -m http.server
   ```
3. Hit <http://localhost:8000/> (or `/benzknobz.html` if you prefer the old URL) in Chrome or Edge.
4. Click **Connect**, pick the MOARkNOBS port, and let the header pill confirm the firmware, schema version, and memory stats.
5. Stage edits in the right-hand column. The **Apply** button only lights up after the JSON passes AJV validation.
6. On Apply the runtime pushes a single `SET_ALL` payload, waits for a `{checksum}` ACK, and only then commits the local snapshot. If the ACK is missing or mismatched the UI auto-rolls back and re-opens the diff panel.
7. Use the **Take Control** toggles per slot before sending live pot data to avoid on-stage jumps. Encoders still stream immediately.
8. Need hardware-free testing? Toggle the **Start simulator** button—the runtime swaps transports and replays canned manifest/state frames.

## Accessibility & Controls

- Every control carries a label and large hit target. Slot focus follows arrow keys; hold `Shift` for coarse/fine changes on numeric inputs just like the hardware.
- The connection pill announces changes through an ARIA live region, and telemetry painting is throttled to animation frames so assistive tech stays responsive.

## Runtime Contract

- The runtime buffers inbound telemetry and paints on `requestAnimationFrame` (~16 ms).
- Outbound pot changes are debounced to ≥24 ms through a shared utility so every control shares the same cadence.
- Schema mismatches fire a migration dialog before any live writes. The manifest now includes `fw_version`, `fw_git`, `build_ts`, and `schema_version` so bug reports can pin exact builds.
- Last-used USB IDs are remembered in `localStorage`; on load the app nudges you to reconnect but never reopens without a user gesture (WebSerial rules).

## Simulator & CI Hooks

`runtime.js` exports a simulator transport that mimics the firmware handshake, config payload, and telemetry frames. CI (or your own tests) can connect, screenshot the UI, and run migrations without a Teensy on the desk.

## Troubleshooting

- Serve over HTTPS or `http://localhost` or the browser will block WebSerial.
- If the status pill sulks, open the Debug Log panel to watch the raw JSON feed.
- AJV validation errors show up in the diff panel—fix them before Apply will enable.

Stay punk, document the weird edge cases, and ship patches with swagger.
