# Bridge Operator Reference

> **Doc class:** Reference. Use the concise [Bridge README](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md) to get running. This page collects settings, routing, CLI, and packaging details.

## Operating paths

| Path | Entry point | Purpose |
| --- | --- | --- |
| Browser console | `npm --prefix bridge start` | Setup, Routing, Monitor/Diagnostics status, passive soundcheck, and App launch |
| CLI | `node bridge/mn42_bridge.js …` | Lowest-level line-oriented routing lane |
| App over Bridge | Console **Open configurator** / `/app/` | Structured configuration while OSC/MIDI remain active |
| Raw debug | `/ws` | Newline serial compatibility and live RPC debugging |
| Structured session | `/api/device/*` and `/ws/events` | Cached manifest/schema/config, staging, Apply, and named events |

The Bridge sends `HELLO`, `GET_MANIFEST`, and `GET_SCHEMA`, then reads configuration (chunked when advertised). A session
becomes ready only after identity/contracts are cached and the normalized config validates against the bundled App
schema. Invalid exports remain outside `liveConfig` and degrade the handshake.

## Settings file

Both entry points accept `--config path/to/settings.json`. Explicit flags override file values. The file can contain:

- transport: `serialName`, `oscHost`, `oscPort`, `oscListen`, `oscBind`, `midiLabel`
- guardrails: `allowFeedbackLoops`, `feedbackWindowMs`, `rtP95TargetMs`, `rtJitterP95TargetMs`, `alertSuppressionMs`
- inbound custom mappings: `midiToOscMappings`
- outbound telemetry policy: `midiTelemetryMode`, `outboundMidiMappings`

Start from `bridge/settings.example.json`. Browser-console Performance Setups are browser-local operator data: loading
one creates pending setup changes in the form and never starts routing or changes an MN42 profile.

## OSC workflow

The default send and listen ports are both `9000`; they may be split. Send legacy live control as a JSON string:

```bash
oscsend localhost 9000 /mn42/cmd s '{"cmd":"SET_SLOT_VALUE","slot":2,"value":95}'
```

Typed messages use `/mn42/event/<kind>`, including `cc`, `note_on`, `note_off`, `pitch_bend`,
`channel_aftertouch`, `poly_aftertouch`, `program_change`, `nrpn`, `rpn`, and `sysex`.

Principal outbound addresses are:

- `/mn42/slots` and `/mn42/telemetry/slots` — 42 raw mapped slot values
- `/mn42/slot-outputs` and `/mn42/telemetry/slot-outputs` — 42 resolved values after modulation
- `/mn42/slot-contributions` and `/mn42/telemetry/slot-contributions` — JSON resolver evidence chunks
- `/mn42/envelopes` and `/mn42/telemetry/envelopes` — six envelope values
- `/mn42/event/<kind>` — typed MIDI-style event encoded as JSON in argument 0

The complete structured App-facing API is separate: [Bridge Transport Contract](BridgeTransportContract.md).

## Host MIDI workflow

1. Create or enable a virtual/loopback MIDI port on the host.
2. Start the Bridge with `--midi` set to the exact port label.
3. Enable that MIDI device in the DAW or host application.
4. Confirm telemetry before enabling bidirectional control.

Legacy outbound mapping uses channel 1 CC `0..41` for slots and channel 2 CC `0..5` for envelope followers. Inbound CC
also reaches the firmware live-control lane. A short guard window suppresses freshly mirrored telemetry by default to
reduce feedback loops; change it only with an intentional routing test.

`midiTelemetryMode: "mapped"` is opt-in. Each outbound mapping names `source` (`slots` or `envelopes`), zero-based
`sourceIndex`, MIDI `channel` (`1..16`), and `controller` (`0..127`).

## Custom MIDI-to-OSC mappings

Current custom mappings are additive and intentionally narrow:

- `kind` is `cc`
- `controller` selects the CC number
- optional `channel` narrows the match
- `address` is the user OSC address
- `valueMode` is `raw` (`0..127`) or `normalized` (`0.0..1.0`)

The browser console can passively learn the next CC and preview a route. It excludes reserved `/mn42/*` addresses.
Adding or removing a route updates active routing without restarting serial, MIDI, or OSC transports. Routes loaded from
a Performance Setup remain pending setup changes until the Bridge is deliberately started with those form values.

## CLI reference

Run `node bridge/mn42_bridge.js --help` or `node bridge/mn42_bridge_server.js --help` for the current complete option
set. The commonly used flags are:

| Flag | Meaning |
| --- | --- |
| `--serial <path>` | Device serial path |
| `--osc <port>` | OSC destination/send port |
| `--osc-listen <port>` | OSC command/listen port |
| `--host <address>` | OSC destination host |
| `--bind <address>` | OSC listen address |
| `--midi <label>` | Existing host MIDI port label |
| `--config <path>` | JSON settings file |
| `--allow-feedback-loops` | Disable default reflected-MIDI suppression |
| `--feedback-window-ms <n>` | Retune suppression window |

## Security boundary

The local HTTP console is loopback-only by default. A random token authorizes privileged POST requests and both
WebSocket endpoints. The tokenized URL is a local credential. `--unsafe-network-http` deliberately widens exposure and
requires an operator-controlled network boundary; it is not a production remote-access feature.

## Packaging status

CI can produce unsigned CLI and console/server binaries with checksums, license payloads, manifest metadata, and smoke
checks. They are evidence artifacts, not signed installers. Public beta/production distribution requires the signing
and notarization gates in [Bridge Signing Plan](../release/BridgeSigningPlan.md).

## Verification and evidence

```bash
npm --prefix bridge test
npm --prefix bridge run smoke
```

Host recipes are setup guidance. Only dated [Bridge receipts](../bench/bridge/README.md) and
[host-recipe receipts](../bench/bridge-host-recipes/README.md) establish observed combinations. Live performance writes
and staged configuration writes have different authority; see [Bridge Write Lanes](BridgeWriteLanes.md).
