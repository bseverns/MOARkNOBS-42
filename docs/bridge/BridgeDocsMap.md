# Bridge Docs Map

> **Doc class:** Orientation. Use this page to choose the right Bridge document without flattening performer, operator, contract, and packaging material into one long page.

The Bridge docs are intentionally split because the Bridge serves several audiences: performers, desktop operators, App/runtime developers, release packagers, and host-integration testers.

## Start Here By Job

| Job                                     | Read First                                                                                                                         | Why                                                                       |
| --------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| I need the rig working before rehearsal | [Bridge for Performers](../guides/BridgeForPerformers.md)                                                                          | One-page show/rehearsal checklist.                                        |
| I am operating the browser console      | [Bridge Console Tour](BridgeConsoleTour.md)                                                                                        | Explains Setup, Stage, Advanced, snapshots, and recovery actions.         |
| I need the full Bridge runbook          | [bridge/README.md](../../bridge/README.md)                                                                                         | Canonical Bridge behavior, support boundary, install/run/package details. |
| I am wiring App-over-Bridge behavior    | [Bridge Transport Contract](BridgeTransportContract.md)                                                                            | HTTP/WebSocket surfaces and structured session behavior.                  |
| I am debugging writes or dirty state    | [Bridge Write Lanes](BridgeWriteLanes.md)                                                                                          | Staged config writes versus live performance writes.                      |
| I am preparing release artifacts        | [Bridge Signing Plan](../release/BridgeSigningPlan.md)                                                                             | Signing, notarization, and unsigned-artifact boundaries.                  |
| I am checking host setup evidence       | [Known Good Host Recipes](../reference/KnownGoodHostRecipes.md) and [Bridge host receipts](../bench/bridge-host-recipes/README.md) | Separates setup recipes from observed receipts.                           |

## Current Split

- **Performer docs** answer what to run and what to check before a set.
- **Console docs** explain the desktop UI and operator state.
- **Contract docs** define transport surfaces and write lanes.
- **Packaging docs** explain artifacts, signing, and release boundaries.
- **Evidence docs** record what was actually observed on a host, browser, Bridge, or board.

## Upgrade Status

The old Bridge runtime upgrade plan has been retired because its relevant work is now represented by current code and contract docs.

| Upgrade Area                    | Current Home                                                                                    | Status                                                                                                                                  |
| ------------------------------- | ----------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| Device session layer            | `bridge/lib/device/session.js`, [Bridge Transport Contract](BridgeTransportContract.md)         | Implemented as cached manifest/schema/live/staged/dirty/apply state.                                                                    |
| Bridge simulator                | `bridge/lib/device/simulator.js`, `bridge/test/simulator.test.js`                               | Implemented for handshake, ACK modes, and reconnect coverage.                                                                           |
| Structured App-facing transport | `/api/device/*`, `/ws/events`, [Bridge Transport Contract](BridgeTransportContract.md)          | Implemented and contract-tested.                                                                                                        |
| Staged apply/write discipline   | [Bridge Write Lanes](BridgeWriteLanes.md), `bridge/test/device_session.test.js`                 | Implemented as schema-validated staged writes with ACK/rollback boundaries.                                                             |
| Browser console role split      | [Bridge Console Tour](BridgeConsoleTour.md)                                                     | Implemented as Setup, Stage, and Advanced operator modes.                                                                               |
| Artifact hardening              | [bridge README](../../bridge/README.md), [Bridge Signing Plan](../release/BridgeSigningPlan.md) | Unsigned CI artifacts now include checksums, README, license payload, manifest metadata, and smoke checks. Signing remains future work. |

The remaining Bridge work should live in focused docs instead of a broad upgrade plan:

- signing and installer readiness belong in [Bridge Signing Plan](../release/BridgeSigningPlan.md)
- transport/API behavior belongs in [Bridge Transport Contract](BridgeTransportContract.md)
- staged/live write behavior belongs in [Bridge Write Lanes](BridgeWriteLanes.md)
- operator workflow belongs in [Bridge Console Tour](BridgeConsoleTour.md)
- host evidence belongs in [Bench Receipts](../bench/README.md)

## What Not To Merge Yet

Do not merge the Bridge docs into one giant handbook until the current simplified nav has had time to prove itself. The current split keeps three boundaries visible:

- live performance writes are not staged config writes
- setup recipes are not host compatibility claims
- unsigned CI artifacts are not signed public installers

The next useful bridge-doc edit is not a merge. It is to keep cross-links current and make sure each page says whether it is a performer guide, operator tour, contract, packaging note, or evidence receipt.
