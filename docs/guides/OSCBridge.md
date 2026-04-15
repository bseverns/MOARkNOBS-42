# OSC and Virtual MIDI Bridge Quickstart

This page is the friendly quickstart for using MOARkNOBS-42 with OSC apps and DAWs.

For the full CLI reference and message contract, see [bridge/README.md](https://github.com/bseverns/benzknober/blob/main/bridge/README.md).

## What you can do with the bridge

- read MN42 slot/envelope updates in OSC
- route those updates into a DAW over virtual MIDI
- send control commands back to the hardware from OSC or MIDI

## Start in 60 seconds

From repo root:

```bash
npm --prefix bridge ci
node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --midi "MN42 Bridge"
```

Then:
- in an OSC monitor, listen on UDP `9000` for `/mn42/slots` and `/mn42/envelopes`
- in your DAW, enable the MIDI input named `MN42 Bridge`

## OSC command example

Send one command to set slot 2 to 95:

```bash
oscsend localhost 9000 /mn42/cmd s '{"cmd":"SET_SLOT_VALUE","slot":2,"value":95}'
```

## Virtual MIDI example

- Start bridge with default MIDI label (`MN42 Bridge`)
- In your DAW MIDI devices, enable `MN42 Bridge`
- You should see incoming CC data while turning knobs

Bridge MIDI mapping:
- Channel 1 CC 0..41 = slot values
- Channel 2 CC 0..5 = envelope follower values

## Common mistakes

- Wrong serial device path
- Sending OSC to the wrong port (`--osc-listen`)
- Sending invalid JSON or out-of-range slot/value

## If you want one-click install later

Packaging is not shipped yet. The current bridge is CLI-first.
If needed, the next step is a per-platform bundle using `pkg` or `nexe` plus signed installers.


Performer-friendly one-pager: [`docs/BridgeForPerformers.md`](BridgeForPerformers.md).
