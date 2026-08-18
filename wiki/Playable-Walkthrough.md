# First Playable Walkthrough

This walkthrough gives one exact, reproducible MIDI/reactive-control target
instead of another architecture tour.
Canonical source: `docs/getting-started/MusicianFirstGuide.md`

## What to use

- MOARkNOBS-42 with the current schema-8 firmware contract
- USB data cable
- configurator, direct or through Bridge
- one MIDI synth or DAW instrument
- one audio/control signal connected to envelope input 1
- checked-in configuration:
  [`App/presets/korg/minilogue-init.json`](https://github.com/bseverns/MOARkNOBS-42/blob/main/App/presets/korg/minilogue-init.json)

Download that JSON from GitHub, then upload/import it in the configurator. The
same configuration is available from the preset picker as
`Korg Minilogue XD – Layer Launch`.

## Exact musical map

| Element | Configuration | What proves it |
| --- | --- | --- |
| Physical pot/slot 2 | MIDI channel 2, CC 3, active | MIDI monitor and learned destination move from 0–127 |
| Envelope input 1 | Targets slot 1, MIDI channel 1, note 48 | A strong envelope produces the note/slot response |
| ARG | Enabled, `AVG`, weights A `2.0`, B `1.0` | Reactive movement follows the averaged weighted sources |
| LED | Requests brightness `64`, color `#90FFC0` | Mint identity appears at the active firmware cap (`26` on default `teensy40_main`) |
| Filter/response | `HIGHPASS`, response control `1680`, Q `1.1`, linear envelope mode | Envelope response is visually distinct in telemetry |

The preset contains 42 complete slots. The table names only the checkpoints
needed to make the first session legible.

## Prepare the synth or DAW

1. Create a MIDI instrument track listening on channel 2.
2. MIDI-learn CC 3 to an obvious audible parameter such as filter cutoff.
3. Create or arm a channel-1 destination that makes note 48 audible.
4. If using the Bridge, enable the `MN42 Bridge` MIDI input.
5. Keep a MIDI monitor visible until the mapping is proven.

Host support is setup-specific. This is a mapping recipe, not a universal DAW
certification claim.

## Apply and prove each layer

1. Connect and confirm the expected device/firmware banner.
2. Import or select the configuration.
3. Review the staged diff.
4. Click **Apply** and wait for verified success.
5. Confirm the LED changes to mint. The preset requests brightness `64`, but the default safe firmware caps effective output at `26`; only a separately validated reworked build can emit the full request.
6. Move physical pot/slot 2. Confirm channel-2 CC 3 in the MIDI monitor and hear
   the learned parameter move.
7. Feed a clear dynamic signal into envelope input 1. Confirm envelope
   telemetry moves and the channel-1 note-48 target responds.
8. Compare one follower alone with ARG `AVG` enabled so the combined motion is
   perceptible rather than merely configured.

## Save and recover

1. Save the verified clean state into profile B.
2. Download an external JSON backup.
3. Switch to another profile and return to B.
4. Confirm the mint LED identity, CC 3 movement, and envelope response return.
5. Rehearse `Ctrl0 + Ctrl1 + Ctrl2` once as the path back to the active profile
   baseline.

## Evidence status

The downloadable configuration is committed and revisioned. The repository also
contains a dated App-over-Bridge Apply screenshot/transaction receipt tied to
firmware git `f15aa3a`:
[`docs/bench/app/2026-05-31-app-over-bridge-session-summary.md`](https://github.com/bseverns/MOARkNOBS-42/blob/main/docs/bench/app/2026-05-31-app-over-bridge-session-summary.md).

There is currently **no committed audio/video receipt proving this exact
playable walkthrough**. Capture one on physical hardware before calling this
walkthrough media-verified. The receipt should record:

- firmware git revision and schema version
- the exact JSON configuration checksum/path
- pot 2 → channel-2 CC 3 in a MIDI monitor
- audible destination movement
- envelope-input response and ARG comparison
- mint LED confirmation
- host/DAW name and version

That missing receipt is an evidence gap, not permission to imply the run has
already been observed.
