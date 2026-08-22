# App Behavior Contract

> **Doc class:** Contract. This page defines browser-configurator behavior. The concise [App README](https://github.com/bseverns/MOARkNOBS-42/blob/main/App/README.md) is the operational entry point.

## Runtime ownership

- `App/runtime.js` is the runtime composition root. It wires transport, contract, configuration-session, telemetry, and
  live-control services, but does not define their schema, transaction, or device-policy rules.
- `App/views/benzknobz.js` is the view composition root. It assembles panels/controllers and binds operator controls to
  the public runtime API; device truth and write policy must remain behind those dependencies.
- `App/runtime/` owns manifest/schema negotiation, state normalization, validation, staged diffs, checksummed Apply,
  uncertainty/resynchronization, telemetry coalescing, and transport-specific behavior.
- `App/config_schema.json` is the bundled schema 8 fallback. A compatible device-provided schema takes precedence.
- `App/benzknobz.css` defines shared visual tokens and mode presentation.

The view must not invent device truth. Identity, capabilities, configuration, persistence actions, and power-safety
metadata remain manifest-, schema-, command-, or telemetry-backed.

## Transport modes

The App supports direct WebSerial, structured Bridge session, raw Bridge compatibility, and simulator transports. Their
operator labels and truth boundaries are normative in [App Transport Truth Table](AppTransportTruthTable.md).

Direct WebSerial requires a secure context or localhost and a user gesture for port selection. The App may remember
USB identifiers and a staged snapshot, but it must not reopen a serial port without a user gesture.

## Handshake and compatibility

Connect remains busy until transport handlers and runtime subscriptions are bound. A successful device session obtains
identity, manifest, schema, and configuration before enabling authoritative editing. Before that handshake, bundled
fallback data must not be presented as attached-device truth.

A schema mismatch enters `migration-required` and blocks Apply. The App supports export and inspection in that state;
it must not advertise a migration transform until that transform, validation, diff, and operator confirmation flow is
implemented and tested.

## Draft and Apply semantics

The browser maintains live device state separately from a staged candidate. Edits, imported JSON, and recipe actions
update the candidate first. A write occurs only through an explicit action on the relevant write lane.

The primary configuration Apply sends a validated, checksummed candidate. A valid receipt plus authoritative readback
establishes device truth. Missing, malformed, or contradictory receipts enter uncertainty/resynchronization; they do not
prove rollback. The candidate stays visible until readback resolves the outcome.

Profile, scene, macro-recall, and transport actions use their native command paths rather than masquerading as config
diffs. Actions that can replace a dirty configuration draft must be blocked until the operator applies or discards it.
Schema-rendered controls stage browser intent synchronously and debounce only outbound live patches. Destroying or
rebuilding a rendered control must cancel its pending transmission without discarding the staged edit. Removing an
array item remains authoritative over timers owned by that item's former controls.

The complete state model is [Configuration Transaction Model](../reference/ConfigurationTransactionModel.md).

## UI modes

- **Stage** is the performance surface: connection, live profile/scene recall, power safety, slot activity, envelope
  levels, motion, and panic help.
- **Configure** is the default everyday mapping and profile workspace.
- **Lab** exposes complete EF, ARG, fixed-LFO, filter, LED, scope, MIDI-monitor, import/export, and diagnostic controls.

The persisted compatibility values remain `stage`, `basic`, and `advanced`. A mode may hide complexity, but it must not
change the authority or write semantics of the underlying state.

## Device-backed and browser-local state

Device-backed controls must be advertised by the manifest/schema or a documented live protocol lane. Browser-only slot
labels and MIDI badges remain local metadata and must not be serialized as device configuration. Optional display
metadata sent to the Bridge remains advisory and cannot affect routing authority.

Import stages configuration locally. Export saves the current candidate, including unsent edits. Profile A–D actions,
scene actions, macro actions, and live controls remain capability-gated so older firmware fails closed.

## Telemetry and visualization

The runtime coalesces telemetry into approximately 50 ms paint frames. Scope time is host-observed arrival time, not
firmware-source time or round-trip latency. EF and LFO traces, activity holds, and simulator traces are musical/operator
visualization; latency-grade claims require the [bench latency method](../bench/latency/method.md).

The simulator uses the same runtime API and deterministic synthetic telemetry. It is suitable for UI behavior,
automation, screenshots, schema/migration rehearsal, and staged-transaction tests. It is not analog calibration,
electrical, physical-control, transport-latency, or hardware-validation evidence.

## Accessibility and input

Controls require visible labels, keyboard operation, logical focus order, and non-color status cues. The selected-slot
surface supports arrow-key navigation and documented coarse/fine adjustment. Dialogs and status announcements must
remain usable without relying on pointer interaction or color alone.

## Verification

Run:

```bash
npm --prefix App test
npm --prefix App run test:architecture
```

Playwright exercises the real runtime/view modules through the stable `benzknobz.html` harness, including simulator,
schema validation, staged diff, Apply/receipt failures, uncertainty recovery, migration blocking, profiles, and the
mode surfaces. The architecture guard rejects coordinator imports that bypass the public layers and policy-shaped code
that would pull schema constraints, transaction semantics, or device capability decisions back into the two composition
roots. Operator-facing hardware claims additionally require receipts under `docs/bench/app/`.
