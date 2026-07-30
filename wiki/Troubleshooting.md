# Troubleshooting

Start with the symptom you can observe. Do not diagnose the repository
architecture first.
Canonical source: `docs/validation/FailureFirst.md`

## Connect did nothing

1. Confirm the cable carries data.
2. Close other serial terminals, Bridge instances, and browser tabs that may
   own the port.
3. Use a Chromium-based browser for direct WebSerial.
4. Serve/open the configurator from an allowed secure context.
5. Reconnect USB, reload the page, and choose the Teensy device again.

If the Teensy never enumerates, move to the physical checks in
`docs/validation/Troubleshooting.md`.

## Apply outcome uncertain

1. Stop applying or discarding drafts.
2. Keep the instrument connected.
3. Wait for authoritative readback/resynchronization.
4. If readback reports **Device differs**, treat the shown live state as device
   truth and the staged state as an unapplied draft.
5. Export the staged draft before reconnecting if it matters.

An uncertain result does not prove that firmware rolled back.

## Apply rejected before transmission

- Validation failure: correct the staged fields.
- Stale session revision: refresh the Bridge session and retry.
- Apply already in progress: wait for the active transaction to finish.
- Device not ready: restore a ready connection before retrying.

Device authority remains verified because this attempt did not reach the serial
Apply writer.

## Bridge cannot find the serial port

1. Disconnect direct WebSerial and close serial terminals.
2. Refresh detected ports in the Bridge console.
3. Select or type the actual Teensy serial path.
4. Start the Bridge and wait for **Serial** and **Device** to become ready.
5. If the port appeared after the DAW opened, restart the DAW after Bridge MIDI
   is available.

## MIDI is not reaching the DAW

1. Confirm the slot value moves in the configurator or Bridge telemetry.
2. Confirm the Bridge is running if the DAW expects `MN42 Bridge`.
3. Enable the correct MIDI input and channel on the receiving track.
4. Use a MIDI monitor to confirm the expected CC/note leaves the instrument.
5. Verify the destination parameter is actually MIDI-learned or mapped.

The current repository documents DAW routing but does not claim universal
DAW-by-DAW verification.

## OSC host receives nothing

1. Confirm Bridge send host/port matches the OSC listener.
2. Confirm the listener binds the intended interface.
3. Watch `/mn42/slots` or `/mn42/envelopes`.
4. Verify local firewall rules and avoid using the same UDP port for an
   incompatible listener.

## Envelope follower does not react

1. Confirm the source is connected to the intended EF input.
2. Watch envelope telemetry before changing mappings.
3. Reduce to one follower with `LINEAR` or `LOWPASS`.
4. Disable ARG temporarily.
5. Confirm the target slot is active and the receiving synth/DAW is mapped.

## Connection was lost

1. Stop moving controls until the link returns.
2. Reconnect through the same transport you started with.
3. Let device truth hydrate before editing.
4. Confirm whether a staged draft remains dirty.
5. Use `Ctrl0 + Ctrl1 + Ctrl2` if the performance state itself needs a known
   baseline.

## Deeper references

- `docs/validation/FailureFirst.md`
- `docs/validation/Troubleshooting.md`
- `docs/reference/ConfigurationTransactionModel.md`
- `docs/guides/BridgeForPerformers.md`
- [First Playable Walkthrough](Playable-Walkthrough.md)
