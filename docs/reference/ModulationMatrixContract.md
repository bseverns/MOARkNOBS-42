# Modulation Matrix Contract

> **Doc class:** Contract doc. This page defines the canonical structured report for EF, ARG, LFO, slot, MIDI, and OSC modulation routing.

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
  "type": "mod_matrix",
  "command": "GET_MOD_MATRIX",
  "contract_version": 1,
  "limits": {
    "lfo_route_capacity": 8,
    "lfo_route_total": 3,
    "lfo_route_reported": 3,
    "lfo_route_truncated": false
  },
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
      "amount": 100,
      "minValue": 0,
      "maxValue": 127,
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
- `mode: "add_clamp"` is the default EF behavior: EF contribution is added to
  the baseline pot MIDI value and clamped to `0..127`.
- EF routes may also report `subtract`, `replace`, `scale`, or `centered`.
  These destination modes are persisted with the slot EF profile settings.
- Fixed LFO lane `add_clamp` and `subtract` modes consume normalized `0..1`
  oscillator values and contribute up to `0..127` at 100% amount. Fixed-lane
  `centered` consumes the bipolar `-1..1` value and contributes `-64..+63`;
  `replace` and `scale` also use the bipolar value.
- A persisted global `SlotValue` route reports `legacy_replace` and preserves
  its historical baseline-independent `0..127` output. It reports
  `legacy_shadowed` when the corresponding fixed lane owns that LFO/slot pair.
- `mode: "pre_add_arg"` means ARG math is shaping the EF contribution before the
  final EF add/clamp destination.
- `mode: "add_bus"` means an LFO is summed into an internal runtime modulation
  bus such as EF gain trim, arp swing, velocity shift, probability, gate, or
  jitter tuning.
- `rateLimitMs` is present for LFO routes that emit transport-facing values.

## Conflicts

`conflicts` currently reports shared MIDI CC writers and shared slot-value
writers. A warning is not a failure: collisions can be musically useful, but
the device must make them visible.

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

## Route Amount And Range

LFO routes persist the first transform controls in profile payload version 3:

- `amount`: signed percent from `-100..100`; negative values invert the route
  around center, and smaller absolute values reduce modulation travel.
- `minValue` / `maxValue`: direct aliases for the persisted profile route fields.
- `range.min` / `range.max`: the outgoing transport range in MIDI units
  `0..127`.

Older profile payloads migrate as `amount: 100`, `range.min: 0`, and
`range.max: 127`.

## Validation And Truncation

- `SET_PROFILE` route patches reject unsupported route types, LFO indexes,
  slot indexes, internal targets, MIDI channels, and CC indexes before the
  profile is persisted.
- Route scalar fields are normalized at the command boundary:
  `depth` clamps to `0..1`, `amount` clamps to `-100..100`, and
  `minValue` / `maxValue` are clamped to `0..127` then reordered ascending.
- `limits.lfo_route_truncated` reports when live firmware state contains more
  LFO routes than the current structured report emits, so hosts do not mistake
  the matrix for a complete list.

## Planned Extensions

Future profile versions should add optional offset, slew, and curve.
