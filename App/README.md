# MOARkNOBS-42 Browser Configurator

Use the browser configurator when you want direct USB setup, monitoring, and profile management over WebSerial. If you need OSC or a virtual MIDI port on a desktop host instead, start with [docs/ConnectivityGuide.md](../docs/getting-started/ConnectivityGuide.md) and use the bridge.

Current support boundary:

- strongest repo evidence for the direct-browser path: Chromium-based WebSerial
- strongest repo evidence for the non-WebSerial path: the bridge-served `/app/` configurator on a Node 20 desktop host
- not claimed here as a verified production path: Firefox/Safari WebSerial support or universal browser compatibility

See [docs/HostCompatibility.md](../docs/reference/HostCompatibility.md) for the conservative matrix before treating this as a broad browser-support promise.

[bseverns.github.io/MN42](http://bseverns.github.io/MN42) is the browser-based patch bay for the MOARkNOBS-42 controller. The page is now split between a tiny runtime “kernel” and a BenzKnobz-specific view layer:

- `runtime.js` – owns WebSerial, schema validation (via the bundled mini-Ajv in `lib/mini-ajv.js`), state diffing, checksum/rollback handling, throttling, and the simulator transport.
- `views/benzknobz.js` – renders the current layout, wires UI controls to the runtime API, and keeps the hardware muscle-memory alive.
- `benzknobz.css` – ships the design tokens as CSS custom properties so dark/light themes are a fast follow.
- `.eslintrc.json` / `.prettierrc.json` – browser-friendly lint + format defaults so contributions stay tidy.
- `config_schema.json` – the JSON Schema (draft 2020-12) that the runtime enforces before an Apply is ever allowed.

The repo deliberately feels like half studio notebook, half field guide. Snag the runtime for another layout later; the protocol stays centralized and testable.

## Quickstart

1. Flash the firmware and plug the controller into USB.
2. Choose your transport:
   - direct WebSerial path: from this directory run a quick server (WebSerial refuses to jam over `file://`):
   ```bash
   python3 -m http.server
   ```
   - bridge path: run `npm --prefix bridge start`, open <http://127.0.0.1:8787/>, then click **Open configurator**
3. Hit <http://localhost:8000/> for the direct path, or the bridge-served `/app/` URL for the bridge path. The legacy `/benzknobz.html` URL sticks around as a redirect for old bookmarks.
4. Click **Connect**, pick the MOARkNOBS port if you are on WebSerial, and let the header pill confirm the firmware, schema version, and memory stats.
5. Stage edits in the right-hand column. The **Apply** button only lights up after the JSON passes the active schema validator (device schema when compatible, bundled `config_schema.json` fallback otherwise).
6. On Apply the runtime stages one config payload with schema version, manifest metadata, and a SHA-256 checksum. Simulator mode keeps the JSON-RPC envelope; native WebSerial and bridge sessions adapt that request onto the firmware's `SET_ALL` text protocol. Only a matching device ACK promotes staged state to live state; if the ACK is missing or mismatched the UI auto-rolls back and re-opens the diff panel.
7. Use the browser-only **Take Control** toggles per slot before sending live pot data to avoid on-stage jumps. They are local safety guards, not firmware-backed config, so they do not require **Apply**.
8. Need hardware-free testing? Toggle the **Start simulator** button—the runtime swaps transports and replays canned manifest/state frames.

For the operator-facing explanation of `Immediate local response` versus the browser-only pickup guard, read [docs/OperatorTutorial.md](../docs/guides/OperatorTutorial.md).

The written field guide below is the current operator-facing reference; add screenshots when you want a release-ready visual walkthrough, not as a substitute for the contract notes.

## UI Field Guide

- **LED Color Lab** – Slide the brightness control and watch it punch straight into the staged JSON; the value is clamped to the same 0–255 lane the firmware enforces, so you’re rehearsing reality. Ride the fader slowly and the runtime’s [24 ms throttling](#runtime-contract) keeps chatter to a polite murmur; slam it and the staged hex display still tracks every move so you can screen-cap or copy/paste the exact color code. Hex edits round-trip: type a six-character value, press return, and the slider jumps to the matching luminance. If Apply can’t land (checksum blowout, unplugged cable), the [checksum rollback flow](#quickstart) rewinds both the slider and hex badge so the LED preview never lies.
- **Preset Import/Export Pad** – Drop a `.json` file or click **Import** and the manifest streams into staging only after the mini-Ajv bundle gives it a clean bill of health—same validation gauntlet called out in [Quickstart](#quickstart). Export takes whatever is staged right now, including unsent tweaks, so you can stash experiments in git or share patches without touching hardware. When Apply sticks, the status pill records the checksum + filename combo so your studio notebook and the controller stay in lockstep.
- **Profile Slot Workflow** – The A–D profile picker keeps a browser-side target slot and file backup path available, but current firmware also exposes real device-backed **Switch profile** / **Save profile** / **Reset profile** actions. The manifest still gates those buttons so older firmware builds fail closed instead of pretending support.
- **Simulator Toggle** – The **Start simulator** switch sits dead-center under transport controls for a reason: it swaps WebSerial for the canned bridge inside `runtime.js` instantly. The log banner flips to “Simulated” and it stays that way until you reconnect a device. Because the simulator obeys the same throttled paint loop documented in [Runtime Contract](#runtime-contract), you can chase layout timing bugs or automation macros without a Teensy on the desk.
- **Device Monitor Stack** – The telemetry cards (uptime, firmware hash, slot stats) repaint on every animation frame so you can feel live latency. Hover to freeze the ticker when you need to copy numbers into a bug report. Any schema or checksum mismatch slams you back into the [rollback workflow](#quickstart), and the monitor holds onto the last verified frame so you know exactly what state the firmware was in when things went sideways.
- **Staged Diff Panel** – The right-hand rail wakes up as soon as the staged JSON drifts from the live manifest. Validation errors park directly above the offending field; fix them and **Apply** roars back to life in the same breath. Post-Apply, scroll to the tail to see the runtime commit log—checksum, slot count, and any throttled writes. Tooltips on greyed-out controls punch straight back into the contract notes in [Runtime Contract](#runtime-contract) so you can trace every guardrail.
- **Schema-driven Forms** – Every control in the right-hand rail is rendered from `config_schema.json` (Filter, ARG, LEDs, EF assignments, and all slot knobs). The `FormRenderer` builds collapsible sections, clamps number fields to the schema’s bounds, and stages each edit immediately; hitting **Apply** batches the staged JSON through `set_config`, while field-level writes can still travel through `runtime.applyPatch(...)/set_param` when a control wants an immediate RPC. Profile save/load/reset and macro/scene actions travel on their own native command paths instead of pretending to be config diffs. Keybindings still apply: slot focus follows arrow keys; hold `Shift` for coarse/fine nudging; and the simulator status pill keeps status events in sync even when the board takes a coffee break.
- **Browser-only Slot Notes** – `Slot label`, the MIDI badge, and `Take Control` now live outside the device schema. They are stored in local browser state so reconnects keep your operator hints and pickup guards without pretending the firmware persisted them.
- **Local response modes** – The tiny slot badge in the live grid is an operator aid, not device config. `IM` means the browser responds immediately to local control input; `PK` means the browser waits for the control to catch the current value before activating it again. The deeper operator explanation lives in [docs/OperatorTutorial.md](../docs/guides/OperatorTutorial.md).
- **Basic / Advanced Mode** – New sessions start in **Basic** mode, keeping the panel focused on everyday knob-to-MIDI mapping. Flip to **Advanced** to reveal EF/ARG/filter tuning, scope tools, and debug surfaces. The choice is saved in `localStorage`, and glossary-style info badges explain jargon like EF and ARG in-place.

## MIDI Monitor Panel

A new MIDI Monitor panel sits beside the transport controls. Toggle it open, grant Web MIDI, and the panel streams incoming/outgoing bytes into a capped (1k entries) log, keeping the DOM light even at a sustained 1k msgs/min. Select the desired output, set the BPM slider, and press **Start clock** to stream realtime `0xF8` pulses—the button flips to **Stop clock** while the clock runs and every tick logs as an outgoing entry so you can verify the bench flow without leaving the browser.

## Slot Architecture Cheat Sheet

- Every slot now exposes its per-slot envelope follower payload. The editor’s **Envelope Follower** block lets you park a follower index, pick a filter shape, and dial in frequency, Q, oversample count, smoothing, baseline, and gain without leaving the browser.
- EF assignment rows now accept comma-separated slot lists per follower (`efSlots[].slots`), so one follower can modulate multiple slots from the same staging pass.
- ARG lives per-slot too. The **ARG Combiner** panel flips the enable flag, locks the math method, and routes sources A/B with the same coarse/fine nudging as the hardware encoders.
- The slot details panel mirrors the new firmware schema: EF index, filter, dynamics, baseline/gain, plus the ARG mode and source map stream live next to the MIDI stats.
- `runtime.js` normalises staged JSON so the firmware always gets properly-shaped `ef`/`arg` bundles, and it now digests live `slot_patch` frames from the bridge to keep the UI in sync when the hardware mutates slots.

## Accessibility & Controls

- Every control carries a label and large hit target. Slot focus follows arrow keys; hold `Shift` for coarse/fine changes on numeric inputs just like the hardware.
- The connection pill announces changes through an ARIA live region, and telemetry painting is throttled to animation frames so assistive tech stays responsive.
- Selecting a SysEx slot reveals a hex template field. Enter space-separated bytes plus `XX`/`MSB`/`LSB` placeholders—the UI normalises the case and the firmware swaps in live values on send.

## Runtime Contract

- Transport handshake is `hello` → `get_manifest` → `get_schema` → `get_config`. On simulator transport those travel as JSON-RPC; on native WebSerial and bridge sessions the runtime maps them to `HELLO`, `GET_MANIFEST`, `GET_SCHEMA`, and `GET_CONFIG` before trusting any config payload.
- The runtime keeps separate `liveConfig` and `stagedConfig` snapshots. The UI only mutates staged state; successful Apply promotes staged state to live state.
- The diff panel is computed from `liveConfig` vs `stagedConfig`, which is why it can remain truthful even while device patches are streaming in.
- The runtime buffers inbound telemetry and paints on `requestAnimationFrame` (~16 ms) so frequent state messages do not turn the DOM into soup.
- Outbound pot changes are debounced to ≥24 ms through a shared utility so every control shares the same cadence.
- `runtime.applyPatch(path, value)` stages a field locally first, then routes a `{rpc:"set_param"}` call through the same RPC lane. The simulator applies it immediately; native firmware defers those fine-grained writes until the next full Apply because the production contract is config-oriented. If the RPC path fails, the runtime rolls the staged state back.
- Full Apply sends `set_config` with schema version, manifest metadata, staged config, and a SHA-256 checksum. A mismatched ACK triggers rollback instead of silently pretending success.
- Browser-only slot metadata (`label`, pickup guard, and the MIDI badge) is stored separately in `localStorage`, merged back into the UI on read, and never included in `Apply` or schema diffing.
- Schema mismatches fire a migration-required event before any live writes. Migration adapters live in the App layer as a `migrations` map passed to `createRuntime(...)`, keyed like `"5->6"`.
- Last-used USB IDs and the last staged snapshot are remembered in `localStorage`; on load the app nudges you to reconnect but never reopens without a user gesture (WebSerial rules).

## Simulator & CI Hooks

`runtime.js` exports a simulator transport that mimics the firmware handshake, config payload, and telemetry frames. CI (or your own tests) can connect, screenshot the UI, and run migrations without a Teensy on the desk.

## Headless Harness

Need proof the simulator flow hasn’t rotted without booting Chrome manually? Run:

```bash
npm --prefix App test
```

## Testing & CI

- `npx playwright test` runs the UI suite (Playwright spins up `App/tests/dev-server.mjs` and targets `/benzknobz.html`).
- The CI pipeline now verifies `npx playwright test` alongside firmware/unit suites; keep this command green before merging.

That spins up a tiny static server, launches Playwright’s headless Chromium, and imports the real `runtime.js` + `views/benzknobz.js`. The script walks through the README workflows—arming the simulator, driving the staged diff validator, forcing an ACK mismatch to trigger rollback, rewriting the manifest on the fly to rehearse the migration dialog, and finally flipping the simulator back off once a clean apply lands. When the test passes you know WebSerial ergonomics (and the migration guardrails) survived without babysitting a browser window.

## Troubleshooting

- Serve over HTTPS or `http://localhost` or the browser will block WebSerial.
- If the status pill sulks, open the Debug Log panel to watch the raw JSON feed.
- Schema validation errors show up in the diff panel—fix them before Apply will enable.

Stay punk, document the weird edge cases, and ship patches with swagger.
