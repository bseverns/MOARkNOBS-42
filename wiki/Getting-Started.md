# Getting Started

This is the finished-instrument path from cable to one saved, playable setup.
You do not need PlatformIO, Python, or the firmware source to follow it.
Canonical source: `docs/getting-started/QuickstartForPerformers.md`

## 1. Connect safely

1. Use a known-good USB **data** cable, not a charge-only cable.
2. Connect the instrument through its normal documented USB/power arrangement.
3. Wait for the Teensy USB device to appear before opening another serial tool.
4. Do not open the browser configurator and Bridge against the same serial port
   at the same time.

If the instrument does not enumerate, go directly to
[Connect did nothing](Troubleshooting.md#connect-did-nothing).

## 2. Choose the simpler connection

| Your goal | Open this | Connection |
| --- | --- | --- |
| Configure one instrument, inspect it, or manage profiles | Browser configurator supplied with the instrument | Direct WebSerial in a Chromium-based browser |
| Use OSC, a DAW-facing MIDI route, or App-over-Bridge | MN42 Bridge console | Bridge session |

Start with direct WebSerial unless OSC or DAW routing is part of tonight's
session. If you received the instrument with a hosted configurator URL, open
that. Running the App or Bridge from a source checkout belongs in
[Developer Setup](Developer-Setup.md).

If neither a configurator URL nor an MN42 Bridge application was supplied, the
current hardware-test package does not yet provide a zero-setup end-user
launcher; use Developer Setup or ask the instrument builder for the matching
App/Bridge artifact.

## 3. Confirm recognition

In the configurator:

1. click **Connect**
2. choose the MOARkNOBS-42/Teensy serial device
3. wait for the banner to show the device identity and firmware
4. confirm the transport/contract indicator is verified

In the Bridge console, wait until **Bridge**, **Serial**, and **Device** all
report ready, then choose **Open configurator**.

![Annotated configurator overview identifying the transport and contract banner, connection controls, profile workspace, Apply and Rollback actions, and recovery status.](assets/ui/configurator-top-annotated.png)

## 4. Load one known starting point

For the shortest first pass:

1. switch the configurator to **Basic** or **Advanced**
2. choose `DEMO_A - Reactive Stack` from the preset picker
3. confirm the staged-diff area becomes visible
4. click **Apply**

For an exact DAW/synth exercise with a downloadable JSON configuration, use
[First Playable Walkthrough](Playable-Walkthrough.md).

## 5. Move, listen, and look

A successful first checkpoint has three kinds of evidence:

- moving an enabled control changes its slot value and emits MIDI
- a receiving synth or DAW reacts to the mapped message
- the slot/LED and configurator telemetry move with the control or envelope

If the browser changes but the synth does not, use
[MIDI is not reaching the DAW](Troubleshooting.md#midi-is-not-reaching-the-daw).

## 6. Interpret Apply honestly

- **Synced** or verified readback: the captured configuration became device
  truth.
- **No device write needed**: the normalized device state was already the same.
- **Applied captured configuration / newer edits remain staged**: the captured
  version applied, but later edits are still local.
- **Apply rejected before transmission**: correct or refresh, then retry; the
  device was not written by that attempt.
- **Apply outcome uncertain**: stop editing and let authoritative readback
  finish. Do not assume success or rollback.
- **Device differs**: device truth is known, but it does not match the attempted
  candidate; the draft remains available.

## 7. Save it

After the device is verified and the staged diff is clean:

1. choose profile `A`, `B`, `C`, or `D`
2. click **Save profile**
3. use **Download profile/configuration** for an external JSON backup
4. switch away and back once to prove recall before relying on it live

Profiles use the current generation-backed LittleFS persistence system, not the
historical EEPROM layout.

## 8. Learn one recovery move

The panic-safe hardware combo is `Ctrl0 + Ctrl1 + Ctrl2`. It stops the arp,
disables EF follow, and reloads the active profile baseline.

Keep [Troubleshooting](Troubleshooting.md) open during the first session. It is
organized by the symptom you see, not by repository subsystem.

## Next useful pages

- [First Playable Walkthrough](Playable-Walkthrough.md)
- [Configure in Browser](WebSerial-App.md)
- [Connect to DAW / OSC](OSC-Bridge.md)
- [Save and recall profiles](WebSerial-App.md#save-recall-and-back-up)
- [Developer Setup](Developer-Setup.md)
