# MN42 Bridge

The MN42 Bridge is the desktop companion for the browser configurator. Use it when a rig needs OSC, host/virtual MIDI,
a local browser console, or App access without browser WebSerial. For direct USB configuration only, start with the
[App](../App/README.md).

## Support boundary

- Best repo evidence: Node.js 24 desktop host with the browser console or CLI.
- OSC and DAW routing are setup-specific; consult receipts before making host-wide claims.
- CI artifacts are unsigned. This repository does not claim a signed installer or broad DAW certification.
- Package scripts intentionally require Node `>=24 <25` until wider versions have explicit evidence.

See [Host Compatibility](../docs/reference/HostCompatibility.md) for the conservative matrix.

## Quick start: browser console

Prerequisites: Node.js 24.x and an MN42 connected over USB.

```bash
npm --prefix bridge ci
npm --prefix bridge start
```

Open <http://127.0.0.1:8787/> if the server does not open it automatically. Then:

1. Choose the serial port and confirm the OSC/MIDI destinations.
2. Start the Bridge and wait for the device session to report ready.
3. Move one hardware control and confirm a Device/OSC/MIDI route heartbeat.
4. Select **Open configurator** when you need profile or mapping edits.

The HTTP server binds to loopback by default. Every launch creates a random control token and opens a tokenized URL;
treat it as a local credential. Public-interface HTTP requires the explicit `--unsafe-network-http` option and your own
network protection.

## Quick start: CLI

Find the serial device (`/dev/cu.usbmodem*` on macOS, `/dev/ttyACM*` or `/dev/ttyUSB*` on Linux, or a COM port on
Windows), then run:

```bash
node bridge/mn42_bridge.js \
  --serial /dev/ttyACM0 \
  --osc 9000 \
  --osc-listen 9000 \
  --host 127.0.0.1 \
  --bind 127.0.0.1 \
  --midi "MN42 Bridge"
```

Replace the serial path and MIDI port label with values from the host. Use `--config ./bridge/settings.example.json`
for saved settings; explicit CLI flags override the file.

## What the Bridge routes

- Device telemetry to OSC and optional host MIDI.
- OSC or MIDI performance messages back to the firmware live-control lane.
- Structured device-session state to the browser configurator.
- Revisioned staged configuration through `/api/device/*` and `/ws/events`.
- Raw newline-oriented serial compatibility through `/ws`.

Live `SET_SLOT_VALUE` and typed MIDI/OSC events are performance writes, not staged configuration. Configuration changes
use stage/apply endpoints and preserve uncertainty until authoritative readback resolves device truth.

## Everyday workflows

- **Rehearsal/show:** follow [Bridge for Performers](../docs/guides/BridgeForPerformers.md).
- **Browser console:** use [Bridge Console Tour](../docs/bridge/BridgeConsoleTour.md).
- **OSC or DAW setup:** use [Bridge Operator Reference](../docs/bridge/BridgeOperatorReference.md).
- **App integration:** use [Bridge Transport Contract](../docs/bridge/BridgeTransportContract.md).
- **Write behavior:** use [Bridge Write Lanes](../docs/bridge/BridgeWriteLanes.md).

## Troubleshooting

- **No device updates:** confirm the serial path, close other serial clients, then restart and wait for `HELLO`.
- **OSC commands ignored:** send to the listen port (default `9000`) and use `/mn42/cmd` or a documented typed address.
- **DAW cannot see MIDI:** create/enable a host loopback port and pass its exact label to `--midi`.
- **Port already in use:** stop the existing process or choose different HTTP/OSC ports.
- **Apply is uncertain:** do not repeat the write blindly; wait for the session's authoritative readback.

## Develop and test

```bash
npm --prefix bridge test
npm --prefix bridge run smoke
```

Packaging and signing are separate release concerns. See [Bridge Signing Plan](../docs/release/BridgeSigningPlan.md) and
the [artifact checklist](../docs/release/bridge-artifacts-checklist.md).

## Reference

- [Bridge Docs Map](../docs/bridge/BridgeDocsMap.md)
- [Bridge Operator Reference](../docs/bridge/BridgeOperatorReference.md)
- [Bridge Transport Contract](../docs/bridge/BridgeTransportContract.md)
- [Bridge Write Lanes](../docs/bridge/BridgeWriteLanes.md)
- [Known Good Host Recipes](../docs/reference/KnownGoodHostRecipes.md)
- [Bridge bench receipts](../docs/bench/bridge/README.md)
- [Documentation Truth Map](../docs/reference/DocumentationTruthMap.md)

Bridge code is licensed under the repository license; bundled dependency notices are in
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
