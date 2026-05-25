# Bridge Runtime Upgrade Plan

## Why this exists

`bridge/` already proves the MOARkNOBS-42 desktop path can:

- attach to the device over USB serial on a Node 24 host,
- forward telemetry to OSC and host MIDI,
- accept OSC or MIDI control input and translate it back onto the device line protocol,
- serve a local browser console at `http://127.0.0.1:8787/`,
- serve the App at `/app/`,
- expose a raw `/ws` text lane for debug and App-over-bridge transport.

That is useful, but it is still shaped like a transport helper. The App has moved further: it now treats the device as a schema-aware, manifest-driven, staged-state runtime with checksum-verified apply/rollback. The bridge should align with that contract so the desktop path is not a weaker side door.

## Current bridge contract

Current repo evidence shows these bridge seams:

- `bridge/lib/bridge_service.js`
  - owns serial, OSC, MIDI, logs, counters, performance alerts, and current browser-console state.
  - current device-awareness is shallow: `ready`, `manifest`, and last telemetry/route metadata.
- `bridge/lib/http_bridge_server.js`
  - serves the local browser console and `/app/`.
  - exposes `/api/state`, `/api/ports`, `/api/midi-ports`, `/api/connect`, `/api/disconnect`, and raw `/ws`.
- `bridge/ui/bridge-ui.js`
  - treats bridge state as one large dashboard blob.
  - is oriented around “start bridge / watch diagnostics”, not around setup vs performance vs deep debug roles.
- raw `/ws`
  - currently streams line-oriented text for compatibility/debugging.

This is intentionally conservative and should stay intact:

- CLI entrypoints remain valid.
- raw serial/WebSocket behavior remains available.
- Node 24 stays the tested support boundary.
- CI remains the source of unsigned packaged artifacts.

## App contract the bridge should align with

The App runtime already assumes:

- handshake: `HELLO` -> `GET_MANIFEST` -> `GET_SCHEMA` -> `GET_CONFIG`
- device truth comes from manifest and schema, not hardcoded UI guesses
- separate `liveConfig` and `stagedConfig`
- `dirty` derives from live vs staged state
- staged apply requires:
  - schema validation
  - manifest-aware capability checks
  - checksum generation
  - verified ACK before staged -> live promotion
  - rollback on timeout, mismatch, or disconnect
- browser-only convenience metadata is kept out of firmware state

The bridge should not bypass that discipline. OSC/MIDI and the browser console must respect the same schema/manifest boundaries as the App.

## Proposed bridge changes

### 1. Device session layer

Add `bridge/lib/device/` as the device-facing runtime surface.

Proposed responsibilities:

- serial handshake orchestration
- cached device session state:
  - hello/ready
  - manifest
  - schema
  - `liveConfig`
  - `stagedConfig`
  - `dirty`
  - `lastApplyResult`
  - power-safety fields
  - firmware identity fields
- line parsing for session-scoped replies and errors
- lifecycle handling for disconnect/reconnect

This session becomes the source of truth for bridge API state, structured transport events, and browser-console state JSON.

### 2. Structured App-facing transport

Keep raw `/ws` as-is for debug and compatibility.

Add a structured bridge event stream for host tools and `/app/` consumers:

- `device.ready`
- `device.telemetry`
- `device.config.live`
- `device.config.staged`
- `device.config.dirty`
- `device.apply.ack`
- `device.apply.rollback`
- `bridge.alert`
- `bridge.performance`

That stream should be documented and contract-tested so App-over-bridge has a stable desktop runtime surface.

### 3. Schema-aware validation and staged apply

Bridge-side config writes should follow the same logic as the App:

- stage config
- validate against the active schema
- reject unsupported or manifest-disallowed writes with machine-readable errors
- send bulk config over `SET_ALL`
- require a verified checksum ACK
- promote staged -> live only on verified success
- rollback on timeout, mismatch, disconnect, or firmware error

OSC/MIDI ingestion must not become a schema/capability bypass.

### 4. Bridge simulator

Add a simulated MN42 device for bridge tests so the session/apply transport can be exercised without depending on physical serial hardware.

The simulator should support:

- `HELLO`
- `GET_MANIFEST`
- `GET_SCHEMA`
- `GET_CONFIG`
- telemetry frames
- delayed ACK
- bad ACK
- disconnect/reconnect
- malformed responses
- schema mismatch

### 5. Browser console role split

Reframe the console into three operator modes instead of one giant dashboard:

- `Setup`
  - ports
  - connect/disconnect
  - config file
  - recipe selection
- `Stage`
  - connection health
  - OSC/MIDI status
  - RT p95/jitter
  - active alerts
  - panic/snapshot
- `Advanced`
  - raw serial
  - route traces
  - feedback guard
  - mappings
  - state JSON

This preserves diagnostics while making the bridge usable during a show.

### 6. Artifact hardening without pretending installer readiness

Release workflow behavior should stay conservative:

- CI still generates unsigned artifacts
- add checksums, per-target README, third-party licenses, and artifact manifest
- add smoke runs against packaged binaries where feasible
- document future signing separately instead of implying it exists now

## Non-goals for this pass

- no claim of signed installer readiness
- no broad DAW compatibility claims beyond recipe/docs/test evidence
- no removal of raw `/ws` or line-oriented debug behavior
- no change to the `http://127.0.0.1:8787/` console address
- no widening of the Node support boundary beyond 24.x without new proof

## Implementation order

1. Device session and simulator
2. Structured transport and contract docs/tests
3. Validation and staged apply/rollback discipline
4. Browser console role split
5. Presets/recipes docs
6. CI artifact hardening and signing-plan docs
