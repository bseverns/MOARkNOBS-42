# WebSerial Groove

The Teensy screams JSON snapshots over WebSerial so the browser can watch the synth wiggle in real time. It's dead‑simple and a little rowdy.

## Handshake

1. Browser opens the serial port at **31250** baud.
2. Browser sends `HELLO\n`.
3. Teensy answers with:
   ```json
   {"hello":"mn42"}
   ```
4. Streaming begins. Bail out by closing the port.

## State Messages

Every ~100 ms the firmware spits a newline‑terminated JSON blob:

```json
{"slots":[0,1,2,...,41],"envelopes":[0,0,0,0,0,0]}
```

- `slots` – 42 MIDI‑scaled values (0‑127) for each virtual slot.
- `envelopes` – live levels from the six envelope followers, also 0‑127.

Parse each line as JSON and redraw your UI. There’s no framing besides the newline, because who needs more ceremony?

## Why So Barebones?

Less baggage means faster feedback. This protocol exists so you can patch in a browser, twist a pot, and immediately see the numbers jump. Fork it, abuse it, or teach it to do bigger tricks.

## See It in Action

Want a live demo? Fire up the [WebSerial configuration app](../firmware/App/README.md) and watch the slots and envelopes shimmy while you tweak settings.
