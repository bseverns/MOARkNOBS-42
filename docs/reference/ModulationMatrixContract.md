# Modulation Matrix Contract

`GET_MOD_MATRIX` is the canonical read-only report for live modulation routing.
It names the current firmware behavior across EF, ARG, LFO, slot, MIDI, and OSC
lanes without requiring hosts to infer routing from profiles plus runtime code.

## Command

```text
GET_MOD_MATRIX
```

The response is JSON:

```json
{
  "command": "GET_MOD_MATRIX",
  "contract_version": 1,
  "sources": {
    "ef": [0, 1, 2, 3, 4, 5],
    "lfo": [0, 1],
    "pot": [0, 1, 2]
  },
  "routes": [
    {
      "id": "lfo0_route0",
      "source": "lfo0",
      "source_type": "lfo",
      "transform": "sine bipolar lfo_depth 1.00 route_depth 0.50",
      "destination": "slot7.value",
      "mode": "replace",
      "exit": "midi",
      "range": { "min": 0, "max": 127 },
      "rateLimitMs": 9,
      "persisted": true,
      "active": true
    }
  ],
  "conflicts": []
}
```

## Route Semantics

- `mode: "replace"` means the source writes the outgoing destination value.
- `mode: "add_clamp"` is the current EF behavior: EF contribution is added to
  the baseline pot MIDI value and clamped to `0..127`.
- `mode: "pre_add_arg"` means ARG math is shaping the EF contribution before the
  final EF add/clamp destination.
- `mode: "add_bus"` means an LFO is summed into an internal runtime modulation
  bus such as EF gain trim, arp swing, velocity shift, probability, gate, or
  jitter tuning.
- `rateLimitMs` is present for LFO routes that emit transport-facing values.

## Conflicts

`conflicts` currently reports shared MIDI CC writers. A warning is not a failure:
collisions can be musically useful, but the device must make them visible.

Example:

```json
{
  "target": "midi.cc",
  "channel": 1,
  "cc": 74,
  "writers": "pot7, ef0_slot7, lfo0_route0",
  "message": "3 live modulators write CC 74 on channel 1"
}
```

## Planned Extensions

Future profile versions should add signed amount and output range to persisted
routes first, then optional offset, slew, and curve. EF destination behavior
should keep today&apos;s `add_clamp` default but expose explicit `add`,
`subtract`, `replace`, `scale`, and `centered` modes.
