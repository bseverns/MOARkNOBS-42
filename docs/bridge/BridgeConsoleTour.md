# Bridge Console Tour

The Bridge console is the operator-facing desktop view for `MN42 Bridge`. Use it when you need desktop OSC/MIDI routing, a cached device session, or an App-over-bridge path that does not depend on browser WebSerial support.

For the full Bridge doc split, see [Bridge Docs Map](BridgeDocsMap.md).
For transport and support-boundary tie-breaks, see [Bridge README](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md), [Bridge Transport Contract](./BridgeTransportContract.md), and [Bridge Write Lanes](./BridgeWriteLanes.md).

## Before You Start

- Start the console with `npm --prefix bridge start`.
- Open `http://127.0.0.1:8787/`.
- Use Node `24.x` only; the Bridge package still pins `>=24 <25`.

Screenshot note: the images below were recaptured on 2026-08-03 from the local Bridge console against the repository's MN42 device simulator. They show the current operator labels and a complete cached Bridge session, but are UI documentation—not hardware validation evidence.

Simulator session example from this capture:

- serial path: `/dev/simulated-mn42`
- firmware identity: `MOARkNOBS-42` / schema `8` / git `simulated`
- power boundary: `POWER_CHOKED_V1`
- LED cap: `26`
- rail state: `unverified`

## Setup Mode

![Bridge console Setup mode with serial port entry, recipe picker, and start controls](../images/bridge-setup-mode.png)

Setup mode is where a non-developer operator should start.

This capture shows a complete setup path, including the simulator serial path, a simulator MIDI label, and the top-line `Bridge live and cached device session ready.` status after the operator has started the Bridge.

- `Serial port` is the firmware USB path. Pick a detected entry from the list or type it manually if the detector missed it.
- `MIDI port label` should match the host loopback port you intend to use.
- `OSC host`, `OSC send port`, `OSC listen port`, and `OSC bind address` define where the Bridge publishes and listens.
- `Feedback guard` and `alert suppression` tune operator safety and log noise; leave them at defaults unless you have a documented host reason to change them.
- `Known-good recipe` prefills documented host settings and shows a short checklist. It is the fastest safe path for an operator who is not debugging custom routing.
- `Start bridge` launches the desktop runtime. `Stop bridge` disconnects the serial/MIDI/OSC runtime cleanly. `Refresh ports` rescans serial and MIDI devices.

### Serial Port Selection

The serial chooser is intentionally simple:

- Prefer the detected list first.
- If the board appears as `/dev/tty.*` on macOS, the console will prefer the matching `/dev/cu.*` path for you.
- If nothing is detected, you can still type the port path manually and start the Bridge.

## Stage Mode

![Bridge console Stage mode with connection-health cards, cached device session, and operator actions](../images/bridge-stage-mode.png)

Stage mode is the compact operator view. It is meant to answer “is the Bridge healthy?” without exposing raw debugging detail.

In the screenshot above, the simulated device is connected, `Device` is `ready`, and the cached session shows the simulator's `POWER_CHOKED_V1` boundary with `LED cap 26` and `rail state unverified`.

- `Bridge`, `Serial`, and `Device` show whether the desktop runtime is running, whether the serial link is up, and whether the device session is actually ready.
- `Telemetry`, `RT p95`, and `Jitter p95` summarize whether the runtime is seeing fresh traffic and whether round-trip timing is staying inside configured targets.
- `Cached device session` shows the last known firmware identity, schema version/source, power profile, LED cap, rail state, dirty state, and last apply result.
- `Operator actions` keeps only the show-safe actions visible:
  - `Open configurator`
  - `Download snapshot`
  - `Refresh state`

### What “Session Ready” Means

The console reports the session as ready only after the Bridge has cached all of the following:

- `HELLO`
- `GET_MANIFEST`
- `GET_SCHEMA`
- `GET_CONFIG`

Until then, Stage mode may show `awaiting HELLO`, `awaiting handshake`, or empty cached-session fields. That is expected during startup or after a reconnect.

In the screenshots for this tour, `ready` means the Bridge has already cached device identity, schema authority, a live config snapshot, and conservative power-boundary fields. Confirm those values on attached hardware before relying on them for a release or performance decision.

### App Launch

`Open configurator` launches `/app/` through the Bridge server, not through direct browser WebSerial.

That means:

- the App prefers the structured Bridge session when available
- the App can still fall back to raw Bridge `/ws` compatibility if structured session setup fails
- desktop OSC/MIDI routing can stay active while the operator uses the configurator

## Advanced Mode

![Bridge console Advanced mode with runtime status, raw serial output, logs, route traces, and state JSON](../images/bridge-advanced-mode.png)

Advanced mode is the diagnostic view. It is still operator-usable, but it assumes you need evidence rather than a quiet status board.

This simulator capture shows three useful student-facing examples at once:

- the runtime and cached session are present
- the raw serial/debug lane and route trace surfaces are available for diagnosis
- the state JSON includes the same cached power and firmware identity fields shown in Stage mode

- `Runtime status` expands the performance and error counters: OSC/MIDI endpoints, last route, source timestamp/skew, round-trip samples, parse drops, command drops, and feedback suppression counts.
- `Mappings and guards` shows the active MIDI-to-OSC mapping snapshot and guard settings.
- `Route traces` shows recent routed events at a human-readable level.
- `Raw serial / debug lane` is the direct line-oriented device feed. Use it only when you need to confirm exactly what the firmware emitted.
- `Bridge log` is the console/service log stream.
- `State JSON` is the most complete machine-readable snapshot of what the Bridge currently believes.

## Student Demo Path

For a classroom walk-through, use the same order the console uses:

1. Start in `Setup` and point out the exact serial port and MIDI label the host will use.
2. Switch to `Stage` and wait for `Device` to become `ready`.
3. Call out the cached session facts that matter most:
   - firmware identity
   - schema version/source
   - power profile / LED cap / rail state
   - dirty state and last apply result
4. Open `Advanced` only after the students understand that `Stage` is the operator truth surface and `Advanced` is the evidence/debug surface.

That sequence keeps the lesson anchored on operator decisions first, then on lower-level traces.

## Warnings And Alerts

Warnings surface in two places:

- the top summary line and status cards
- the `Active alerts` list in Stage mode

Use `Clear alerts` when you intentionally want a fresh operator slate after acknowledging an issue. Do not clear alerts just to hide an unresolved fault.

Typical meanings:

- `awaiting handshake` or `awaiting HELLO`: serial link exists, but the device session is not ready yet
- `dirty: true`: staged Bridge config differs from live config and has not been promoted by a verified ACK
- `RT health` degraded or alert count rising: route timing or command health is outside the configured target window

## Snapshot, Disconnect, And Recovery Actions

The Bridge console exposes these operator actions today:

- `Stop bridge`: the practical disconnect action for the desktop runtime
- `Download snapshot`: saves the current Bridge/session state for evidence or support review
- `Refresh state`: re-reads current Bridge state into the console
- `Reset metrics`: clears accumulated round-trip metrics so you can measure a fresh run

There is no dedicated Bridge-console `panic` button today. If you need deeper recovery guidance or staged-config recovery tools, use `Open configurator` and work from the App’s recovery surfaces.

## Raw Serial / Debug Lane Boundary

The raw serial pane is useful, but it is not the primary operator lane.

- Use `Stage` first for health decisions.
- Use `Advanced` only when you need evidence, traces, or direct line-level confirmation.
- Do not infer staged-config success from raw lines alone when the cached session/apply state is available; the Bridge’s structured session is the authoritative operator surface for staged/live/apply state.
