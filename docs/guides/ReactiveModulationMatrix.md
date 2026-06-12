# Reactive Modulation Matrix

> **Doc class:** Orientation. This page explains the modulation map in performer language; the canonical structured contract is [Modulation Matrix Contract](../reference/ModulationMatrixContract.md).

The modulation matrix answers one production question:

> What is modulating what, by how much, and where does it leave the device?

MN42 currently has several related modulation systems. The goal is to make them read as one map without pretending they are all implemented by one firmware subsystem.

## Sources

| Source            | Meaning                      | Typical Use                                                          |
| ----------------- | ---------------------------- | -------------------------------------------------------------------- |
| Pot               | A physical slot control      | Direct MIDI value or baseline for modulation                         |
| EF                | Envelope follower input      | Audio/CV-driven movement of slot parameters                          |
| ARG               | EF pair math                 | Combined, inverted, or shaped EF behavior before it reaches a target |
| LFO               | Internal cyclic modulation   | Clocked or free movement for slots, MIDI CC, internal buses, or OSC  |
| Bridge live input | Host/browser runtime control | Session testing, performer workflows, and live host integration      |

## Destinations

| Destination   | Meaning                                                                             |
| ------------- | ----------------------------------------------------------------------------------- |
| `slotN.value` | The MIDI value tracked by a slot                                                    |
| MIDI CC route | A channel/CC output lane                                                            |
| Internal bus  | Firmware behavior such as EF gain, arp feel, velocity, probability, gate, or jitter |
| OSC route     | Host-facing OSC output where configured                                             |

## Route Behavior

The current default destination behavior is additive and clamped: a source contributes to a baseline value, then the final result stays inside the valid target range. That is predictable and useful, but the matrix should make it explicit when a route is using add, replace, scale, centered, or another mode.

For now, read existing modulation depth conservatively:

- EF routes are performance modulation of slot behavior.
- ARG routes shape EF pair contribution before it reaches a destination.
- LFO routes can target MIDI CC, slot values, internal buses, or OSC depending on configured route type.
- Bridge live writes are runtime events, not automatically persisted profile edits.

## Collision Story

Multiple sources can intentionally target the same destination. That can be musically useful, but it should be visible.

The matrix should warn, not automatically block, when:

- an EF and LFO both write the same slot value
- two LFO routes hit the same slot or CC
- a live pot movement and a slot-value LFO both emit messages
- EF and LFO CC routes share the same channel/CC
- Bridge live input enters a lane already driven by local modulation

The firmware contract now also reports when the structured matrix is truncated.
If `limits.lfo_route_truncated` is true, treat the report as bounded-but-partial
instead of assuming every configured LFO route made it into the JSON payload.

## Operator View

A production App view should eventually show:

| Source | Transform    | Destination   | Depth  | Range | Active | Last Value |
| ------ | ------------ | ------------- | ------ | ----- | ------ | ---------- |
| `ef0`  | envelope     | `slot3.value` | add    | 0-127 | yes    | 92         |
| `arg0` | ef0 + ef1    | `slot5.value` | add    | 0-127 | yes    | 48         |
| `lfo0` | sine bipolar | `slot7.value` | signed | 20-96 | yes    | 64         |
| `lfo1` | clocked      | `cc14 ch1`    | add    | 0-127 | yes    | 101        |

That view does not need to expose every firmware detail. It needs to show enough to answer what is moving, why it is moving, and where the movement is being sent.

## Reference Links

- [Modulation Matrix Contract](../reference/ModulationMatrixContract.md)
- [ARG Guide](ARGGuide.md)
- [LFO Route Guide](LfoRouteGuide.md)
- [Reactive Control Guide](ReactiveControlGuide.md)
- [Bridge Write Lanes](../bridge/BridgeWriteLanes.md)
