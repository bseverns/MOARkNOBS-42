# MOARkNOBS-42 Browser Configurator

> **Doc class:** Contract doc. This is the App-facing behavior and support boundary for the browser configurator; use the linked evidence docs before widening compatibility claims.

Use the browser configurator when you want direct USB setup, monitoring, and profile management over WebSerial. If you need OSC or a virtual MIDI port on a desktop host instead, start with [docs/ConnectivityGuide.md](../docs/getting-started/ConnectivityGuide.md) and use the bridge.

For document tie-break rules, see [Documentation Truth Map](../docs/reference/DocumentationTruthMap.md).

Current support boundary:

- the bundled fallback contract is schema 8, including two fixed per-slot LFO lanes and profile-local ARG/LFO persistence
- Connect controls remain disabled and marked busy until the operator shell has bound its transport handlers and runtime subscriptions; this also makes the legacy `benzknobz.html` redirect safe against early clicks.
- Lab selected-slot editing exposes both fixed LFO lanes with enable, combine-mode, and signed-amount controls; these are staged and verified through the normal Apply transaction.
- strongest repo evidence for the direct-browser path: Chromium-based WebSerial
- strongest repo evidence for the non-WebSerial path: the bridge-served `/app/` configurator on a Node 24 desktop host
- package scripts intentionally pin Node to `>=24 <25`; widening that floor needs explicit test evidence first
- not claimed here as a verified production path: Firefox/Safari WebSerial support or universal browser compatibility

Bench receipts and receipt templates for operator-facing App HIL lanes live in [../docs/bench/app/README.md](../docs/bench/app/README.md). The latest live Bridge-served proof is [2026-05-31 App-over-Bridge-session summary](../docs/bench/app/2026-05-31-app-over-bridge-session-summary.md).
Transport-mode truth and UI labeling rules live in [../docs/app/AppTransportTruthTable.md](../docs/app/AppTransportTruthTable.md).

See [Host Compatibility](../docs/reference/HostCompatibility.md) for the conservative matrix before treating this as a broad browser-support promise.

[bseverns.github.io/MN42](http://bseverns.github.io/MN42) is the browser-based patch bay for the MOARkNOBS-42 controller. The page is now split between a tiny runtime “kernel” and a BenzKnobz-specific view layer:

- `runtime.js` – owns WebSerial, schema validation, state diffing, transaction uncertainty/resynchronization, throttling, and the simulator transport.
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
3. Hit <http://localhost:8000/> for the direct path, or the bridge-served `/app/` URL for the bridge path. For local testing and older bookmarks, `benzknobz.html` remains a supported entry page.
4. If you are using the direct USB path, click **Check compatibility** first when you are unsure about browser/OS support. The configurator now calls out unsupported browsers, insecure origins, and cancelled port pickers before you burn time on vague connection failures.
5. Click **Connect**, pick the MOARkNOBS port if you are on WebSerial, and let the header pill confirm the firmware, schema version, and memory stats.
6. Stage edits in the right-hand column. The **Apply** button only lights up after the JSON passes the active schema validator (device schema when compatible, bundled `config_schema.json` fallback otherwise).
7. Use **Import config JSON** and **Export config JSON** in the utility rail for backup, restore, and shareable profiles. Import stages the file locally first; nothing touches the hardware until you click **Apply**.
8. On Apply the runtime sends one checksummed candidate. A valid receipt and authoritative readback establish device truth. Missing or malformed receipts enter `uncertain`/`resynchronizing`; they never imply that hardware rolled back. The canonical state model is [Configuration Transaction Model](../docs/reference/ConfigurationTransactionModel.md).
9. Need a hardware-free introduction? Choose **Start simulator** directly from Configure's empty state. Lab retains the transport toggle for stopping or restarting it.

## UI Field Guide

- **LED Color Lab** – Brightness and color edits remain staged until Apply. If Apply becomes ambiguous, the candidate remains visible while the runtime reads authoritative device state back.
- **Power Safety Summary** – Before a successful handshake the App says **Power status unavailable** and never treats the bundled fallback manifest as attached-device truth. After connection, mode-appropriate surfaces may show the device-reported `power_profile`, `led_brightness_cap`, and `rail_topology_verified` values as technical metadata. Stage keeps healthy technical metadata and firmware version inside its collapsed **Device details** disclosure, while `POWER_CHOKED_V1` still triggers the prominent global power-limited warning in Stage, Configure, and Lab.
- **Config Import/Export Pad** – Use **Import config JSON** to stage a saved configuration locally before you touch the hardware; the file lands in the same staged workspace used by live edits. **Export config JSON** saves whatever is staged right now, including unsent tweaks, so you can stash experiments in git or share patches without touching hardware. When Apply sticks, the status pill records the exported filename or sync result so your studio notebook and the controller stay in lockstep.
- **Profile Slot Workflow** – The A–D profile picker keeps a browser-side target slot and file backup path available, while supported firmware exposes consequence-oriented actions such as **Save to Profile A**, **Switch to Profile A now**, and **Reset Profile A**. Profile, scene, macro-recall, and transport actions that could replace a dirty draft are blocked until the operator explicitly applies or discards it. The manifest still gates those buttons so older firmware builds fail closed instead of pretending support.
- **Simulator Controls** – Configure's empty-state **Start simulator** action swaps WebSerial for the canned transport and connects in one step. Lab retains the explicit transport toggle. Because the simulator obeys the same throttled paint loop documented in [Runtime Contract](#runtime-contract), you can chase layout timing bugs or automation macros without a Teensy on the desk.
- **Panic & Recovery Help** – The **Panic Help** button now opens a compact recovery dialog instead of parking firmware-flash instructions in the utility rail full time. It still leads with the hardware combo (`Ctrl0 + Ctrl1 + Ctrl2`), but it also gives you on-demand backup/export actions, configurator reboot, and the two real firmware lanes: release hex via `teensy_loader_cli` or source upload via `pio run -d firmware -t upload -e teensy40_main`.
- **Live Runtime Controls** – The utility rail now hosts browser-driven runtime controls that do not participate in the staged Apply/Discard transaction. `USB MIDI Output`, `Note Dynamics`, and `Jitter` talk straight to `GET_/SET_` protocol lanes, mirror telemetry readback, and use soft takeover on the firmware side so the hardware pots can reclaim control without a jump.
- **Device Clock & Diagnostics** – The MIDI utility tab now separates browser-generated Web MIDI clock from the firmware’s own clock engine. `Device Clock` edits the hardware clock source, internal BPM, and clock-out flag live, while the `Device Monitor` now consumes live telemetry for clock state and loop/MIDI overrun counters and surfaces manifest-backed EEPROM health signals such as brownout count and primary/backup validity.
- **Device Monitor Stack** – The runtime coalesces telemetry into approximately 50 ms frames. Contract or receipt failures are shown as degraded, uncertain, or resynchronizing until device truth is established.
- **Staged Transaction Bar** – A fixed viewport bar is the sole primary Apply surface. It counts staged changes, groups Review rows by slot or subsystem, labels live and staged values, puts validation errors first, and links back to changed targets. It also reports Applying, uncertain, resynchronizing, and device-different states without hiding the candidate. **Discard draft** asks for confirmation for larger, imported, or long-running drafts.
- **Schema-driven Forms** – Every control in the right-hand rail is rendered from `config_schema.json` (Filter, ARG, LEDs, EF assignments, and all slot knobs). The `FormRenderer` builds collapsible sections, clamps number fields to the schema’s bounds, and stages each edit immediately; **Apply staged changes** batches the staged JSON through `set_config`, while field-level writes can still travel through `runtime.applyPatch(...)/set_param` when a control wants an immediate RPC. Profile save/load/reset and macro/scene actions travel on their own native command paths instead of pretending to be config diffs. Keybindings still apply: slot focus follows arrow keys; hold `Shift` for coarse/fine nudging; and the simulator status pill keeps status events in sync even when the board takes a coffee break.
- **Browser-only Slot Notes** – `Slot label` and the MIDI badge live outside the device schema. They are stored in local browser state so reconnects keep those operator hints without pretending the firmware persisted them.
- **Stage / Configure / Lab** – New sessions start in **Configure**, keeping the workspace focused on everyday knob-to-MIDI mapping. Pick **Stage** at a gig, or open with `?mode=stage`, for a performance-safe dashboard with connection, live profile and scene recall, power safety, slot activity, envelope levels, and panic-combo help. Flip to **Lab** to reveal EF/ARG/fixed-LFO/filter tuning, LED settings, scope tools, MIDI monitor, full device monitor, import/export, and debug surfaces. The internal persisted values remain `stage`, `basic`, and `advanced` for compatibility. The single mode switch lives in the global top bar; Configure and Lab replace the large Stage dashboard with their workspaces. The choice is saved in `localStorage`, and glossary-style info badges explain jargon like EF and ARG in-place.
- **Compatibility Probe** – The transport bar now includes **Check compatibility**, which reports whether the current browser/session can use Web Serial and explains likely failure modes such as unsupported browsers, insecure origins, or cancelled device pickers.

## MIDI Monitor Panel

A new MIDI Monitor panel sits beside the transport controls. Toggle it open, grant Web MIDI, and the panel streams incoming/outgoing bytes into a capped (1k entries) log, keeping the DOM light even at a sustained 1k msgs/min. Select the desired output, set the BPM slider, and press **Start clock** to stream realtime `0xF8` pulses—the button flips to **Stop clock** while the clock runs and every tick logs as an outgoing entry so you can verify the bench flow without leaving the browser.

## Slot Architecture Cheat Sheet

- Every slot now exposes its per-slot envelope follower payload. The editor’s **Envelope Follower** block lets you park a follower index, pick a filter shape, and dial in frequency, Q, oversample count, smoothing, baseline, and gain without leaving the browser.
- EF assignment rows now use clickable slot chips per follower, so one follower can modulate multiple slots without typing comma-separated indices.
- Live and Stage slot grids now color-code slot types so CC, Note, program-style, NRPN/RPN, and SysEx lanes are scannable at a glance.
- Stage slot highlights pulse briefly when telemetry changes; a stationary nonzero value remains readable without looking continuously active.
- The Lab Scope keeps a separate rolling history for every manifest-advertised envelope follower. **EF traces: Active** uses firmware `efStatus[]` plus a 1.5-second visual hold so transient movement fades instead of disappearing; **All** reveals every EF trace. LFOs remain visible in both modes as secondary dashed traces. The persistent view line names Active/All and any temporary EF focus, while **Leave solo** returns to the underlying view. Inactive histories continue recording, and the no-active-EF state says so explicitly.
- Stage includes a collapsed, read-only **Motion** drawer with the same Active/All/EF-focus behavior and LFO identities but none of Lab's FPS, refresh, or snapshot diagnostics. Its summary reports current/recent EF activity and persistent LFO visibility. It records history while closed and pauses canvas animation until opened.
- EF1–EF6 keep stable colors across Scope traces, Stage meters, assignment labels, and slot modulation badges; LFO1–LFO2 use the same shared identity palette in Scope and slot badges.
- Stage envelope meters report firmware active/idle state and configured route counts. With current firmware, selected-slot status shows the raw baseline, each resolver-stage EF/LFO delta, and the resolved output while retaining current source values as context. Older firmware falls back to its reported slot value plus EF/LFO source levels. The clock summary identifies internal/external source plus the applicable BPM and explicitly reports internal fallback while external follow is armed but its signal is missing.
- Browser-local slot labels appear in Stage slot cells and lead the selected-slot summary without entering firmware config. Expand an EF meter's route count to reveal the labeled Stage destinations it currently targets.
- ARG lives per-slot too. The **ARG Combiner** panel flips the enable flag, locks the math method, and routes sources A/B with the same coarse/fine nudging as the hardware encoders.
- The slot details panel mirrors the new firmware schema: EF index, filter, dynamics, baseline/gain, ARG mode and source map, and both fixed LFO lanes stream live next to the MIDI stats.
- Live and Stage slot grids show compact configured-source badges (`E#`, `A`, `L1`, and `L2`) so modulated slots remain apparent outside the editor. These badges describe configuration; the numeric slot readout prefers the device-reported resolved `slotOutputs` value and falls back to legacy `slots` telemetry.
- `runtime.js` normalises staged JSON so the firmware always gets properly-shaped `ef`/`arg` bundles, and it now digests live `slot_patch` frames from the bridge to keep the UI in sync when the hardware mutates slots.

## Accessibility & Controls

- Every control carries a label and large hit target. Slot focus follows arrow keys; hold `Shift` for coarse/fine changes on numeric inputs just like the hardware.
- The connection pill announces changes through an ARIA live region, and telemetry painting is throttled to animation frames so assistive tech stays responsive.
- Selecting a SysEx slot reveals a hex template field. Enter space-separated bytes plus `XX`/`MSB`/`LSB` placeholders—the UI normalises the case and the firmware swaps in live values on send.

## Runtime Contract

- Transport handshake is `hello` → `get_manifest` → `get_schema` → `get_config`. On simulator transport those travel as JSON-RPC; on native WebSerial and raw bridge transport the runtime maps them to `HELLO`, `GET_MANIFEST`, `GET_SCHEMA`, and `GET_CONFIG` before trusting any config payload. Hardware that advertises `capabilities.chunked_reads.config` is queried with `GET_CONFIG_CHUNKED`; the App checksum-verifies and reassembles its `read_chunk` frames before accepting the config. `GET_CONFIG` remains the compatibility path. When a structured bridge session is available, the runtime hydrates from `/api/device/session` and `/ws/events` first, with raw `/ws` retained for compatibility and live-control RPCs.
- Profile Arpeggiator assignments are explicit and optional. A profile stores zero-based `assigned_slots`; recalling it arms those slots for the hardware arp toggle but never starts note output. Live Arp start remains a direct, non-persisted override.
- The transport-mode truth table for direct WebSerial, bridge session, bridge raw `/ws`, and simulator lives in [../docs/app/AppTransportTruthTable.md](../docs/app/AppTransportTruthTable.md).
- Live runtime RPCs are separate from staged config writes: the configurator now uses `GET_NOTE_DYNAMICS` / `SET_NOTE_DYNAMICS`, `GET_JITTER` / `SET_JITTER`, `GET_CLOCK` / `SET_CLOCK`, and `GET_USB_MIDI` / `SET_USB_MIDI` for direct-control lanes.
- For the bridge path, staged config writes versus live performance writes are documented in [../docs/bridge/BridgeWriteLanes.md](../docs/bridge/BridgeWriteLanes.md).
- The runtime keeps separate `liveConfig` and `stagedConfig` snapshots. Apply captures an immutable candidate; edits made while it is in flight remain a separate next draft. Verification promotes device truth for that candidate to live state while retaining the newer draft as dirty.
- Device readback and profile/scene recall paths hydrate verified truth through the explicitly named `hydrateAuthoritativeConfig()` boundary. Browser files and presets must use `stage()` and cannot mark themselves authoritative.
- The diff panel is computed from `liveConfig` vs `stagedConfig`, which is why it can remain truthful even while device patches are streaming in.
- The runtime buffers inbound telemetry into approximately 50 ms state frames so frequent state messages do not turn the DOM into soup; visual panels may animate those snapshots independently.
- Outbound pot changes are debounced to ≥24 ms through a shared utility so every control shares the same cadence.
- `runtime.applyPatch(path, value)` stages a field locally first, then routes a `{rpc:"set_param"}` call through the same RPC lane. The simulator applies it immediately; native firmware defers those fine-grained writes until the next full Apply because the production contract is config-oriented. If live preview fails, the intentional edit remains staged and the failure is reported.
- Full Apply sends `set_config` with schema version, manifest metadata, an immutable staged candidate, and a SHA-256 checksum. Once any Apply bytes have been transmitted, a missing or malformed receipt enters an uncertain/resynchronizing state and reads device configuration back; it never claims a local rollback restored hardware. An unsent structured-Bridge draft survives disconnect/reconnect and is reconciled after the first authoritative session snapshot.
- Browser-only slot metadata (`label` and the MIDI badge) is stored separately in `localStorage`, merged back into the UI on read, and never included in `Apply` or schema diffing.
- Browser-only profile names are stored separately in `localStorage` and shown ahead of A–D identifiers in the header and Stage controls without entering firmware config or dirtying a draft. Firmware-backed scene names are mirrored into Stage after the scene directory is synchronized.
- Stage distinguishes selected recall targets from confirmed performance state: **Active Profile** follows manifest/telemetry or a successful profile RPC, while **Last recalled scene** is shown only after a successful browser-observed recall and resets when the active profile changes or the device disconnects.
- Stage mirrors the runtime telemetry health boundary as **offline**, **waiting**, **live**, **delayed**, or **stale**. Delayed/stale readings remain visible as last-known values but are de-emphasized rather than continuing to look live.
- Device serialization includes every changed per-slot EF payload, including advanced-only fields and intentional resets back to default values; default-looking data is not discarded when it differs from verified device truth.
- Schema mismatches put the device contract into `migration-required` and block Apply. The App currently supports export and inspection only; do not register or advertise migration adapters until their transform, validation, diff, and operator-confirmation flow is implemented.
- Last-used USB IDs and the last staged snapshot are remembered in `localStorage`; on load the app nudges you to reconnect but never reopens without a user gesture (WebSerial rules).

## Simulator & CI Hooks

`runtime.js` exports a simulator transport that mimics the firmware handshake, config payload, and telemetry frames. CI (or your own tests) can connect, screenshot the UI, and run migrations without a Teensy on the desk.

## Headless Harness

Need proof the simulator flow hasn’t rotted without booting Chrome manually? Run:

```bash
npm --prefix App test
```

## Testing & CI

- `npm --prefix App test` runs the UI suite.
- Playwright spins up `App/tests/dev-server.mjs` and currently targets `/benzknobz.html` as the stable harness entry.
- The CI pipeline verifies `npm --prefix App test` alongside the firmware and bridge checks; keep this command green before merging.

That spins up a tiny static server, launches Playwright’s headless Chromium, and imports the real `runtime.js` + `views/benzknobz.js`. The script walks through the README workflows—arming the simulator, driving the staged diff validator, forcing an ACK mismatch to trigger rollback, rewriting the manifest on the fly to rehearse the migration dialog, and finally flipping the simulator back off once a clean apply lands. When the test passes you know WebSerial ergonomics (and the migration guardrails) survived without babysitting a browser window.

## Troubleshooting

- Serve over HTTPS or `http://localhost` or the browser will block WebSerial.
- If Connect reports `No device selected`, the browser picker was cancelled before a port was granted. Click **Connect** again and choose the MOARkNOBS device explicitly.
- If the status pill sulks, open the Debug Log panel to watch the raw JSON feed.
- Schema validation errors show up in the diff panel—fix them before Apply will enable.

Stay punk, document the weird edge cases, and ship patches with swagger.
