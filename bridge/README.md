# MN42 Bridge

Welcome to the scrappy little Node.js sidecar that lets your **MOARkNOBS-42** talk trash on modern networks.

## System context

Think of this as the surly middle manager between the MN42 controller and whatever OSC-aware DAW you're torturing. It listens on `/dev/ttyACM0` at 31,250 baud, shovels controller dumps out to `/mn42/slots` and `/mn42/envelopes`, and waits on `/mn42/cmd` for anything you want shot back over serial. Outbound OSC heads to whatever port you feed `--osc`; inbound commands always land on UDP 9000.

```bash
oscsend localhost 9000 /mn42/cmd '{"cmd":"SET_POT","slot":2,"value":99}'  # OSC -> serial sets pot 2 to 99
```

## What's the gig?

- **Serial** in at 31,250 baud.
- **OSC** blasts out on `/mn42/slots` and `/mn42/envelopes` (aimed at the `--osc` port, default 9000).
- **Virtual MIDI** mirror so WebMIDI punks can jam along.
- Shoot back commands like `SET_POT` over OSC or MIDI and they hitch a ride over serial.

## CLI

```bash
node mn42_bridge.js \
  --serial /dev/ttyACM0 \
  --osc 9000 \
  --bind 127.0.0.1 \
  --midi "MN42 Bridge"
```

Flags:

- `--serial` (`-s`) – which serial port to sniff.
- `--osc` (`-o`) – remote UDP port to scream OSC at. The bridge still listens for `/mn42/cmd` on 9000.
- `--bind` (`-b`) – IP to park the UDP server on. Defaults to `127.0.0.1` so randos can't wiggle your knobs.
- `--midi` (`-m`) – label for the virtual MIDI port.

Safety bumpers:

- The bridge ignores OSC/MIDI JSON that doesn't spell out `cmd`, `slot`, and a numeric `value`. No half-baked packets make it to serial.

## Teaching moment

The bridge waits for `{"hello":"mn42"}` from the controller before it starts spewing data. Each JSON line from the serial port is parsed and shotgunned to both OSC and MIDI. Incoming OSC or MIDI messages get repackaged as JSON and fired back at the controller.

## Install

This miscreant only rolls with **Node.js 20**. Anything older is a poser and won't even get past the door. Check your version:

```bash
node --version
# v20.x.x or bust
```

Once you're speaking fluent v20, wire up the deps:

```bash
npm install
```

Feeling deterministic? Swap in the lockstep version:

```bash
npm ci
```

## Testing vibes

This repo doesn't ship with a hardware mock. To prove the script at least boots and dies gracefully when the wire's pulled:

```bash
npm test
```

The test pokes a fake serial port so you can watch the bridge complain and keep its cool. If you've got the real controller, open an OSC monitor and a WebMIDI client, twiddle a pot, and watch the packets fly.

## Example Session

Need proof this gremlin works? Try this slam-dunk walkthrough.

1. Kick the bridge to life:

   ```bash
   node mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --midi "MN42 Bridge"
   ```

2. In another terminal, eavesdrop on the OSC noise:

   ```bash
   oscdump 9000
   ```

3. Sniff the MIDI echo too:

   ```bash
   aseqdump -p "MN42 Bridge"
   ```

4. Now hurl a `SET_POT` command at slot 2:

   ```bash
   oscsend localhost 9000 /mn42/cmd s '{"cmd":"SET_POT","slot":2,"value":95}'
   ```

   The hardware's slot 2 should snap to 95. `oscdump` spits back a `/mn42/slots` update and `aseqdump` coughs up a matching Control Change. That's the round trip—OSC in, MIDI out, and the rig obeys.

## License

This scrappy sidecar rides under the [MIT License](../LICENSE). Peep the root file for the full legal riff.

