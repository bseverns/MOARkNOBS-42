# ButtonManager

Part of the firmware `include` jungle. The [include README](../README.md) explains how button rage propagates; the [main firmware README](../../README.md) zooms out to the whole machine.

Scans the 7×6 button grid, smacks bounce in the teeth, and spits out events.

## Where it fits

ButtonManager rides the multiplexed button grid, feeds MIDIHandler and flashes clues through LEDManager. ConfigManager keeps the map of what each press actually means.

```
[Muxed buttons] --> ButtonManager --> MIDIHandler
                                \-> LEDManager
```

See the big picture in the [main firmware README](../../README.md).

## Key Methods

- `initButtons()` – wire up mux pins and ready the machines.
- `processButtons(ctx)` – poll the matrix and trigger actions.
- `isMuxButtonPressed(idx)` – peek a raw button for tests.

## Typical Use

```cpp
#include "ButtonManager.h"

ButtonManager buttons(hwConfig, CONTROL_PINS, &pots);

void setup() {
  buttons.initButtons();
}

void loop() {
  buttons.processButtons(ctx);
}
```

That `hwConfig` bundle wrangles mux pins, LED counts, and timing so the tests and firmware slam in sync.

Dig deeper in [ButtonManager.h](../ButtonManager.h).

## Button Map

### Control Buttons

Need the cheat sheet for the six front-panel punks? Here it is.

*Long-press stunts ask for a quick confirm tap after you let go—no more accidental nukes.*

| Button | Short Press | Long Press | Double Press |
| ------ | ----------- | ---------- | ------------ |
| Ctrl0 | Toggle EF | Calibrate EF baseline | Cycle EF filter forward |
| Ctrl1 | Next Slot | — | Cycle EF filter backward |
| Ctrl2 | Cycle EF assignment | Toggle Slot Active | Cycle MIDI Type |
| Ctrl3 | Cycle MIDI Channel | Reset EEPROM | — |
| Ctrl4 | Cycle CC Number | Save config | Reload profile from EEPROM |
| Ctrl5 | Tap BPM | — | — |

### Slot Buttons

| Move | Action |
| --- | --- |
| Short press | Select the slot |
| Long press + confirm | Assign/cycle an Envelope Follower and flip it on |

### Combo Moves

| Combo | What happens |
| ----- | ------------- |
| Ctrl0 + Ctrl1 | Cycle EF ARG mode method |
| Ctrl2 + Ctrl3 | Cycle LED display modes |
| Ctrl4 + Ctrl5 | Enable EF and randomize settings |
| Ctrl0 + Ctrl4 | Set slot to MIDI Note mode |
| Ctrl0 + Ctrl5 | Set slot to Program Change |
| Ctrl1 + Ctrl4 | Set slot to Aftertouch |
| Ctrl1 + Ctrl5 | Set slot to Pitch Bend |
| Ctrl2 + Ctrl4 | Set slot to NRPN |
| Ctrl0 + Ctrl3 | Set slot to SysEx |
| Ctrl1 + Ctrl2 | Toggle MIDI clock output |
| Ctrl2 + Ctrl5 | Cycle ARG envelope pair |
| Ctrl3 + Ctrl4 | Bump arpeggiator base note |
| Ctrl3 + Ctrl5 | Toggle Arpeggiator mode |
| Ctrl0 + Ctrl2 | Cycle configuration profiles |

For deeper madness see the [firmware README](../../README.md).
