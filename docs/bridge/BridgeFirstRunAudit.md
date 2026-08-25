# Bridge First-Run Audit

> **Doc class:** Internal UI audit. This page summarizes first-run friction in the Bridge console and docs. It does not replace the Bridge contract, host-compatibility docs, or bench receipts.

Inspected surfaces: `bridge/README.md`, `bridge/ui/`, `bridge/lib/http_bridge_server.js`, `bridge/lib/device/session.js`, `bridge/test/browser_bridge_server.test.js`, `bridge/test/device_session.test.js`, [Bridge Console Tour](BridgeConsoleTour.md), [Bridge For Performers](../guides/BridgeForPerformers.md), [Bridge Write Lanes](BridgeWriteLanes.md), `docs/bench/bridge/`, and `docs/bench/bridge-host-recipes/`.

## Required Concepts Before Success

- The Bridge is needed when direct WebSerial is not enough: OSC routing, host MIDI routing, desktop session cache, or App-over-Bridge.
- Node 24 is the supported desktop host lane in the repo.
- A serial device path must be chosen before the runtime can connect.
- A host recipe can prefill OSC/MIDI settings, but a recipe is not evidence by itself.
- The App opened from the Bridge uses `/app/` over the Bridge transport, not direct browser WebSerial.
- `Download snapshot` captures Bridge/session state for debugging or support review.
- Local-only matters: `/api/*`, `/ws`, OSC, and MIDI command lanes are trusted-local control surfaces.

## Controls Visible Before Connection

- Setup shows Start bridge, Stop bridge, Refresh ports, Download snapshot, and Open configurator.
- Stop bridge remains visible in every mode, is visually distinct, and requires confirmation before disconnecting routing.
- Reset metrics and Clear alerts are Diagnostics-only; clearing alerts requires confirmation.
- Console tabs: Setup, Routing, Monitor, Diagnostics.
- Setup leads with the serial port and host recipe; resolved MIDI/OSC destinations remain visible while raw route fields stay behind `Edit routing details`.
- Known-good recipe picker, requirements, and validation checklist.
- Browser-local named Performance Setups with explicit load, overwrite/delete confirmation, and versioned JSON import/export.
- Detected serial and MIDI port lists.

## Controls That Should Be Diagnostics-Only

- MIDI feedback guard window.
- Alert suppression window.
- Allow feedback loops.
- RT p95 target and RT jitter p95 target.
- Raw route JSON remains a power-user surface, while Routing leads with passive MIDI learn, recent OSC destinations, and an explicit preview/confirmation step.
- Raw serial/debug lane, route traces, state JSON, and detailed runtime counters.

The source keeps evidence-heavy controls in Diagnostics. Setup now hides raw host addressing and guard/timing controls until the operator deliberately opens the routing-details disclosure.

## Missing "Choose Recipe" Affordances

- The console has recipe selection, requirements, and checklist, but the first screen did not name the intended default path plainly enough.
- Recipes prefill ports, but the UI could better say: pick a host recipe first, then only edit the serial/MIDI fields if detection is wrong.
- The docs correctly warn that host receipts, not recipe files alone, define evidence-backed host paths.

## Missing Recovery/Snapshot Affordances

- `Download snapshot` exists globally and in Monitor.
- The snapshot API includes runtime, power-safety, and Bridge state metadata.
- The UI did not explain early enough that snapshot export is the support/evidence move when startup fails.
- There is no dedicated Bridge-console panic button, by design; deeper recovery remains in the App.

## Change Made From This Audit

- Added a first-run path callout to the Bridge console hero:
  1. Choose device.
  2. Choose host recipe.
  3. Start.
  4. Open App.
  5. Download snapshot if something fails.
- Added local-only security copy to the default screen.
- Made Setup recipe-first, added a resolved routing summary, and moved raw host fields plus guard/timing controls behind `Edit routing details`.
- Added browser-local named Performance Setups. Loading remains write-free until the operator deliberately starts the Bridge, and personal setups remain distinct from evidence-backed bundled recipes and firmware profiles.
- Renamed the visible console roles to Setup, Routing, Monitor, and Diagnostics while preserving internal mode identifiers and transport contracts.
- Reserved `Draft state` for the genuine staged device-configuration lane; host routing is described as active immediately or pending the next Bridge start.
