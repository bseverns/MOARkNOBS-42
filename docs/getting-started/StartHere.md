# New User Story

If you are new to MOARkNOBS-42, the easiest mistake is to think it is "just a controller with a lot of knobs." It is closer to a small system with a very visible control surface.

This page is the beginner-friendly tour: what the parts are, how they relate, and what to read next when you want to go deeper.

If you need a brutally short first path, start with [First Five Minutes](FirstFiveMinutes.md). It sorts the first step into four choices: play it, configure it, build/test it, or understand it.

If you want the whole hardware/App/Bridge/firmware/evidence picture before the tour, open the [System Map](SystemMap.md).

If you already know you want a longer role-based reading path, skip ahead to [Guided Routes](GuidedRoutes.md).

## The elevator version

MOARkNOBS-42 is built around four layers:

```mermaid
flowchart TD
  H[Hardware layer<br>buttons, pots, LEDs, envelope inputs] --> F[Firmware layer<br>scheduler, MIDI, config, persistence]
  F --> C[Contract layer<br>manifest, schema, WebSerial, bridge RPC]
  C --> U[User layer<br>browser app, OSC bridge, performer workflow]
```

If one layer lies to the next, the whole experience gets weird fast. That is why the docs keep returning to contract sync, test coverage, authoritative readback, and traceability.

## What you are looking at

### On the board

![Render of the MOARkNOBS board showing its control layout and main supporting components.](../sketch/MOAR_BOARD.png)

- a Teensy 4.0 running the firmware
- a button grid and control buttons for direct interaction
- LED output for visual feedback
- envelope follower inputs for audio-reactive control
- generation-checked persistent storage so the machine remembers verified state

### In the browser

![Annotated browser configurator screenshot showing live slots, the selected-slot editor, the utility rail, and the staged-diff panel.](../assets/ui/configurator-workbench-annotated.png)

- a runtime that connects over WebSerial
- a schema-driven form system
- staged edits that can be reviewed before they are applied
- checksum-backed Apply with explicit uncertainty and authoritative readback

## The first mental model to keep

The browser does **not** own the truth by itself, and the firmware does **not** blindly trust the browser.

The normal conversation is:

1. connect to the device
2. ask for its manifest
3. compare schema versions
4. fetch the config
5. stage changes locally
6. apply changes and wait for a verified receipt/readback outcome

That is why the configurator can safely support staging, diff views, local draft discard, uncertainty recovery, and migration warnings.

## The first practical walk

If you want the shortest role-based route, use [First Five Minutes](FirstFiveMinutes.md). If you want the learning tour, follow this order:

### 1. Learn the high-level workflow

Read [Process Overview](../project/ProcessOverview.md).

That page tells you the sequence from hardware prep to release without burying you in implementation details.

### 2. Learn how to recover from problems

Read [Troubleshooting](../validation/Troubleshooting.md).

The best time to understand failure modes is before the board is half-built and silent.

### 3. Learn the browser model

Read [Configurator Tour](../guides/Configurator.md), then [WebSerial Walkthrough](../guides/ProtocolWalkthrough.md).

If you learn best from concrete examples rather than empty forms, read [Preset Library](../guides/PresetLibrary.md) after that. It explains what each shipped preset is actually for and why the picker exists.

If you want the vocabulary before the examples, read [Glossary](../reference/Glossary.md).

If you want the reactive-control concepts broken out into small explainers instead of one big system page, follow [Reactive Control Guide](../guides/ReactiveControlGuide.md) with [ARG Guide](../guides/ARGGuide.md), [Filter Feel Guide](../guides/FilterFeelGuide.md), and [LFO Route Guide](../guides/LfoRouteGuide.md).

Those pages explain how the browser thinks about staged state, manifests, patches, and apply safety.

### 4. Learn what the tests actually prove

Read [Testing Story](../validation/TestingStory.md), then [Testing](../validation/TESTING.md).

If your real question is "how do I get one playable setup tonight?" jump to [Musician-First Guide](MusicianFirstGuide.md).

That page is where the repo stops being mystical about "tested" versus "not yet proven on real hardware."

## What to ignore on your first pass

You do **not** need to read these immediately unless you are debugging or extending the stack:

- [Legacy EEPROM Layout — Schema 4](../reference/EEPROMLayout.md) — historical only
- [Pin Map](../reference/PinMap.md)
- [Assumption Ledger](../reference/assumption-ledger.md)

They matter, but they are better after you already know the main path through the instrument.

## When you are ready for the deeper story

Once the basic workflow makes sense, read [History](../project/HISTORY.md).

That is where the project starts to become legible as more than a collection of files. You can see when test coverage expanded, when the browser became schema-driven, when release hardening started to matter, and why the project now reads so defensively in places.
