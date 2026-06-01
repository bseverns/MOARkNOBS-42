# App Runtime Refactor Plan

## Goal

Thin [App/runtime.js](/Users/bseverns/Documents/GitHub/benzknober/App/runtime.js) into smaller runtime modules without changing operator-facing behavior, transport semantics, staged/live config rules, or existing Bridge/WebSerial/simulator support boundaries.

## Current Responsibilities In `runtime.js`

`runtime.js` still owns these distinct responsibilities:

| Responsibility                | Current Shape In `runtime.js`                                                                                       | Primary Collaborators                                                                          |
| ----------------------------- | ------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| Transport mode selection      | Resolves direct WebSerial vs Bridge raw `/ws` vs Bridge structured session vs simulator, plus Bridge fallback rules | `runtime/transports.js`, `runtime/bridge_session_client.js`                                    |
| Direct WebSerial              | Requests ports, remembers preferred USB filters, opens native transport, runs handshake                             | `runtime/native_transport.js`, `runtime/connection_handshake.js`, `runtime/port_preference.js` |
| Raw Bridge                    | Resolves `/ws`, opens raw WebSocket transport, preserves JSON-RPC compatibility                                     | `runtime/ws_transport.js`, `runtime/rpc_kernel.js`                                             |
| Structured Bridge session     | Warms `/api/device/session`, opens `/ws/events`, syncs staged/live state, falls back to raw Bridge if needed        | `runtime/bridge_session_client.js`, `runtime/config_session.js`                                |
| Simulator                     | Creates local manifest/config transport, exposes test-friendly runtime                                              | `runtime/simulator_transport.js`, `manifest_contract.js`                                       |
| Config session                | Owns staged vs live config, diffing, apply, rollback, snapshot restore, browser metadata merge                      | `runtime/config_session.js`, `runtime/local_slot_meta.js`, `runtime/state_snapshot.js`         |
| Live controls                 | Pot guard state, `set_param`, macro commands, scene commands, configurator boot request                             | `runtime/rpc_kernel.js`, runtime emitters                                                      |
| Telemetry                     | Buffers partial telemetry frames, assembles traces, emits status/telemetry events                                   | `runtime/line_router.js`                                                                       |
| Local slot metadata           | Browser-only slot labels/notes merged over live firmware config                                                     | `runtime/local_slot_meta.js`                                                                   |
| Macro / scene / profile calls | Serializes command lifetimes and timeouts around firmware response lines                                            | `runtime/rpc_kernel.js`, `runtime/line_router.js`                                              |
| Event emission                | Internal event bus and status fan-out for view controllers                                                          | local emitter helpers                                                                          |

## Proposed Extraction Targets

### `runtime/transport_mode.js`

Move the pure transport-mode helpers first:

- Bridge base URL and `/ws` resolution
- structured-session preference resolution
- active transport mode reporting
- shared mode/fallback predicates used by `connect()` and `getState()`

This is the safest first extraction because it is mostly deterministic input/output logic and already covered by Playwright transport-mode tests.

### `runtime/bridge_session_runtime.js`

Move structured Bridge session behavior next:

- Bridge session client creation
- warm session fetch
- `/ws/events` handling
- staged-config sync scheduling
- raw-Bridge fallback status reporting

This should leave `runtime.js` responsible for sequencing rather than Bridge-session cache management details.

### `runtime/live_controls_runtime.js`

Move live command helpers:

- macro pending lifecycle
- scene pending lifecycle
- configurator boot request
- pot-guard helpers
- `applyPatch()` and related `set_param` staging bridge

This separates staged-config persistence concerns from transient live performance control paths.

### `runtime/telemetry_runtime.js`

Move telemetry assembly:

- chunk merge rules
- trace-aware frame buffering
- flush timing
- emission of telemetry/status payloads

This reduces non-transport state in `runtime.js` and makes telemetry behavior independently testable.

## Suggested Order

1. Extract `runtime/transport_mode.js`.
2. Extract `runtime/telemetry_runtime.js`.
3. Extract `runtime/bridge_session_runtime.js`.
4. Extract `runtime/live_controls_runtime.js`.
5. Re-evaluate whether the local emitter/diff helpers should stay in `runtime.js` or move to utility modules.

## Guardrails

- Do not change transport labels, fallback semantics, or staged/live/browser boundaries.
- Do not collapse Bridge structured session and raw `/ws` into one abstraction; the split is intentional and operator-visible.
- Keep simulator behavior test-friendly and deterministic.
- Preserve the current `getState().transportMode` values:
  - `direct-webserial`
  - `bridge-raw`
  - `bridge-session`
  - `simulator`
- Prefer pure helper extractions first so existing Playwright suites prove no behavior drift.

## Evidence To Rely On During Refactor

The most relevant existing coverage is in:

- [App/tests/bridge_structured_transport.spec.js](/Users/bseverns/Documents/GitHub/benzknober/App/tests/bridge_structured_transport.spec.js)
- [App/tests/ui_mode.spec.js](/Users/bseverns/Documents/GitHub/benzknober/App/tests/ui_mode.spec.js)
- [App/tests/connection_banner.spec.js](/Users/bseverns/Documents/GitHub/benzknober/App/tests/connection_banner.spec.js)
- [App/tests/native_transport.spec.js](/Users/bseverns/Documents/GitHub/benzknober/App/tests/native_transport.spec.js)
- [App/tests/state_boundary.spec.js](/Users/bseverns/Documents/GitHub/benzknober/App/tests/state_boundary.spec.js)
- [docs/app/AppTransportTruthTable.md](/Users/bseverns/Documents/GitHub/benzknober/docs/app/AppTransportTruthTable.md)
