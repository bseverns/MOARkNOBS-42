# MN42 Bridge

The MN42 Bridge now has a browser-driven local console for everyday use, plus the original CLI for advanced/manual workflows.

Use the browser configurator first if you only need direct USB setup and profile editing. Use the bridge when you need OSC or a virtual MIDI port on a desktop host. Start with [docs/ConnectivityGuide.md](../docs/getting-started/ConnectivityGuide.md) if you are deciding between them.

Current support boundary:

- strongest repo evidence for this tool: Node.js 20 desktop host plus the browser console or CLI
- documented but still setup-specific: OSC-host and DAW routing behavior after the bridge is running
- not claimed here: a signed one-click installer or broad DAW-by-DAW compatibility proof

See [docs/HostCompatibility.md](../docs/reference/HostCompatibility.md) for the conservative matrix.

It does three things:

- reads JSON telemetry from the controller over USB serial,
- publishes that data as OSC and MIDI,
- accepts OSC or MIDI control messages and forwards them back to the controller.

![Bridge CLI showing startup handshake and port bindings](mn42_bridge_cli.svg)

## Quick start (browser console)

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

### 2) Start the local bridge console

From repo root:

```bash
npm --prefix bridge start
```

Or from `bridge/`:

```bash
npm start
```

By default the console is served at <http://127.0.0.1:8787/>.

### 3) Open the bridge console in your browser

Use the browser page to:

- choose or type the serial port,
- set OSC send/listen ports,
- set the virtual MIDI label,
- start or stop the bridge,
- launch the full configurator over the bridge transport.

The configurator opened from this page uses the bridge WebSocket path instead of WebSerial, so profile management, config RPCs, and telemetry still work while OSC and virtual MIDI stay active.

### 4) Confirm it is live

After startup, the bridge sends `HELLO` over serial and waits for:

```json
{ "hello": "mn42" }
```

When the handshake arrives, slot/envelope updates begin forwarding and the browser console reports the device as ready.

## Quick start (CLI)

### 1) Find your serial port

- macOS: `ls /dev/cu.usbmodem*`
- Linux: `ls /dev/ttyACM* /dev/ttyUSB*`
- Windows PowerShell: `Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name`

### 2) Start the bridge

From repo root:

```bash
node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --host 127.0.0.1 --bind 127.0.0.1 --midi "MN42 Bridge"
```

Or from `bridge/`:

```bash
node mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --host 127.0.0.1 --bind 127.0.0.1 --midi "MN42 Bridge"
```

Replace `/dev/ttyACM0` with your device path.

## Everyday workflows

### OSC workflow (TouchOSC, Max, Pd, custom apps)

1. Start the bridge.
2. In your OSC app, listen on your outbound port (`--osc`, default `9000`).
3. Send control commands to `/mn42/cmd` on the inbound port (`--osc-listen`, default `9000`).

Example command (liblo/oscsend):

```bash
oscsend localhost 9000 /mn42/cmd s '{"cmd":"SET_SLOT_VALUE","slot":2,"value":95}'
```

That sets slot 2 to value 95.

### DAW workflow (virtual MIDI)

1. Start the bridge with `--midi "MN42 Bridge"` (default label).
2. In your DAW, enable the MIDI device named `MN42 Bridge`.
3. Record/monitor incoming CC data or send CC data back to the bridge.

This is the documented bridge contract, not a claim that every DAW has already been bench-validated in this repo pass.

MIDI mapping used by the bridge:

- Channel 1 CC: slot updates (`CC 0..41`)
- Channel 2 CC: envelope follower updates (`CC 0..5`)

Inbound MIDI CC on any channel is converted to:

```json
{"cmd":"SET_SLOT_VALUE","slot":<cc_number>,"value":<cc_value>}
```

To prevent MIDI feedback loops by default, the bridge suppresses inbound CC events that match its own freshly emitted telemetry (`status`, `cc`, `value`) within a short guard window (`120 ms` by default). You can disable this with `--allow-feedback-loops` or retune the window with `--feedback-window-ms`.

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
  - optional metadata fields (for bridge-local tracing only):
    - `traceId` (string)
    - `timestamp` / `timestampMs` / `ts` (number or parseable timestamp string)

Valid example:

```json
{ "cmd": "SET_SLOT_VALUE", "slot": 2, "value": 99 }
```

Rejected example:

```json
{ "cmd": "SET_POT", "slot": 2, "value": 99 }
```

## Validation and limits

The bridge drops messages that do not match the contract.

- Max command size: 128 bytes
- `cmd` must be `SET_SLOT_VALUE`
- `slot` must be `0..41`
- `value` must be `0..127`
- Missing keys (`cmd`, `slot`, `value`) are rejected
- Browser/API state also reports drop counters:
  - `serialParseErrors`
  - `serialOversizeDrops`
  - `badOscCmdDrops`
  - `badMidiCmdDrops`
  - `feedbackSuppressed`
- Browser/API state also includes operator alerts:
  - `alerts.active` (currently active warnings/errors)
  - `alerts.recent` (bounded alert history)
- Browser/API state now includes translation trace/timing diagnostics:
  - `timing.lastSerialSourceTimestampMs`
  - `timing.lastSerialHostTimestampMs`
  - `timing.lastSerialSkewMs`
  - `lastRouteAt`
  - `lastRouteTraceId`
  - `routes` (bounded recent translation events, max 200)
- Browser/API state now includes live round-trip metrics:
  - `performance.roundTrip.sampleCount`, `pending`
  - `performance.roundTrip.lastMs`, `p50Ms`, `p95Ms`, `meanMs`
  - `performance.roundTrip.jitterP95Ms`, `jitterMeanMs`
  - `performance.health.status` (`no_data`, `ok`, `warn`) plus threshold/reason details
  - `performance.counters.completed`, `expired`
  - matching counters: `matchedByTrace`, `matchedBySlotValue`

## CLI reference

| Flag                        | Alias | Default        | Purpose                                         |
| --------------------------- | ----- | -------------- | ----------------------------------------------- |
| `--serial`                  | `-s`  | `/dev/ttyACM0` | Serial device path                              |
| `--osc`                     | `-o`  | `9000`         | UDP port for outbound OSC                       |
| `--osc-listen`              | -     | `9000`         | UDP port for inbound OSC commands               |
| `--host`                    | `-H`  | `127.0.0.1`    | Destination host for outbound OSC               |
| `--bind`                    | `-b`  | `127.0.0.1`    | Local interface for inbound OSC listener        |
| `--midi`                    | `-m`  | `MN42 Bridge`  | Virtual MIDI port label                         |
| `--allow-feedback-loops`    | -     | `false`        | Disable MIDI telemetry-echo suppression         |
| `--feedback-window-ms`      | -     | `120`          | Echo-suppression match window in milliseconds   |
| `--rt-p95-target-ms`        | -     | `10`           | Warn threshold for round-trip p95 latency       |
| `--rt-jitter-p95-target-ms` | -     | `5`            | Warn threshold for round-trip jitter p95        |
| `--alert-suppression-ms`    | -     | `3000`         | Alert dedupe/suppression window in milliseconds |

## Browser console reference

The browser console uses the same bridge core and adds:

- local control page: `http://127.0.0.1:8787/`
- bundled configurator: `http://127.0.0.1:8787/app/`
- bridge transport websocket: `ws://127.0.0.1:8787/ws`

The console form also surfaces loop-guard controls (`allowFeedbackLoops`, `feedbackWindowMs`), alert cooldown tuning (`alertSuppressionMs`), and runtime counter diagnostics.
It now also surfaces recent route/timing fields (`Last route`, `Last trace`, `Source timestamp`, and `Clock skew`) so drift and trace continuity are visible during runs.
It now surfaces live round-trip diagnostics (`RT samples`, `RT pending`, `RT last`, `RT p50`, `RT p95`, `RT jitter p95`, `RT health`) derived from command→telemetry acknowledgments.
It also surfaces active alert count and top alert message, plus a one-click clear action for operator triage.

Operator API add-on:

- `POST /api/performance/reset` clears rolling round-trip samples/counters without disconnecting transports.
- `POST /api/alerts/clear` clears currently active alerts without touching metrics history.
- `GET /api/state/snapshot` downloads a timestamped JSON snapshot of runtime metadata + current bridge state.

The console accepts the same serial/OSC/MIDI settings as the CLI, plus:

| Flag          | Default     | Purpose                                          |
| ------------- | ----------- | ------------------------------------------------ |
| `--http-host` | `127.0.0.1` | Host interface for the browser console           |
| `--http-port` | `8787`      | Port for the browser console and `/ws` transport |

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
- Ensure JSON has `cmd`, `slot`, `value` with `cmd` set to `SET_SLOT_VALUE`.
- Confirm `slot` and `value` are in range.

### DAW cannot find `MN42 Bridge`

- Restart DAW after bridge launch.
- Check that bridge has no startup MIDI errors.
- Try a different `--midi` label and re-scan MIDI devices.

### Port in use errors

- Change ports with `--osc` / `--osc-listen`.
- Keep `--bind 127.0.0.1` if you only need local access.

## Packaging status (for non-command-line users)

Current state: the bridge has a browser-based local control surface, but it is still launched from Node.js and is not yet shipped as a signed one-click installer.

If demand grows, this is the practical path:

1. Prototype binary packaging with `pkg` or `nexe` per platform.
2. Include prebuilt native dependencies for `serialport`.
3. Ship signed installers that launch the bridge with a simple UI wrapper.
4. Keep advanced flags available for power users.

Until then, this README is the canonical runbook for daily use.

For rollout planning, see [`docs/BridgePackaging.md`](../docs/release/BridgePackaging.md).
For a show-day quick reference, see [`docs/BridgeForPerformers.md`](../docs/guides/BridgeForPerformers.md).

## Development

From `bridge/`:

```bash
npm run ci
npm run release:prep
npm run lint
npm run format
# optional packaging step
npm run package:bridge
```

`npm run ci` runs the split bridge coverage lanes plus both entrypoint smoke checks. `release:prep` uses the same path before packaging.

## License

MIT, same as the repository root: [../LICENSE](../LICENSE).
