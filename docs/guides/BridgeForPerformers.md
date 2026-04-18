# Bridge for Performers (One-Page Cheatsheet)

Use this when you just want the rig working before rehearsal or a set.

If you want the broader rehearsal-first workflow before the bridge-specific details, start with [Musician-First Guide](../getting-started/MusicianFirstGuide.md).

![Routing overview showing MN42 hardware feeding the Node bridge, then branching to OSC, virtual MIDI, and the browser app over the bridge path.](../assets/workflows/bridge-routing-overview.png)

## 1) Start the bridge

From repo root:

```bash
node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --host 127.0.0.1 --bind 127.0.0.1 --midi "MN42 Bridge"
```

If your serial path differs, replace `/dev/ttyACM0`.

## 2) Pick one workflow

### OSC workflow (TouchOSC, Max, Pd, custom)

- Listen on UDP port `9000` for:
  - `/mn42/slots` (42 args)
  - `/mn42/envelopes` (6 args)
  - `/mn42/telemetry/slots` and `/mn42/telemetry/envelopes` (same payloads, namespaced)
- Send commands to `/mn42/cmd` (legacy slot lane) or `/mn42/event/*` (typed lane) on UDP `9000`.

Example command:

```bash
oscsend localhost 9000 /mn42/cmd s '{"cmd":"SET_SLOT_VALUE","slot":2,"value":95}'
```

Typed event example:

```bash
oscsend localhost 9000 /mn42/event/note_on s '{"channel":1,"note":60,"velocity":110}'
```

### DAW workflow (virtual MIDI)

- In your DAW MIDI device list, enable `MN42 Bridge`.
- Twist a knob on MN42 and confirm incoming MIDI CC.
- Record/midi-map as normal.

Bridge MIDI mapping:

- Ch 1 CC `0..41`: slots
- Ch 2 CC `0..5`: envelopes

## 3) Quick checks before show time

- Confirm bridge prints no serial errors.
- Confirm handshake appears once: `{"hello":"mn42"}`.
- Confirm OSC monitor or DAW meter moves when knobs move.
- Save your DAW project template with `MN42 Bridge` enabled.

## 4) Panic fixes (fast)

- **No data at all**: wrong serial path or cable; replug and restart bridge.
- **DAW sees nothing**: relaunch DAW after bridge starts.
- **OSC not responding**: wrong port or wrong address; use `/mn42/cmd` or `/mn42/event/*` on port `9000`.
- **Command ignored**: wrong command shape or slot/value out of range; use `SET_SLOT_VALUE`, slot `0..41`, value `0..127`.
- **Port already in use**: move ports, for example `--osc 7000 --osc-listen 8000`.

## 5) Keep nearby

- Full bridge docs: [bridge/README.md](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md)
- OSC quickstart: [`docs/OSCBridge.md`](OSCBridge.md)
- Packaging roadmap: [`docs/BridgePackaging.md`](../release/BridgePackaging.md)
- Profile/preset behavior: [`docs/ProfileWorkflow.md`](ProfileWorkflow.md)
- Fast recovery cases: [`docs/FailureFirst.md`](../validation/FailureFirst.md)
