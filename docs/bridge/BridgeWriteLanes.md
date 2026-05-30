# Bridge Write Lanes

This page defines the bridge write surfaces and clarifies which ones are staged configuration lanes versus live performance lanes.

This is a contract doc. For tie-break rules, see [Documentation Truth Map](../reference/DocumentationTruthMap.md).

## Summary

The bridge has one guarded config-write lane and several live-control lanes.

- Guarded config lane: `/api/device/stage` plus `/api/device/apply`
- Live/control lanes: raw `/ws`, OSC `/mn42/cmd`, OSC `/mn42/event/*`, and inbound MIDI CC

The important rule is simple: live-control writes must not silently become staged config edits.

## Lane table

| Lane                                      | What it writes                                                                                                                        | Schema validated                                           | Persisted                                                                            | Affects `stagedConfig` dirty state                                                                      | Rollback behavior                                                                                  | Intended use                                                  |
| ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- | ------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| `/api/device/stage` + `/api/device/apply` | Full config object staged in the bridge session, then chunked `SET_ALL` to firmware on apply                                          | Yes, bridge session validates staged config before apply   | Yes, but only after firmware ACK on apply                                            | Yes. `stage` sets dirty when staged differs from live. `apply` clears dirty only after ACK or rollback. | Automatic rollback on timeout, checksum mismatch, device error, or explicit `/api/device/rollback` | Operator-facing config edits and App staged/apply workflow    |
| Raw `/ws`                                 | Raw newline firmware commands and responses, including live commands such as `SET_SLOT_VALUE` and any manually typed firmware command | No bridge-side schema validation                           | Not bridge-managed. Persistence depends on the raw firmware command the caller sends | No. Raw writes do not update the bridge session staged cache                                            | None from the bridge session. Caller owns the consequences                                         | Compatibility, debugging, and low-level tooling               |
| OSC `/mn42/cmd`                           | Validated live `SET_SLOT_VALUE` command payloads forwarded to firmware                                                                | Payload shape/range validation only, not schema validation | No                                                                                   | No                                                                                                      | None                                                                                               | Live performance control and host-driven slot value injection |
| OSC `/mn42/event/*`                       | Typed event lane. Sends host MIDI-style events; CC events in slot range also mirror to live `SET_SLOT_VALUE`                          | Event normalization only, not schema validation            | No                                                                                   | No                                                                                                      | None                                                                                               | Typed live performance events and host routing                |
| MIDI CC input                             | Inbound MIDI CC converted to live `SET_SLOT_VALUE`; also rebroadcast as typed OSC event                                               | MIDI parse/range handling only, not schema validation      | No                                                                                   | No                                                                                                      | None                                                                                               | Live DAW/controller performance lane                          |

## Practical rules

### Staged config lane

- Use `/api/device/stage` to replace the bridge session's staged config.
- Use `/api/device/apply` to send the staged config to firmware.
- The bridge session tracks `liveConfig`, `stagedConfig`, `dirty`, and `lastApplyResult`.
- `dirty` clears only after firmware ACK or rollback.

### Live performance lanes

- Use OSC `/mn42/cmd` or inbound MIDI CC when you want immediate live slot-value control.
- Use OSC `/mn42/event/*` when you want typed host MIDI-style events and optional live CC mirroring.
- These lanes do not mutate the staged config cache and must not be treated as persisted config edits.

### Raw bridge websocket

- `/ws` is intentionally a raw back-compat/debug lane.
- Because it can carry arbitrary firmware commands, it bypasses bridge-side schema validation and staged/apply discipline.
- Do not treat successful raw `/ws` writes as evidence that the guarded staged config lane was exercised.
