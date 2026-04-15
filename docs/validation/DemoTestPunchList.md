# Demo Test Punch List

Use this when a prototype arrives, before a customer-facing demo, or before you tell anyone the rig is ready for show-day use.

This is intentionally practical. Mark each line as:

- `PASS` if it worked as expected
- `BLOCKED` if the demo should stop until fixed
- `FOLLOW-UP` if the demo can continue but the issue needs a tracked note

## 1. Preflight

- Confirm the correct firmware is loaded: `teensy40_main`
- Confirm you have one known-good USB data cable
- Confirm you have the browser configurator path ready
- Confirm you have the bridge path ready if OSC or DAW validation is part of the demo
- Confirm the intended demo profiles or presets are identified before power-up

## 2. Power and handshake

- Power the board over USB
- Confirm the board enumerates on the host machine
- Send `HELLO`
- Confirm the board responds with `{"hello":"mn42"}`
- Confirm the configurator or bridge shows the expected device identity

Pass criteria:

- No boot loop
- No repeated disconnect/reconnect cycle
- Handshake succeeds on the first normal attempt

## 3. Basic hardware behavior

- Confirm the button matrix responds across multiple areas of the panel
- Confirm the LEDs light and change state
- Confirm at least several pots/controls produce live value changes
- Confirm the OLED or status display behaves as expected if fitted in the build

Pass criteria:

- No dead control cluster
- No obviously stuck LED or frozen display state
- No control that causes immediate instability

## 4. Configurator path

- Open the configurator
- Connect successfully
- Confirm manifest/config data loads
- Stage one safe edit
- Apply the edit
- Save the current state to a profile and load it again on current firmware
- If those controls are disabled, treat that as stale firmware, failed manifest load, or offline state and export a backup file instead
- Export a backup if the demo depends on a known restore point

Pass criteria:

- Config loads without schema/manifest confusion
- Apply succeeds
- Device-backed save/load succeeds on the current firmware build
- Disabled profile controls read as stale/offline protection rather than a broken shipped feature
- Reloaded state matches what you just saved when the save/load path is part of the demo

## 5. Bridge path

Only run this section if the demo needs OSC, virtual MIDI, or the browser-driven bridge console.

- Start the bridge
- Confirm the bridge console opens
- Confirm the configurator opens through the bridge path
- Confirm slot/envelope telemetry is visible
- Confirm at least one inbound control path works:
  - OSC `/mn42/cmd`
  - virtual MIDI CC back into the board

Pass criteria:

- Bridge stays connected
- No obvious forwarding lag or reconnect loop
- At least one external control path is proven on the actual demo host

## 6. Profile and scene sanity

- Load the intended demo profile using the real path for this firmware:
  - browser device-backed profile controls on current firmware
  - hardware-side profile switching only when validating an older firmware build
- Switch to one other profile and back if that path is part of the demo contract
- Confirm the active profile story is understandable on screen and in behavior
- If scenes or macro snapshots are part of the demo, save and recall at least one of each on current firmware

Pass criteria:

- You can intentionally explain which profile is active
- Returning to the primary demo profile restores the expected baseline
- No disabled browser control looks like a broken shipped feature

## 7. Stress pass

- Leave the rig connected for at least 5 minutes
- During that window:
  - change slots
  - make one small staged/apply cycle
  - load and save a profile
  - create envelope activity or live input if the demo uses it
  - if applicable, run clocked behavior from the real host setup

Pass criteria:

- No UI freeze
- No stuck notes
- No repeated serial/bridge reconnect loop
- No obvious timing stall

## 8. Recovery behavior

- Unplug and reconnect USB once
- Reconnect the configurator or bridge path
- Trigger the panic/reset path if the demo plan depends on it
- Confirm the rig returns to a safe known state

Pass criteria:

- Recovery does not require guesswork
- The operator can explain the recovery path in one sentence

## 9. Evidence to capture

- Firmware build or git commit used for the demo
- Which profile slot is the primary demo state
- Which host/browser/DAW path was used
- Any `FOLLOW-UP` item discovered during the pass
- Short video or photo evidence if the issue is visual or timing-related

## 10. Demo sign-off rule

The demo is ready when all of these are true:

- power and handshake passed
- basic hardware behavior passed
- the intended control path passed
- the intended profile path passed
- the stress pass did not reveal a blocker
- the operator knows the recovery path

If any of those are not true, do not call the rig demo-ready. Use [Validation Flow](ValidationFlow.md) to decide whether the next step is re-test, bench fix, or documentation cleanup.
