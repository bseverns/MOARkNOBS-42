# MN42 Bridge

Welcome to the scrappy little Node.js sidecar that lets your **MOARkNOBS-42** talk trash on modern networks.

## System context

Think of this as the surly middle manager between the MN42 controller and whatever OSC-aware DAW you're torturing. It listens on `/dev/ttyACM0` at 31,250 baud, shovels controller dumps out to `/mn42/slots` and `/mn42/envelopes`, and waits on `/mn42/cmd` for anything you want shot back over serial.

```bash
oscsend localhost 9000 /mn42/cmd '{"cmd":"SET_POT","slot":2,"value":99}'  # OSC -> serial sets pot 2 to 99
```

## What's the gig?

- **Serial** in at 31,250 baud.
- **OSC** blasts out on `/mn42/slots` and `/mn42/envelopes` (UDP 9000 by default).
- **Virtual MIDI** mirror so WebMIDI punks can jam along.
- Shoot back commands like `SET_POT` over OSC or MIDI and they hitch a ride over serial.

## CLI

```bash
node mn42_bridge.js \
  --serial /dev/ttyACM0 \
  --osc 9000 \
  --midi "MN42 Bridge"
```

Flags:

- `--serial` (`-s`) – which serial port to sniff.
- `--osc` (`-o`) – UDP port to scream OSC from (and listen for commands).
- `--midi` (`-m`) – label for the virtual MIDI port.

## Teaching moment

The bridge waits for `{"hello":"mn42"}` from the controller before it starts spewing data. Each JSON line from the serial port is parsed and shotgunned to both OSC and MIDI. Incoming OSC or MIDI messages get repackaged as JSON and fired back at the controller.

## Testing vibes

This repo doesn't ship with a hardware mock. To prove the script at least boots:

```bash
npm test
```

If you've got the real controller, open an OSC monitor and a WebMIDI client, twiddle a pot, and watch the packets fly.

