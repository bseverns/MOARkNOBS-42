# Failure-First Guide

A lot of people learn instruments by joyful misuse. That is fine. But for a device like this, recovery stories are often the fastest route to understanding.

This page explains the most common "wait, what just happened?" situations in the stack and what they usually mean.

## 1. Preset staged, but nothing changed on the device

What happened:

- you chose a preset in the picker
- the browser staged it locally
- you did not Apply yet

Why this exists:

- it protects the live deck from silent overwrite
- it lets you inspect changes before committing them

What to do:

1. review the staged diff
2. press Apply
3. save into a profile only if you want permanence

See: [Preset Library](PresetLibrary.md), [Profile Workflow](ProfileWorkflow.md)

## 2. Apply failed or rolled back

What happened:

- the browser sent staged state
- the device did not confirm it correctly
- the runtime rolled back rather than pretending success

Why this exists:

- it keeps the UI honest
- it protects you from phantom state

What to do:

1. check connection health
2. check schema compatibility
3. retry only after the status makes sense

See: [Configurator Tour](Configurator.md), [WebSerial Walkthrough](ProtocolWalkthrough.md)

## 3. Schema mismatch warning appears before editing

What happened:

- the browser manifest check found that the UI and device do not agree on config structure

Why this exists:

- editing across schema drift is how you create invisible corruption

What to do:

1. export anything important
2. update firmware or UI as needed
3. only use migration adapters when the UI explicitly offers one

See: [WebSerial Walkthrough](ProtocolWalkthrough.md), [Glossary](Glossary.md)

## 4. Loading a profile wiped the thing you were editing

What happened:

- you told the device to recall its saved EEPROM snapshot
- the browser then synced to that newly loaded state

Why this exists:

- loading a profile is a replacement action, not a merge

What to do:

- export your current staged work before loading if you may want it later

See: [Profile Workflow](ProfileWorkflow.md)

## 5. The board recovered from EEPROM backup or defaulted on boot

What happened:

- the primary saved config was invalid
- firmware recovered from backup, or fell all the way back to defaults

Why this exists:

- the instrument prefers degraded recovery over silent corruption

What to do:

1. reconnect and inspect the current config/profile state
2. reload a known-good profile if needed
3. save again once the state is trustworthy

See: [BuildersHandbook](BuildersHandbook.md), [EEPROM Layout](EEPROMLayout.md)

## 6. Reactive behavior feels wrong, noisy, or harder than expected

What happened:

- your EF/filter/ARG combination is doing more than you think

Why this exists:

- reactive controls are compositional, not just numeric

What to do:

1. reduce to one envelope follower
2. switch to `LINEAR` or `LOWPASS`
3. disable ARG
4. reintroduce complexity one step at a time

See: [Reactive Control Guide](ReactiveControlGuide.md)

If the confusion is specifically about blending or shape, read:

- [ARG Guide](ARGGuide.md)
- [Filter Feel Guide](FilterFeelGuide.md)
- [LFO Route Guide](LfoRouteGuide.md)

## 7. Bridge is connected, but your DAW or OSC tool still feels dead

What happened:

- the bridge may be up, but the wrong port, path, or endpoint is still in play

What to do:

1. confirm serial path
2. confirm handshake appears once
3. confirm UDP port and address match the docs
4. relaunch the DAW if the virtual MIDI device came up late

See: [Bridge for Performers](BridgeForPerformers.md)

## 8. You do not know whether to use browser, bridge, preset, or profile

Use this shortcut:

- browser: editing and inspection
- bridge: live OSC/DAW workflows
- preset: browser-side starting template
- profile: device-side saved memory

If the confusion is about terms rather than behavior, read [Glossary](Glossary.md).

## Read next

- [Troubleshooting](Troubleshooting.md) for physical hardware failures
- [Glossary](Glossary.md) for the vocabulary behind these failure cases
- [Guided Routes](GuidedRoutes.md) if you want a calmer route through the site
