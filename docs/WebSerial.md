# WebSerial Groove

The Teensy screams JSON snapshots over WebSerial so the browser can watch the synth wiggle in real time. It's dead‑simple and a little rowdy.

## Handshake

1. Browser opens the serial port at **31250** baud.
2. Browser sends `HELLO\n`.
3. Teensy answers with:
   ```json
   { "hello": "mn42" }
   ```
   ![Browser console showing HELLO handshake with JSON response](webserial_handshake.svg)
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

### Command Roster

This table mirrors the shout list in `firmware_main.cpp` so the code and docs
never fall out of sync.

| Command                            | Arguments         | What it pokes                                                                       |
| ---------------------------------- | ----------------- | ----------------------------------------------------------------------------------- |
| `HELLO`                            | –                 | start WebSerial streaming                                                           |
| `GET_SCHEMA`                       | –                 | dump config schema                                                                  |
|                                    |                   | If the device ghosts you, the HTML app drags out its baked-in `config_schema.json`. |
| `GET_BROWNOUTS`                    | –                 | number of brownouts seen                                                            |
| `SET_POT` `<slot>,<chan>,<cc>`     | ints              | bind slot to channel+CC                                                             |
| `SET_ALL` `<payload>`              | JSON or bulk CSV  | mass update slots/LED                                                               |
| `GET_ALL`                          | –                 | dump every slot and LED setting                                                     |
| `SET_LED` `<bri>,<r>,<g>,<b>`      | 0‑255 each        | paint LED strip                                                                     |
| `GET_LED`                          | –                 | return `bri,r,g,b`                                                                  |
| `SET_ARGMETHOD` `<n>`              | 0‑13              | choose ARG blend                                                                    |
| `GET_ARGMETHOD`                    | –                 | spit current ARG blend                                                              |
| `SET_EF` `<slot>,<ef>`             | slot 0‑41, ef 0‑5 | patch envelope follower                                                             |
| `GET_EF` `<slot>`                  | slot 0‑41         | see follower mapped                                                                 |
| `CAL_ENVS`                         | –                 | recalibrate all followers                                                           |
| `SET_FILTER` `<type>,<freq>,<q>`   | type 0‑?, floats  | stash EF filter settings                                                            |
| `GET_FILTER`                       | –                 | return `type,freq,q`                                                                |
| `SET_ARGPAIR` `<on>,<envA>,<envB>` | 0/1,0‑5,0‑5       | wire two envelopes for ARG                                                          |
| `GET_ARGPAIR`                      | –                 | echo pair config                                                                    |

### Paint the LEDs

```
SET_LED <brightness>,<r>,<g>,<b>
GET_LED
```

`SET_LED` dials the strip’s brightness and RGB vibe (all 0‑255). `GET_LED`
returns the current `brightness,r,g,b` quartet. Feeling lazy? You can also chuck a
colour hex into `SET_ALL` like `{"led":{"color":"#00ffee"}}` and the board
will wake up wearing that shade.

### Pick an ARG Method

```
SET_ARGMETHOD <n>
GET_ARGMETHOD
```

ARG mode can mash signals in fourteen different ways. Toss an index `0‑13` at
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

The configurator lets you twist:

- MIDI type
- channel
- data bytes
- EF routing
- ARG settings
- LED colours
