# Known Good Host Recipes

These are setup recipes for bench validation. They are not a promise that every host version is fully certified.

## macOS IAC + Ableton

1. Open Audio MIDI Setup.
2. Choose Window > Show MIDI Studio.
3. Open IAC Driver and enable Device is online.
4. Add or rename a bus to `MN42 Bridge`.
5. Start the bridge with `--midi "MN42 Bridge"`.
6. In Ableton Live, enable the IAC bus for Track and Remote input.
7. Map or monitor incoming CC traffic from the bridge.

## macOS IAC + Logic

1. Enable the IAC bus in Audio MIDI Setup.
2. Start the bridge with `--midi "MN42 Bridge"`.
3. In Logic, open MIDI Environment or Controller Assignments.
4. Learn incoming CC messages from the IAC bus.
5. Keep feedback loop suppression enabled unless testing bidirectional mappings intentionally.

## Max/MSP OSC

1. Start the bridge with OSC output to localhost:

   ```bash
   node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --host 127.0.0.1
   ```

2. In Max, receive `/mn42/slots`, `/mn42/envelopes`, or `/mn42/telemetry/slots` on UDP port `9000`.
3. Send commands to `/mn42/cmd` only from trusted local patches.

## Pure Data OSC

1. Start the bridge with OSC output enabled.
2. Use `netreceive -u -b 9000` or the existing Pure Data example under `docs/examples/puredata/`.
3. Decode the OSC paths `/mn42/slots` and `/mn42/envelopes`.
4. Keep command traffic local while testing.

## TouchOSC

1. Put TouchOSC on the same trusted local network as the bridge host.
2. Point TouchOSC input at the bridge OSC host and port.
3. Map controls to `/mn42/cmd` only if the bridge bind address and network are intentionally secured.
4. Prefer receive-only layouts for first validation passes.
