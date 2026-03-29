# Quickstart for Performers

This is the shortest path from plugging in the board to using it live.

## 1. Connect the board

1. Plug MOARkNOBS-42 into your computer over USB.
2. Wait for the board to enumerate.
3. If the board is new or newly flashed, confirm it responds to `HELLO` before show-day use.

## 2. Start with the browser configurator

Use the browser configurator when you want direct USB setup, profile management, and live monitoring without extra routing software.

Start here:

- [Configurator Tour](Configurator.md) for the current configurator workflow
- [Operator Tutorial](OperatorTutorial.md) for the practical operating model, including `IM` vs `PK`
- [Connectivity Guide](ConnectivityGuide.md) if you are not sure whether you need the bridge

## 3. Connect in the configurator

1. Serve the configurator from `http://localhost` or use the published project URL you normally deploy from.
2. Open the configurator.
3. Click **Connect**.
4. Pick the MOARkNOBS-42 serial device.
5. Wait for the connection banner to show the device identity and firmware details.

## 4. Save and load profiles

- Use profile slots `A` through `D` for on-device saved states.
- Use **Load profile** to recall a stored device state.
- Use **Save profile** after you have applied and confirmed the setup you want to keep.
- Use export/download if you want an external JSON backup as well.

More detail: [Profile Workflow](ProfileWorkflow.md).

## 5. Use the bridge only when you need it

Use the Node bridge when you need one of these:

- OSC into Max, Pd, TouchOSC, or another OSC host
- A virtual MIDI port for a DAW
- A fallback path when the browser/workstation setup is not the right tool

Bridge details: [OSC Bridge](OSCBridge.md) and [Performer Bridge Guide](BridgeForPerformers.md).

## 6. If connection fails

- Re-seat the USB cable and confirm it is a data cable.
- Reopen the configurator and reconnect.
- Serve the configurator from `http://localhost` or another allowed secure context.
- If WebSerial is not working in your browser/workflow, switch to the bridge path in [Connectivity Guide](ConnectivityGuide.md).
- If the board itself is not responding, check [Troubleshooting](Troubleshooting.md).

## 7. Before a rehearsal or public demo

- Run [Demo Test Punch List](DemoTestPunchList.md).
- Use [Validation Flow](ValidationFlow.md) if you need to decide whether the setup is merely connected, actually demo-ready, or still in bench-only status.
