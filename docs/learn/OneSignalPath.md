# One Signal Path

This page follows one value through MN42. It is deliberately small.

## One Pot Becomes One MIDI Message

A physical control moves.

Firmware reads the control, maps the raw value into MIDI range, and sends the slot's configured MIDI behavior.

For a CC slot, that means:

```text
control movement -> slot value -> MIDI CC
```

The slot decides channel and message shape.

## One Envelope Follower Becomes Modulation

An envelope follower watches signal level.

The slot can use that follower as a modulation source. The EF settings decide how the level is shaped, smoothed, and folded into the outgoing slot value.

```text
signal level -> EF -> slot modulation -> MIDI value
```

ARG can combine two followers before the slot receives the result.

## One LFO Route Becomes Output

An LFO produces a changing value.

A route decides where that value goes:

- internal runtime bus
- MIDI CC
- slot value
- OSC mirror

```text
LFO -> route transform -> destination
```

Route depth, signed amount, and range determine how far the value moves.

## One App Apply Becomes Firmware Config

The App keeps staged edits separate from live device state.

```text
edit in browser -> staged config -> Apply -> firmware ACK -> live config
```

If Apply fails or the checksum does not match, the App does not pretend the write worked.

## One Bridge Session Moves State

The Bridge can sit between browser and device.

It talks to firmware, caches manifest/schema/config state, exposes a structured session to the App, and can also route OSC or host MIDI.

```text
browser/App -> Bridge session -> device line protocol -> firmware
```

Bridge performance writes and staged firmware config writes are separate lanes. That distinction is part of the instrument.

## Where To Read Next

- [Object Card](../getting-started/ObjectCard.md)
- [Configure Without Recompiling](../getting-started/ConfigureWithoutRecompiling.md)
- [Reactive Control Guide](../guides/ReactiveControlGuide.md)
- [Modulation Matrix Contract](../reference/ModulationMatrixContract.md)
