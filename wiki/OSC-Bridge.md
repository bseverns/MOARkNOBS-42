# OSC Bridge

The bridge (`bridge/`) is a Node.js CLI that connects firmware serial telemetry to OSC and virtual MIDI.
Canonical source: `bridge/README.md`

Use this path when the finished instrument needs a DAW-facing virtual MIDI
route or OSC. For browser-only configuration, start with
[Getting Started](Getting-Started.md) instead.

![Routing overview showing MN42 hardware feeding the Node bridge, then branching to OSC, virtual MIDI, and the browser app over the bridge path.](assets/workflows/bridge-routing-overview.png)

## Install

```bash
npm --prefix bridge ci
```

## Run

```bash
node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --host 127.0.0.1 --bind 127.0.0.1 --midi "MN42 Bridge"
```

![Simulated bridge console showing start and stop controls, a connected serial device, OSC input and output endpoints, virtual MIDI, and runtime health.](assets/ui/bridge-console-sim.png)

## Behavior summary

- Reads JSON telemetry from serial.
- Publishes slot/envelope updates to OSC.
- Exposes virtual MIDI device for host DAW workflows.
- Accepts inbound OSC/MIDI commands and forwards validated control commands to firmware.

## DAW checkpoint

1. Start the Bridge and wait for Bridge, Serial, and Device readiness.
2. Enable the Bridge/IAC/loopback MIDI input named by your host recipe.
3. Arm a receiving track and match its MIDI channel.
4. Move one known CC-mapped control and confirm it in a MIDI monitor.
5. MIDI-learn that CC to an audible destination.

Use the exact first mapping in [First Playable Walkthrough](Playable-Walkthrough.md).
If telemetry moves but the DAW does not, follow
[MIDI is not reaching the DAW](Troubleshooting.md#midi-is-not-reaching-the-daw).

## OSC contract

- Outbound:
  - `/mn42/slots` (42 values)
  - `/mn42/envelopes` (6 values)
- Inbound:
  - `/mn42/cmd` with `{ "cmd":"SET_POT", "slot":n, "value":v }`

## Tests

```bash
npm --prefix bridge test
```

## Reference docs

- `bridge/README.md`
- `docs/guides/OSCBridge.md`
- `docs/guides/BridgeForPerformers.md`
- `docs/release/BridgePackaging.md`
