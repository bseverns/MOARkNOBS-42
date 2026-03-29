# Glossary

This glossary exists so new users do not have to infer the meaning of the project’s favorite words from scattered screenshots, commit history, or firmware symbols.

## Core state terms

### Preset

A browser-loaded template from the configurator picker. Presets stage candidate mappings locally first. They are not permanent device memory until you apply them and optionally save them into a device profile.

See: [Preset Library](PresetLibrary.md)

### Profile

One of the four EEPROM-backed snapshots stored on the device as slots A-D. Profiles are persistent device memory.

See: [Profile Workflow](ProfileWorkflow.md)

### Live config

The last configuration the device actually confirmed back to the browser.

See: [Configurator Tour](Configurator.md)

### Staged config

The editable draft currently living in the browser UI. This is where most changes happen before Apply.

See: [Configurator Tour](Configurator.md)

### Apply

The action that sends staged browser state to the device and waits for a confirming acknowledgement before treating the change as real.

See: [WebSerial Walkthrough](ProtocolWalkthrough.md)

### Rollback

The browser-side recovery path when an Apply fails or the acknowledgement does not match expectations. The UI returns to known-good state instead of pretending the device accepted the change.

See: [Failure-First Guide](FailureFirst.md)

## Contract terms

### Manifest

The device identity and capability report the browser requests after `HELLO`. It includes firmware version, schema version, counts, and build metadata.

See: [WebSerial Walkthrough](ProtocolWalkthrough.md)

### Schema

The structural contract describing what a valid config looks like. The UI validates edits against it before Apply is allowed.

See: [WebSerial Protocol](WebSerial.md)

### Patch

A smaller update frame describing a change to one slice of state instead of a full config dump.

See: [WebSerial Protocol](WebSerial.md)

## Musical/control terms

### Slot

One of the 42 mapping lanes on the controller. A slot usually carries a MIDI message type, channel, number/value metadata, active state, and optional EF/ARG settings.

See: [Preset Library](PresetLibrary.md)

### EF

Short for envelope follower. It turns an incoming signal’s changing level into a control value that the firmware can route into slots, LEDs, ARG, or other behavior.

See: [Reactive Control Guide](ReactiveControlGuide.md)

### SEF

Single-envelope-follower behavior: one envelope source driving one slot path directly.

See: [Reactive Control Guide](ReactiveControlGuide.md)

### ARG

The two-input envelope-blending mode. ARG combines two envelope signals using one of several math methods to create a new modulation stream.

See: [ARG Guide](ARGGuide.md)

### Filter type

The shaping applied to envelope follower output. Different filters change whether the response feels smooth, sharp, inverted, noisy, or frequency-selective.

See: [Filter Feel Guide](FilterFeelGuide.md)

### Envelope mode

The response style for the follower path, such as linear or exponential behavior. This changes the feel of the controller as much as it changes the numbers.

See: [Preset Library](PresetLibrary.md)

### LFO

Low-frequency oscillator. In this project it can modulate internal targets like LED brightness, arp swing, and EF gain trim, or route outward to MIDI/OSC.

See: [LFO Route Guide](LfoRouteGuide.md)

### Route

A mapping from an LFO to a destination. A route can target an internal behavior or an outbound MIDI/OSC destination.

See: [LFO Route Guide](LfoRouteGuide.md)

### Combo

A hardware button combination that changes mode, mapping, timing, recovery state, or device behavior.

See: [Combo Guide](ComboGuide.md)

## Workflow terms

### Save profile

Persist the current device state into one of the four A-D profile slots.

### Load profile

Recall one of the four stored device snapshots from EEPROM and make it the active state.

### Reset profile

Replace the active profile with its default baseline and push that clean state back through the same profile machinery.

### Take Control

A browser-local pickup guard used to prevent sudden live-value jumps, especially when a physical control may not match the currently live or staged state.

With the guard on, the configurator waits until the control passes through the current effective value before treating that control as active again.

See: [Operator Tutorial](OperatorTutorial.md)

### Immediate local response

The direct local-control mode used by the configurator when pickup guarding is off.

With immediate response, the browser accepts local control input right away instead of waiting for the control to catch the current value first.

See: [Configurator Tour](Configurator.md)

## Read next

- [Guided Routes](GuidedRoutes.md) if you want a structured way through the docs
- [Preset Library](PresetLibrary.md) if you want examples instead of definitions
- [Failure-First Guide](FailureFirst.md) if you want to learn by recovery scenarios
