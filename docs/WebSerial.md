# WebSerial Groove

The Teensy screams JSON snapshots over WebSerial so the browser can watch the synth wiggle in real time. It's dead‑simple and a little rowdy.

## Handshake

1. Browser opens the serial port at **115200** baud.
2. Browser sends `HELLO\n`.
3. Teensy answers with:
   ```json
   { "hello": "mn42" }
   ```
   ![Browser console showing HELLO handshake with JSON response](webserial_handshake.svg)
4. Browser immediately fires off `GET_MANIFEST\n` so the synth and the UI agree on schema + build trivia. The Teensy replies
   with a newline-terminated JSON manifest:
   ```json
   {
     "fw_version":"1.3.0",
     "git_sha":"012dead",
     "build_time":"2024-05-10 21:37:02",
     "schema_version":4,
     "slot_count":42,
     "pot_count":42,
     "envelope_count":6,
     "arg_method_count":14,
     "led_count":51,
     "free_ram":642112,
     "free_flash":1185792
   }
   ```
5. The UI diffs that manifest against its baked-in schema definition. When anything smells off, pop a non-destructive migrate dialog:
   - Offer to export the user’s current JSON before touching a byte.
   - Run any adapters found in `firmware/App/migrations/` to lift the preset forward.
   - Render the proposed patch for review; no silent rewrites.
6. Streaming begins only after both sides agree on versions. Bail out by closing the port or if the manifest validation fails.

## State Messages

Every ~100 ms the firmware spits a newline‑terminated JSON blob:

```json
{
  "slots":[0,1,2,...,41],
  "envelopes":[0,0,0,0,0,0],
  "currentSlot":5,
  "argMethod":"PLUS",
  "efStatus":[1,0,0,0,0,1],
  "diagnostics":{
    "uart_overruns":0,
    "midi_drops":0,
    "loop_overruns":0,
    "midi_task_overruns":0,
    "loop_max_us":812,
    "loop_last_us":742,
    "midi_isr_max_us":320,
    "midi_isr_last_us":110
  }
}
```

- `slots` – 42 MIDI‑scaled values (0‑127) for each virtual slot.
- `envelopes` – live levels from the six envelope followers, also 0‑127.
- `currentSlot` – which slot is currently screaming.
- `argMethod` – firmware's current ARG calculation mode.
- `efStatus` – array of six flags; `1` means that envelope follower is lit.
- `diagnostics` – brand-new watchdog metrics surfacing how hard the MCU is being pushed:
  - `uart_overruns` – count of DIN UART receive overruns caught in hardware.
  - `midi_drops` – messages we binned due to busted framing or unsupported types.
  - `loop_overruns` – main loop iterations that blew past the 1 ms target.
  - `midi_task_overruns` – MIDI service passes that ran longer than 1 ms.
  - `loop_max_us` / `loop_last_us` – worst and most recent loop durations in microseconds.
  - `midi_isr_max_us` / `midi_isr_last_us` – same deal for `processIncomingMIDI()`.

When any of those counters tick upward the status LED on the board pulses and the JSON stream logs an event, so you get both a visual scream and structured telemetry.

Parse each line as JSON and redraw your UI. There’s no framing besides the newline, because who needs more ceremony?

## Patch Messages

Streaming snapshots are the vibe, but the firmware also fires micro-patches whenever a single slice of state changes. These are
newline-terminated JSON blobs as well, all prefixed with a `type` field so your app can route them without guesswork.

| `type` value | When it fires | Payload notes |
| --- | --- | --- |
| `slot_patch` | Slot MIDI settings mutate (from the board or the browser) | `slot` index plus a nested `slot` object including `type`, `schema_name`, `legacy_name`, `channel`, resolved `data1`, EF routing, and active flag. That mirrors exactly what the editor expects, so you can diff-and-apply without a decode step. |
| `envelope_assignment` | A slot gets re-routed to a new follower | `slot` index and `envelope` index (or `-1` if unassigned). Use it to repaint routing badges. |
| `filter_patch` | Envelope follower filter tweaked live | `filter` object includes type index/name, frequency, and Q. Perfect for sliding UI knobs without yanking the full config. |
| `arg_patch` | ARG mixer mode flips or toggles | Nested `arg` object with the method index/name, enable flag, and the paired envelopes so remote editors stay in sync. |

Every patch obeys the same “don’t spam unless streaming” rule as snapshots. If WebSerial streaming is paused, patches quietly bail
so the USB line isn’t clogged when no one’s listening.

## Text Commands

Spying is fun, but sometimes you gotta bark orders. Hurl plain‑text commands
terminated with a newline and the firmware snaps back with either `OK` or `ERR`.

### Command Roster

This table mirrors the shout list in `firmware_main.cpp` so the code and docs
never fall out of sync.

| Command | Arguments | What it pokes |
| --- | --- | --- |
| `HELLO` | – | start WebSerial streaming |
| `GET_SCHEMA` | – | dump config schema. If the device ghosts you, the HTML app drags out its baked-in `config_schema.json`. |
| `GET_BROWNOUTS` | – | number of brownouts seen |
| `GET_MANIFEST` | – | confirm firmware build + schema, plus free RAM/flash stats |
| `GET_CONFIG` | – | dump the live configuration: slots, pots, envelope routing, ARG/filter state, LEDs |
| `SET_POT` `<slot>,<chan>,<cc>` | ints | bind slot to channel+CC |
| `SET_ALL` `<payload>` | JSON or bulk CSV | mass update slots/LED |
| `GET_ALL` | – | dump every slot and LED setting |
| `SET_LED` `<bri>,<r>,<g>,<b>` | 0‑255 each | paint LED strip |
| `GET_LED` | – | return `bri,r,g,b` |
| `SET_ARGMETHOD` `<n>` | 0‑13 | choose ARG blend |
| `GET_ARGMETHOD` | – | spit current ARG blend |
| `SET_EF` `<slot>,<ef>` | slot 0‑41, ef 0‑5 | patch envelope follower |
| `GET_EF` `<slot>` | slot 0‑41 | see follower mapped |
| `CAL_ENVS` | – | recalibrate all followers |
| `SET_FILTER` `<type>,<freq>,<q>` | type 0‑?, floats | stash EF filter settings |
| `GET_FILTER` | – | return `type,freq,q` |
| `SET_ARGPAIR` `<on>,<envA>,<envB>` | 0/1,0‑5,0‑5 | wire two envelopes for ARG |
| `GET_ARGPAIR` | – | echo pair config |

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

When you need the whole wiring diagram, `GET_CONFIG` is the firehose. It prints
one JSON line mirroring the firmware’s in-RAM state—slot definitions, pot CC
bindings, envelope routing, ARG selections, filter coefficients, and LED mood.
Parse it once and drive your UI off that single source of truth.

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

## Editor Contract Upgrades

These are the ground rules for keeping a responsive UI without bricking rigs on stage. Think of it as a pact between firmware, browser, and the human in the loop.

### Single Source of Truth Handshake

- Treat the manifest response as sacred. Cache it per session and surface a status pill that shouts where you stand: `Disconnected → Handshake → Live`. Flip to `Dirty` whenever staged edits drift from what the device most recently confirmed.
- If the schema diff fails, throw the migrate dialog mentioned above. The only destructive action allowed is a deliberate “Apply Migration” click.

### Atomic Apply with Commit/Rollback

- Stage every tweak locally (Redux store, Svelte store, whatever keeps you honest).
- Clicking **Apply** should pack the pending diff into one message (`SET_ALL` payload or chunked frames with sequence IDs).
- Firmware validates and replies with `{ "checksum": "deadbeef" }`. Only then do you commit locally.
- When the checksum doesn’t match expectations, auto-rollback your local state and show a diff panel so the player knows what the device actually accepted.

### Bidirectional Throttling

- **Inbound**: Buffer device updates and paint them on `requestAnimationFrame` or `setTimeout(..., 16)`. Reflowing the DOM on every line will tank performance.
- **Outbound**: Debounce knobs and sliders to at least 16–24 ms and batch bursts so the Teensy’s USB write locks don’t starve the UI thread.
- Slot streaming at 30–60 Hz keeps things lively without choking browsers or MIDI bandwidth.

### Schema-Versioned Presets

- Every exported preset should include `"schema_version": <int>` at the top-level.
- Stash migration scripts in `/migrations/` as pure JavaScript transforms: `(preset) => nextPreset`.
- When someone drags in a vintage preset, detect the version mismatch, show the migration plan, and let them preview the transformed diff before writing it to the device.

### Deterministic Control IDs

- Represent every control as `{id, human_label, type, range, target}`.
- Only ship IDs and machine-readable targets over the wire. Labels live purely in the UI so you can rename things without breaking stored presets or firmware expectations.

### UX Guardrails

- Connection status pill (Disconnected / Handshake / Live / Dirty) lives in the header.
- Inline validators clamp ranges, show friendly tooltips, and link to the tables in `docs/` (filters, ARG, MIDI types) for context.
- Surface a read-only **Device Monitor** sidebar that streams the manifest, firmware build info, free RAM/flash, and current profile.

### Safe Writes for Pots vs. Encoders

- Encoders nudge values in real time—write as the player twists.
- Potentiometers snap. Keep their live writes behind an explicit **Take Control** toggle so you don’t surprise someone mid-set.

### Teaching + QA Mode

- Ship a WebSerial simulator that mimics the Teensy protocol for classrooms, CI screenshots, and unit tests.
- Mocking the serial layer lets you verify migrations, diff previews, and throttling without hardware on every desk.

### Accessibility + Layout

- Headings stay sequential, controls get explicit labels, and hit areas stay chunky enough for shaky hands.
- Keyboard navigation mirrors hardware muscle memory: arrow keys step values, `Shift` toggles coarse/fine adjustments.
- Document the shortcuts inside the UI so folks using assistive tech don’t have to guess.
