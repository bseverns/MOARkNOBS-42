# Glossary

This glossary exists so new users do not have to infer the meaning of the project’s favorite words from scattered screenshots, commit history, or firmware symbols.

## Core state terms

### Preset

A browser-loaded template from the configurator picker. Presets stage candidate mappings locally first. They are not permanent device memory until you apply them and optionally save them into a device profile.

See: [Preset Library](../guides/PresetLibrary.md)

### Profile

One of the four EEPROM-backed snapshots stored on the device as slots A-D. Profiles are persistent device memory.

See: [Profile Workflow](../guides/ProfileWorkflow.md)

### Live config

The last configuration the device actually confirmed back to the browser.

See: [Configurator Tour](../guides/Configurator.md)

### Staged config

The editable draft currently living in the browser UI. This is where most changes happen before Apply.

See: [Configurator Tour](../guides/Configurator.md)

### Apply

The action that sends staged browser state to the device and waits for a confirming acknowledgement before treating the change as real.

See: [WebSerial Walkthrough](../guides/ProtocolWalkthrough.md)

### Rollback

Rollback or **Discard draft** abandons an untransmitted staged draft and returns the editor to current verified device truth. It does not describe a transmitted Apply failure. Once Apply bytes may have reached the device, the outcome is uncertain until authoritative readback resolves it.

See: [Failure-First Guide](../validation/FailureFirst.md)

### Recipe

An App-side, deterministic patch for one selected slot or modulation subtree. Recipes stage existing schema fields; they are not whole-instrument presets, device profiles, new firmware parameters, or an alternate validation schema.

See: [Configurator Tour](../guides/Configurator.md#selected-slot-tuning-translates-lab-it-does-not-replace-it)

## Contract terms

### Manifest

The device identity and capability report the browser requests after `HELLO`. It includes firmware version, schema version, counts, and build metadata.

See: [WebSerial Walkthrough](../guides/ProtocolWalkthrough.md)

### Schema

The structural contract describing what a valid config looks like. The UI validates edits against it before Apply is allowed.

See: [WebSerial Protocol](../guides/WebSerial.md)

### Patch

A smaller update frame describing a change to one slice of state instead of a full config dump.

See: [WebSerial Protocol](../guides/WebSerial.md)

## Musical/control terms

### Slot

One of the 42 mapping lanes on the controller. A slot usually carries a MIDI message type, channel, number/value metadata, active state, and optional EF/ARG settings.

See: [Preset Library](../guides/PresetLibrary.md)

### EF

Short for envelope follower. It turns an incoming signal’s changing level into a control value that the firmware can route into slots, LEDs, ARG, or other behavior.

See: [Reactive Control Guide](../guides/ReactiveControlGuide.md)

### SEF

Single-envelope-follower behavior: one envelope source driving one slot path directly.

See: [Reactive Control Guide](../guides/ReactiveControlGuide.md)

### ARG

The two-input envelope-blending mode. ARG combines two envelope signals using one of several math methods to create a new modulation stream.

See: [ARG Guide](../guides/ARGGuide.md)

### Filter type

The shaping applied to envelope follower output. Different filters change whether the response feels smooth, sharp, inverted, noisy, or frequency-selective.

See: [Filter Feel Guide](../guides/FilterFeelGuide.md)

### Envelope mode

The response style for the follower path, such as linear or exponential behavior. This changes the feel of the controller as much as it changes the numbers.

See: [Preset Library](../guides/PresetLibrary.md)

### LFO

Low-frequency oscillator. In this project it can modulate internal targets like LED brightness, arp swing, and EF gain trim, or route outward to MIDI/OSC.

See: [LFO Route Guide](../guides/LfoRouteGuide.md)

### Route

A mapping from an LFO to a destination. A route can target an internal behavior or an outbound MIDI/OSC destination.

See: [LFO Route Guide](../guides/LfoRouteGuide.md)

### Combo

A hardware button combination that changes mode, mapping, timing, recovery state, or device behavior.

See: [Combo Guide](../guides/ComboGuide.md)

## Workflow terms

### Save profile

Persist the current device state into one of the four A-D profile slots.

### Load profile

Recall one of the four stored device snapshots from EEPROM and make it the active state.

### Reset profile

Replace the active profile with its default baseline and push that clean state back through the same profile machinery.

## Read next

- [Guided Routes](../getting-started/GuidedRoutes.md) if you want a structured way through the docs
- [Preset Library](../guides/PresetLibrary.md) if you want examples instead of definitions
- [Failure-First Guide](../validation/FailureFirst.md) if you want to learn by recovery scenarios
