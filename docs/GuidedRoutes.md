# Guided Routes

This site now has enough material that a new reader can get lost just by having too many good options. This page fixes that by giving three intentional routes through the docs depending on who you are and what you want from the rig.

Pick one lane, finish it, then come back for the others.

## Builder route

**Best for:** someone assembling, flashing, and proving the hardware.

**Time:** about 45-60 minutes for the first read-through, not counting soldering or bench work.

**Outcome:** you understand how to bring the board up in a sane order and what needs real hardware proof.

1. [New User Story](StartHere.md)
2. [Hardware Bring-Up Story](HardwareStory.md)
3. [Builder's Handbook](BuildersHandbook.md)
4. [Testing Story](TestingStory.md)
5. [Troubleshooting](Troubleshooting.md)
6. [Failure-First Guide](FailureFirst.md)

What this route teaches:

- how the hardware comes alive in stages
- what to flash and test first
- what "working" means on a real board
- how to recover when bring-up goes sideways

## Learner route

**Best for:** someone trying to understand the architecture, vocabulary, and contract without immediately performing on it.

**Time:** about 35-50 minutes.

**Outcome:** you can explain the firmware/app/browser model in plain language and know where the main concepts live.

1. [New User Story](StartHere.md)
2. [Glossary](Glossary.md)
3. [Firmware Architecture Story](FirmwareArchitecture.md)
4. [Configurator Tour](Configurator.md)
5. [WebSerial Walkthrough](ProtocolWalkthrough.md)
6. [Reactive Control Guide](ReactiveControlGuide.md)
7. [ARG Guide](ARGGuide.md)
8. [Filter Feel Guide](FilterFeelGuide.md)
9. [LFO Route Guide](LfoRouteGuide.md)
10. [Preset Library](PresetLibrary.md)

What this route teaches:

- the shared vocabulary of the stack
- staged versus live state
- where presets, profiles, filters, EF, ARG, and LFO routes fit
- why the browser behaves more carefully than a toy editor

## Musician route

**Best for:** someone who wants a playable rig fast, without pretending they need to read every firmware page first.

**Time:** about 20-30 minutes.

**Outcome:** you can connect the deck, choose a preset or profile intentionally, and recover from common live-state mistakes.

1. [Configurator Tour](Configurator.md)
2. [Musician-First Guide](MusicianFirstGuide.md)
3. [Preset Library](PresetLibrary.md)
4. [Profile Workflow](ProfileWorkflow.md)
5. [Combo Guide](ComboGuide.md)
6. [Bridge for Performers](BridgeForPerformers.md)
7. [Failure-First Guide](FailureFirst.md)

What this route teaches:

- which starting configuration to choose
- when to stage, apply, save, load, or reset
- when to use the bridge instead of the browser
- what the most likely pre-show failures look like

## If you are teaching someone else

Use this mixed route:

1. [New User Story](StartHere.md)
2. [Preset Library](PresetLibrary.md)
3. [Profile Workflow](ProfileWorkflow.md)
4. [Reactive Control Guide](ReactiveControlGuide.md)
5. [ARG Guide](ARGGuide.md)
6. [Filter Feel Guide](FilterFeelGuide.md)
7. [Combo Guide](ComboGuide.md)
8. [Testing Story](TestingStory.md)

That path works well for workshops because it starts with concrete examples, then introduces the deeper system model only after the learner has something tactile to attach it to.

## Read next

- [Glossary](Glossary.md) if any term in these routes feels too insider-heavy
- [Docs Guide](DocsGuide.md) if you want the full map instead of a guided lane
