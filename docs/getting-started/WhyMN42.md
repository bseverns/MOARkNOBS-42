# Why MN42

MOARkNOBS-42 is a performance instrument for deciding what gets to move a control.

A performer's hand can establish a value. Sound can disturb it through envelope followers. Two signals can be combined through ARG. Time can move it through an LFO. For eligible direct CC slots and machine-level parameters, another MIDI device can take control; soft pickup lets the performer cross that remote value and take the slot back without a jump.

That shifting relationship between body, environment, time, and other machines is the musical reason MN42 exists.

## Control is a relationship

MN42 is not a bank of knobs with modulation added around the edges. Its 42 slots are places where control relationships can be configured, recalled, observed, and performed.

Two paths make the idea concrete:

- **Reactive composition:** a physical baseline can be shaped by EF/ARG behavior and LFO routes according to their documented add, subtract, replace, scale, or centered semantics.
- **External takeover:** profile-owned incoming MIDI can write an eligible direct, unmodulated CC slot or a supported machine parameter. Soft pickup prevents the physical pot from snapping the value back until it crosses the externally established value.

Those paths are related, but they are not one universal arbitration engine. Current firmware deliberately rejects ambiguous incoming-MIDI targets such as independently modulated slot baselines. The constraint keeps the musical result explainable.

Read [Who Controls This Slot?](../learn/OneSignalPath.md) for the visual version, then [Reactive Control Guide](../guides/ReactiveControlGuide.md) and [MIDI Input Mapping](../guides/MidiInputMapping.md) for the two deeper paths.

## Legibility makes the behavior playable

Unusual control relationships are useful only if a performer can understand and recover them.

MN42 keeps that logic visible through:

- open hardware and firmware that can be inspected and rebuilt;
- a browser App with separate Stage, Configure, and Lab surfaces;
- profiles that keep reactive settings and incoming MIDI routes with the device;
- a local Bridge that separates host routing from instrument configuration;
- contracts that define exact behavior instead of asking the UI to invent it;
- evidence documents that distinguish a demonstrated path from a general claim.

Openness is therefore not the whole thesis. It is what makes this reactive instrument teachable, auditable, and trustworthy.

## More than a sealed controller

Many controllers hide value ownership inside presets, host mappings, or companion software. MN42 makes the movement itself inspectable:

- what established the baseline;
- which reactive sources contributed;
- where a route exits;
- when external MIDI owns a virtual pot;
- when the performer's hand has taken it back;
- what is live now versus staged for Apply.

The physical surface, firmware, App, Bridge, profiles, and documentation are parts of one instrument because each helps answer those questions.

## Who this matters to

MN42 makes the most sense for:

- experimental musicians who want control movement to become compositional material;
- sound and media artists working across audio, MIDI, OSC, and physical gesture;
- educators who want modulation, takeover, and state authority to remain visible;
- instrument hackers who want to audit, rebuild, or alter the system without pretending unverified behavior is proven.

If that sounds like you, continue with [Who Controls This Slot?](../learn/OneSignalPath.md), [Quickstart for Performers](QuickstartForPerformers.md), or [Quickstart for Builders](QuickstartForBuilders.md).
