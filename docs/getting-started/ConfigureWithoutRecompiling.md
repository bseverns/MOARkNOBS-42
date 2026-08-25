# Configure Without Recompiling

Many MN42 performance and mapping behaviors can be configured without recompiling firmware.

That does not make MN42 a generic no-code controller platform. The firmware still defines the instrument. The browser configurator and Bridge expose the parts intended to move during setup, rehearsal, and performance.

## Configuration Boundary Table

| Area          | Configurable Without Recompile                                                        | Still Firmware/Contract Bound                                                       |
| ------------- | ------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| Slots         | message type, channel, primary data field, active state, slot-local modulation fields | slot count, schema shape, supported message types                                   |
| Profiles      | save, load, reset, import, export                                                     | EEPROM layout, profile count, firmware persistence behavior                         |
| Incoming MIDI | source port/channel/CC, destination, interaction, output range, takeover             | binding capacity, supported message class, destination eligibility, takeover rules  |
| EF            | assignment, follower-related slot settings, destination behavior where exposed        | envelope count, input hardware, firmware follower implementation                    |
| ARG           | enable, method, sources                                                               | available methods and source count                                                  |
| LFO routes    | route type, target, amount/depth/range where supported                                | route count, target enum, output scheduler behavior                                 |
| Bridge        | serial path, OSC/MIDI ports, host recipes, App-over-Bridge workflow                   | staged/live write boundaries and Bridge/device session contract                     |

## Browser Configurator

Use the browser configurator for the main staged configuration workflow:

- inspect manifest, schema, and current config
- edit slots
- stage changes locally
- validate against schema
- Apply to firmware
- save, load, reset, and export profiles

The safe rule is simple: staged edits do not become live firmware truth until Apply succeeds and the device acknowledges the write.

## Slots

Slots can be configured without recompiling:

- MIDI message type
- MIDI channel
- CC or primary data field
- active/muted state
- slot-local EF settings
- slot-local ARG settings

The exact available fields depend on the current firmware manifest and schema.

## Profiles

Profiles let the device store working states instead of rebuilding a setup from scratch. The App exposes profile workflows for saving, loading, resetting, importing, and exporting profile data.

Use profiles for rehearsal states, show setups, and experiments you want to recover.

## Incoming MIDI

Incoming MIDI bindings are profile-owned firmware configuration, not Bridge host mappings. In Lab, use **Profile Performance → Incoming MIDI** to configure:

- DIN, USB, or either input port;
- MIDI channel and 7-bit CC number;
- a supported slot or machine-level destination;
- Continuous, Momentary, or Toggle interaction;
- output minimum and maximum;
- soft pickup or jump takeover for Continuous routes.

The current implementation accepts only CC7 input. Slot destinations are intentionally limited to active, direct, unmodulated CC slots so external ownership and physical takeover remain unambiguous. See [MIDI Input Mapping](../guides/MidiInputMapping.md).

## EF Settings

Envelope follower behavior can be configured per slot:

- assigned follower index
- response/filter shape
- detection mode
- attack/release or related timing
- baseline and gain behavior
- destination behavior for how EF contribution affects the slot value

EF changes are still bounded by firmware support. The configurator should fail closed when the connected firmware does not advertise a capability.

## ARG Settings

ARG blends two envelope follower sources before the slot uses the modulation result. Per-slot ARG settings include:

- enable/disable
- method
- source A
- source B

Use [ARG Guide](../guides/ARGGuide.md) when choosing a method by feel rather than by formula.

## LFO Routes

LFO routes can target internal behavior, MIDI/CC output, slot values, or OSC mirrors depending on firmware support.

Current route controls include:

- LFO index
- route type
- target
- depth
- signed amount
- output min/max range

Use [LFO Route Guide](../guides/LfoRouteGuide.md) and [Modulation Matrix Contract](../reference/ModulationMatrixContract.md) for the deeper model.

## Bridge And Session Workflows

Use the Bridge when the browser cannot or should not talk directly to the device, or when the performance setup needs OSC or virtual MIDI.

Bridge-side configuration can include:

- serial path
- OSC host/ports
- MIDI port label
- custom MIDI-to-OSC mappings
- cached structured device session
- App-over-Bridge workflow

Bridge live-control writes are not the same as staged firmware config writes. See [Bridge Write Lanes](../bridge/BridgeWriteLanes.md).

## Conservative Boundary

Without recompiling does not mean without proof.

Before treating a workflow as production-safe, check:

- [Host Compatibility](../reference/HostCompatibility.md)
- [Connectivity Guide](ConnectivityGuide.md)
- [TESTING](../validation/TESTING.md)
- [Release Criteria](../release/ReleaseCriteria.md)
