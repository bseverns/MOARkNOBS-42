# MN42 Bridge

The MN42 Bridge is a Node.js CLI that connects MOARkNOBS-42 hardware to OSC apps and virtual MIDI ports.

Use the browser configurator first if you only need direct USB setup and profile editing. Use the bridge when you need OSC or a DAW-facing virtual MIDI port. Start with [docs/ConnectivityGuide.md](../docs/ConnectivityGuide.md) if you are deciding between them.

It does three things:

- reads JSON telemetry from the controller over USB serial,
- publishes that data as OSC and MIDI,
- accepts OSC or MIDI control messages and forwards them back to the controller.

![Bridge CLI showing startup handshake and port bindings](mn42_bridge_cli.svg)

## Quick start (musician-friendly)

### 1) Install prerequisites

- Node.js 20.x (`node --version` must report `v20.*`)
- MN42 connected over USB

From repo root:

```bash
npm --prefix bridge ci
```

Or from `bridge/`:

```bash
npm ci
```

### 2) Find your serial port

- macOS: `ls /dev/cu.usbmodem*`
- Linux: `ls /dev/ttyACM* /dev/ttyUSB*`
- Windows PowerShell: `Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name`

### 3) Start the bridge

From repo root:

```bash
node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --host 127.0.0.1 --bind 127.0.0.1 --midi "MN42 Bridge"
```

Or from `bridge/`:

```bash
node mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --host 127.0.0.1 --bind 127.0.0.1 --midi "MN42 Bridge"
```

Replace `/dev/ttyACM0` with your device path.

### 4) Confirm it is live

After startup, the bridge sends `HELLO` over serial and waits for:

```json
{ "hello": "mn42" }
```

When the handshake arrives, slot/envelope updates begin forwarding.

## Everyday workflows

### OSC workflow (TouchOSC, Max, Pd, custom apps)

1. Start the bridge.
2. In your OSC app, listen on your outbound port (`--osc`, default `9000`).
3. Send control commands to `/mn42/cmd` on the inbound port (`--osc-listen`, default `9000`).

Example command (liblo/oscsend):

```bash
oscsend localhost 9000 /mn42/cmd s '{"cmd":"SET_POT","slot":2,"value":95}'
```

That sets slot 2 to value 95.

### DAW workflow (virtual MIDI)

1. Start the bridge with `--midi "MN42 Bridge"` (default label).
2. In your DAW, enable the MIDI device named `MN42 Bridge`.
3. Record/monitor incoming CC data or send CC data back to the bridge.

MIDI mapping used by the bridge:

- Channel 1 CC: slot updates (`CC 0..41`)
- Channel 2 CC: envelope follower updates (`CC 0..5`)

Inbound MIDI CC on any channel is converted to:

```json
{"cmd":"SET_POT","slot":<cc_number>,"value":<cc_value>}
```

## OSC addresses and payloads

### Outbound (bridge -> OSC)

- `/mn42/slots`
  - OSC args: 42 integers, each `0..127`
  - index 0 maps to slot 0, index 41 maps to slot 41
- `/mn42/envelopes`
  - OSC args: 6 integers, each `0..127`

### Inbound (OSC -> bridge)

- `/mn42/cmd`
  - first argument must be JSON text or object with:
    - `cmd` (string)
    - `slot` (integer `0..41`)
    - `value` (integer `0..127`)

Valid example:

```json
{ "cmd": "SET_POT", "slot": 2, "value": 99 }
```

Rejected example:

```json
{ "cmd": "SET_POT", "slot": 99, "value": -1 }
```

## Validation and limits

The bridge drops messages that do not match the contract.

- Max command size: 128 bytes
- `slot` must be `0..41`
- `value` must be `0..127`
- Missing keys (`cmd`, `slot`, `value`) are rejected

## CLI reference

| Flag           | Alias | Default        | Purpose                                  |
| -------------- | ----- | -------------- | ---------------------------------------- |
| `--serial`     | `-s`  | `/dev/ttyACM0` | Serial device path                       |
| `--osc`        | `-o`  | `9000`         | UDP port for outbound OSC                |
| `--osc-listen` | -     | `9000`         | UDP port for inbound OSC commands        |
| `--host`       | `-H`  | `127.0.0.1`    | Destination host for outbound OSC        |
| `--bind`       | `-b`  | `127.0.0.1`    | Local interface for inbound OSC listener |
| `--midi`       | `-m`  | `MN42 Bridge`  | Virtual MIDI port label                  |

### Split send/receive ports

```bash
node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 7000 --osc-listen 8000
```

- listen for telemetry on port `7000`
- send `/mn42/cmd` commands to port `8000`

## Troubleshooting

### Bridge starts but no updates

- Confirm serial path is correct.
- Confirm controller responds to `HELLO`.
- Ensure firmware serial baud matches `115200`.

### OSC commands are ignored

- Confirm command is sent to `/mn42/cmd`.
- Ensure JSON has `cmd`, `slot`, `value`.
- Confirm `slot` and `value` are in range.

### DAW cannot find `MN42 Bridge`

- Restart DAW after bridge launch.
- Check that bridge has no startup MIDI errors.
- Try a different `--midi` label and re-scan MIDI devices.

### Port in use errors

- Change ports with `--osc` / `--osc-listen`.
- Keep `--bind 127.0.0.1` if you only need local access.

## Packaging status (for non-command-line users)

Current state: the bridge is a Node CLI and is not yet shipped as a one-click installer.

If demand grows, this is the practical path:

1. Prototype binary packaging with `pkg` or `nexe` per platform.
2. Include prebuilt native dependencies for `serialport`.
3. Ship signed installers that launch the bridge with a simple UI wrapper.
4. Keep advanced flags available for power users.

Until then, this README is the canonical runbook for daily use.

For rollout planning, see [`docs/BridgePackaging.md`](../docs/BridgePackaging.md).
For a show-day quick reference, see [`docs/BridgeForPerformers.md`](../docs/BridgeForPerformers.md).

## Development

From `bridge/`:

```bash
npm run release:prep
npm run lint
npm run format
# optional packaging step
npm run package:bridge
```

`release:prep` runs the bridge test suite and CLI smoke checks before packaging.

## License

MIT, same as the repository root: [../LICENSE](../LICENSE).
