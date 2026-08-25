# Known Good Host Recipes

These are setup recipes for bench validation. They are not a promise that every host version is fully certified. The browser bridge console now exposes matching recipe presets under `bridge/presets/`.

Recipe receipts live in [../bench/bridge-host-recipes/README.md](../bench/bridge-host-recipes/README.md). Treat a recipe as evidence-backed only when a matching receipt exists.

## Preset-backed recipes

### `macos-iac-ableton-basic`

Receipt status: no matching receipt committed yet

Use when:

- macOS host
- IAC Driver is available
- Ableton Live is the MIDI consumer

Checklist:

1. Open Audio MIDI Setup and enable the IAC Driver.
2. Create or rename a bus to `MN42 Bridge`.
3. In the bridge console Setup mode, pick `macOS IAC + Ableton Basic`.
4. Confirm the MIDI label matches the actual IAC bus name.
5. In Ableton Live, enable that bus for Track and Remote input.
6. Validate one inbound knob move and one return CC without disabling feedback suppression unless you are intentionally testing loopback.

### `max-osc-localhost`

Receipt status: no matching receipt committed yet

Use when:

- Max/MSP is running on the same host as the bridge
- you want localhost OSC without a network hop

Checklist:

1. Pick `Max OSC Localhost` in the bridge console.
2. Receive `/mn42/slots`, `/mn42/envelopes`, or `/mn42/telemetry/slots` in Max.
3. If you send control back, keep it on `/mn42/cmd` or `/mn42/event/*`.
4. Verify the Bridge **Diagnostics** route traces match what the Max patch sees.

### `touchosc-performance-local`

Receipt status: no matching receipt committed yet

Use when:

- the bridge host and tablet share a trusted local network
- the goal is performance monitoring first, control second

Checklist:

1. Pick `TouchOSC Performance Local`.
2. Confirm the OSC bind address is intentional for your network boundary.
3. Start with receive-only layouts and verify telemetry stability.
4. Only add control widgets after bench-confirming the route and alert posture.

### `windows-loopmidi-reaper-basic`

Receipt status: no matching receipt committed yet

Use when:

- Windows host
- loopMIDI or equivalent virtual MIDI loopback is installed
- Reaper is the MIDI consumer

Checklist:

1. Create a loopMIDI port named `MN42 Bridge` or update the preset-applied MIDI label to the exact port name.
2. Start the bridge and verify the browser console reaches device-ready state.
3. Enable the same loopback port in Reaper.
4. Validate one recorded CC lane from live hardware movement and one inbound CC from Reaper.

## Observation-only receipts

- [2026-05-31 macOS IAC + REAPER basic observed](../bench/bridge-host-recipes/macos-iac-reaper-basic-observed.md)
  - This is not a verified recipe receipt. It records that REAPER was present on the host while the Bridge reached `ready` on `IAC 1 Bus 1`, but it does not prove DAW-side CC capture or return traffic yet.

## Manual recipes that remain documented

### Logic on macOS

1. Enable the IAC bus in Audio MIDI Setup.
2. Start the bridge with `--midi "MN42 Bridge"` or the browser console equivalent.
3. In Logic, open MIDI Environment or Controller Assignments.
4. Learn incoming CC messages from the IAC bus.
5. Keep feedback loop suppression enabled unless you are intentionally testing bidirectional mappings.

### Pure Data OSC

1. Start the bridge with OSC output enabled.
2. Use `netreceive -u -b 9000` or the existing Pure Data example under `docs/examples/puredata/`.
3. Decode `/mn42/slots` and `/mn42/envelopes`.
4. Keep command traffic local while testing.
