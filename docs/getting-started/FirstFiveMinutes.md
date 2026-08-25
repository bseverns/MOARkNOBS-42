# First Five Minutes

> **Doc class:** Orientation. This page helps you choose a first path. It does not replace contract or evidence docs. Current safe repo claim: hardware-test package.

Pick the sentence that sounds most like you. Do that path first. Ignore the rest until you have momentum.

## Explore Without Hardware

**Who this is for:** curious visitors who want to understand the configurator before finding or building an instrument.

1. Open the [hosted configurator](https://bseverns.github.io/MN42/).
2. Switch to **Lab**, then select **Start simulator** instead of connecting a serial device.
3. Change one slot, inspect the staged diff, and apply it to the simulated device.

The simulator proves the browser workflow, not physical controls, timing, MIDI wiring, or electrical behavior.

## Play It

**Who this is for:** performers who want one playable setup before reading the whole repo.

**What you need:** an MN42 board, a USB data cable, a supported browser path, and the audio/MIDI/OSC gear you actually plan to use.

**Steps:**

1. Plug the board in over USB and confirm it enumerates.
2. Open the configurator in performer/stage mode.
3. Connect, check the device banner, and load or save a profile.
4. Use the Bridge only if you need OSC or a DAW-facing virtual MIDI port.
5. Before rehearsal or demo use, run the punch list for that exact rig.

**Primary links:**

- [Quickstart for Performers](QuickstartForPerformers.md)
- [Connectivity Guide](ConnectivityGuide.md)
- [Demo Test Punch List](../validation/DemoTestPunchList.md)

**Caveat:** a working bench setup is not a universal host/DAW/browser claim.

## Configure It

**Who this is for:** users who want to change mappings, profiles, scenes, or live setup without touching firmware.

**What you need:** a connected board, the browser configurator, and either direct WebSerial or the local Bridge path.

**Steps:**

1. Decide whether direct WebSerial is enough or whether you need the Bridge.
2. Connect and wait for manifest/config data before editing.
3. Use **Configure** for everyday slot mapping or **Lab** for exact bench controls.
4. Stage edits, review the diff, apply, and wait for confirmation.
5. Save the profile only after the device accepted the setup.

**Primary links:**

- [Configurator Tour](../guides/Configurator.md)
- [Profile Workflow](../guides/ProfileWorkflow.md)
- [Configure Without Recompiling](ConfigureWithoutRecompiling.md)

**Caveat:** the browser helps edit device state; it is not the source of truth by itself.

## Build/Test It

**Who this is for:** builders validating hardware, flashing firmware, or proving a board on the bench.

**What you need:** the current board/build, Teensy 4.0, PlatformIO, USB data cable, and a bench setup you trust.

**Steps:**

1. Check the current hardware status before ordering, soldering, or assuming fabrication files are ready.
2. Build or upload the main firmware from the repo root with `pio run -d firmware -e teensy40_main`.
3. Confirm the board answers `HELLO`.
4. Run the Unity lane with `pio test -d firmware -e teensy40_unity -vvv`.
5. Use hardware-test lanes and receipts for real-board claims.

**Primary links:**

- [Quickstart for Builders](QuickstartForBuilders.md)
- [Current Hardware Build](../reference/HardwareCurrentBuild.md)
- [TESTING](../validation/TESTING.md)

**Caveat:** this repo is not claiming public fabrication readiness or verified rail topology without dated evidence.

## Understand It

**Who this is for:** readers who want the mental model before operating or building anything.

**What you need:** no hardware; just enough patience to avoid reading every deep reference page first.

**Steps:**

1. Read the object card to learn what MN42 is and is not.
2. Open the system map to see hardware, firmware, App, Bridge, and evidence as one system.
3. Read who controls a slot so the reactive and takeover paths have a concrete shape.
4. Use the glossary only when terms get in the way.

**Primary links:**

- [Object Card](ObjectCard.md)
- [System Map](SystemMap.md)
- [Who Controls This Slot?](../learn/OneSignalPath.md)

**Caveat:** if pages disagree, use the [Documentation Truth Map](../reference/DocumentationTruthMap.md); history and planning pages do not define current behavior.
