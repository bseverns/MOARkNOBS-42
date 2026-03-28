# Fit / Finish Audit

Date: 2026-03-28

Scope: firmware + bridge + app + operator-facing docs, treated as one instrument/system.

Status: the original seam-level release blockers from this audit have been resolved in the repo. This file now tracks what remains before the whole stack feels fully proven to an outside user.

## Current strengths

- The app, bridge, and firmware now agree on the production transport contract. Native app sessions adapt to `HELLO`, `GET_MANIFEST`, `GET_CONFIG`, `GET_SCHEMA`, and `SET_ALL` in `App/runtime.js`, while live host control uses `SET_SLOT_VALUE` in `bridge/lib/bridge_service.js` and `firmware/src/protocol/Protocol.cpp`.
- Browser profile actions are now real firmware behavior rather than simulator-only promises. `firmware/src/protocol/Protocol.cpp` implements `SAVE_PROFILE`, `LOAD_PROFILE`, and `RESET_PROFILE`, `firmware/src/protocol/ManifestReport.cpp` advertises them, and `App/views/benzknobz.js` drives those actions through the native transport layer.
- Macro snapshot and scene storage now have EEPROM-backed firmware support instead of stubbed failure paths. The storage and recall flow lives in `firmware/src/protocol/Protocol.cpp`, with matching UI support in `App/views/benzknobz.js` and operator copy in `App/index.html`.
- Browser-local slot metadata is now clearly separated from device truth. `App/config_schema.json`, `App/runtime.js`, and `App/views/benzknobz.js` treat labels, pickup guards, and the MIDI badge as local operator aids instead of firmware-backed config.

## Resolved since the original audit

- App configurator transport mismatch: resolved by the native command adapter in `App/runtime.js`.
- Bridge inbound OSC/MIDI command mismatch: resolved by the `SET_SLOT_VALUE` path in `bridge/lib/bridge_service.js` and matching docs/tests.
- Browser profile save/load/reset mismatch: resolved by real firmware commands in `firmware/src/protocol/Protocol.cpp` plus manifest support in `firmware/src/protocol/ManifestReport.cpp`.
- Stubbed macro/scene UI: resolved by EEPROM-backed storage in `firmware/src/protocol/Protocol.cpp` and enabled app controls in `App/views/benzknobz.js`.
- Browser-local slot fields presented as device truth: resolved in `App/config_schema.json`, `App/runtime.js`, and `App/views/benzknobz.js`.

## Prioritized remaining issues

1. `trust risk` New recovery/storage paths are implemented, but not yet proven on a real board end-to-end

Evidence

- The operator flow now depends on real device-backed profile, macro, and scene storage in `firmware/src/protocol/Protocol.cpp`.
- The current hardware runner in `firmware/system_test/mn42_fullstack_runner.js` still focuses on handshake, telemetry, and live slot poke, not the newly added EEPROM-backed profile/macro/scene flows.
- Release-facing docs now point operators toward those flows in `docs/ProfileWorkflow.md`, `docs/ValidationFlow.md`, and `docs/DemoTestPunchList.md`.

Why this matters

- The repo now tells a coherent story, but a first serious outside user will still care whether save/load/reset and snapshot recovery were exercised on real hardware, not just implemented and simulated.

Reproduction

1. Run `npm --prefix App test` and `npm --prefix bridge test`.
2. Connect a real board and perform the demo path manually.
3. Notice there is no automated artifact yet proving `SAVE_PROFILE`, `LOAD_PROFILE`, `RESET_PROFILE`, `SAVE_MACRO_SLOT`, `RECALL_MACRO_SLOT`, `SAVE_SCENE`, and `RECALL_SCENE` all survive real EEPROM + reconnect conditions.

Recommended small patch

- Extend `firmware/system_test/mn42_fullstack_runner.js` to exercise one full profile save/load/reset cycle, one macro save/recall cycle, and one scene save/recall cycle.
- Capture those results in `docs/TESTING.md` as a named release smoke check rather than leaving them implied.

2. `friction` The firmware-side automated regression story is better, but still light on direct assertions for the new commands

Evidence

- Parser coverage exists in `firmware/test/test_protocol_dispatch.cpp`, but the broader Unity runner is still the main firmware proof path in `firmware/test/test_mainUnity.cpp`.
- The new profile/macro/scene commands now carry storage and recovery semantics in `firmware/src/protocol/Protocol.cpp`, which are more stateful than the earlier bridge/app seam fixes.

Why this matters

- EEPROM-backed recovery behavior is exactly where small regressions create mistrust.
- Even with the new direct dispatch checks, the storage paths deserve a little more targeted firmware coverage than they have today.

Reproduction

1. Change the profile or scene storage logic in `firmware/src/protocol/Protocol.cpp`.
2. Run only host-side app/bridge tests.
3. It is possible to miss a firmware-only regression until a bench pass.

Recommended small patch

- Add one focused firmware test file that asserts profile reset defaults, profile save/load round-trip, and macro/scene snapshot round-trip at the config level instead of only through command dispatch.

## Release finish summary

This repo no longer feels unfinished because of a contract mismatch. The remaining work is proof, not product-shape confusion:

- prove the new EEPROM-backed recovery flows on a physical board
- tighten firmware-level regression coverage around those storage paths

## Top issues worth fixing first

1. Add a real-board system test pass for profile save/load/reset and macro/scene recall.
2. Add one focused firmware regression test file for storage round-trip behavior.

## Proposed commit plan

### PR 1: Hardware proof of recovery flows

- Extend `firmware/system_test/mn42_fullstack_runner.js` to cover profile, macro, and scene storage.
- Update `docs/TESTING.md`, `docs/ValidationFlow.md`, and `docs/DemoTestPunchList.md` with the exact proof path and expected artifacts.

### PR 2: Tighten firmware storage regression coverage

- Add focused Unity coverage for profile reset defaults and macro/scene/profile round-trips.
- Keep the scope inside the current storage/command contract; no protocol redesign.
