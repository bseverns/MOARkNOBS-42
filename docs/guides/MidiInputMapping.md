# MIDI Input Mapping

MOARkNOBS-42 can consume MIDI control changes as profile-owned performance
input. The routing runs in firmware, so DIN and USB mappings continue to work
without the App or Bridge attached.

## Binding Shape

Schema 9 adds an optional `midiInputBindings` array with up to 16 routes:

```json
{
  "source": { "port": "din", "type": "cc7", "channel": 1, "number": 74 },
  "destination": "arp.swing",
  "mode": "absolute",
  "outputRange": [0, 127],
  "pickup": "soft"
}
```

`port` may be `din`, `usb`, or `any`. The first implementation supports CC7
input with `absolute`, `momentary`, and `toggle` modes. Continuous routes use
soft pickup by default; `jump` applies the first received value immediately.
Toggle routes advance their internal on/off state only after the destination
accepts the proposed write. A rejected edge therefore retries the same value
on the next rising edge instead of inverting a change the machine never made.

## Destinations

- `slot.0.value` through `slot.41.value` for active, direct, unmodulated CC slots
- `arp.swing` and `arp.gate`
- `note.velocity_shift` and `note.probability`
- `jitter.depth` and `jitter.smoothness`

Slot input deliberately starts with direct CC slots. Note, NRPN/RPN, SysEx,
and independently modulated slot baselines require richer origin-aware event
semantics and are rejected rather than producing ambiguous output.

## Takeover and Feedback

An externally injected slot value owns the virtual pot until the physical pot
crosses that value. This prevents the next mux scan from snapping the output
back to the knob's old position.

For slot routes, firmware suppresses output to the MIDI port that originated
the write. A DIN input may still update USB and a USB input may still update
DIN, allowing the machine to participate in a rig without immediately echoing
the message back toward its source. Ordinary MIDI thru remains separate and is
not enabled implicitly.

RPN/NRPN selector and data-entry CCs (`6`, `38`, `98`–`101`) stay reserved for
the parameter decoder and are never interpreted as ordinary CC7 bindings.

## Persistence

Mappings travel with profiles and are committed only through the verified
configuration Apply/profile-save path. Incoming performance values are runtime
state and never cause one EEPROM write per MIDI message. Legacy profile
modulation records migrate with an empty binding table.

In the App's Lab view, open **Profile Performance → Incoming MIDI** to add or
remove routes, then Apply the staged configuration. The workspace is profile-owned rather than part of
**Selected Slot** because a route may target any slot or a machine-level performance parameter.
`GET_CONFIG`/`SET_ALL` and
`GET_PROFILE`/`SET_PROFILE` use the same authoritative JSON codec, so profile
exports round-trip the bindings without a second representation.

The binding editor groups each route into Incoming CC, Destination, and Response.
Since the current firmware accepts only 7-bit Control Change routes, message type
is stated rather than presented as a one-option selector. Destinations are selected by operator-facing name while the App
continues to stage the canonical machine path. Collapsed routes summarize the
port, channel, CC, target, interaction, and range. Output minimum and maximum
remain ordered in the editor so an invalid range cannot be staged accidentally.
Takeover is available only for Continuous interaction. Switching to Momentary or
Toggle disables that control without discarding the route's staged takeover value.

The Bridge's historical CC-number-to-slot adapter remains a host compatibility
path. It is not the authority for the device-owned mappings described here.
