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
{
  "slots":[0,1,2,...,41],
  "envelopes":[0,0,0,0,0,0],
  "currentSlot":5,
  "argMethod":"PLUS",
  "efStatus":[1,0,0,0,0,1]
}
```

- `slots` – 42 MIDI‑scaled values (0‑127) for each virtual slot.
- `envelopes` – live levels from the six envelope followers, also 0‑127.
- `currentSlot` – which slot is currently screaming.
- `argMethod` – firmware's current ARG calculation mode.
- `efStatus` – array of six flags; `1` means that envelope follower is lit.

Parse each line as JSON and redraw your UI. There’s no framing besides the newline, because who needs more ceremony?

## Text Commands

Spying is fun, but sometimes you gotta bark orders. Hurl plain‑text commands
terminated with a newline and the firmware snaps back with either `OK` or `ERR`.

### Paint the LEDs

```
SET_LED <brightness>,<r>,<g>,<b>
GET_LED
```

`SET_LED` dials the strip’s brightness and RGB vibe (all 0‑255). `GET_LED`
returns the current `brightness,r,g,b` quartet.

### Pick an ARG Method

```
SET_ARGMETHOD <n>
GET_ARGMETHOD
```

ARG mode can mash signals in seven different ways. Toss an index `0‑6` at
`SET_ARGMETHOD` to lock one in; `GET_ARGMETHOD` spits the stored index back.

### Rewire an Envelope Follower

```
SET_EF <slot>,<ef>
GET_EF <slot>
```

Patch an envelope follower to a slot with `SET_EF`. Ask who’s riding shotgun
with `GET_EF`, which replies with the follower number or `-1` if nobody showed.

## Why So Barebones?

Less baggage means faster feedback. This protocol exists so you can patch in a browser, twist a pot, and immediately see the numbers jump. Fork it, abuse it, or teach it to do bigger tricks.

## See It in Action

Want a live demo? Fire up the [WebSerial configuration app](../firmware/App/README.md) and watch the slots and envelopes shimmy while you tweak settings. For philosophy and troubleshooting, peep [README_webserial.md](../firmware/App/README_webserial.md).
