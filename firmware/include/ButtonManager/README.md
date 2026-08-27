# ButtonManager

Part of the firmware `include` jungle. The [include README](../README.md) explains how button rage propagates; the [main firmware README](../../README.md) zooms out to the whole machine.

Turns stable states from the 7×6 button grid into gestures and instrument actions.

![Button matrix wiring diagram](../../../docs/sketch/ButtonMatrix.png)

## Where it fits

`ButtonScanner` owns the electrical boundary for the multiplexed button grid: mux addressing, settling, ADC/GPIO reads, debounce state, and stable physical states. `ButtonGestureInterpreter` is the Arduino-independent event machine for press/hold/release timing, exclusive double presses, long-press confirmation, chord settling, and chord consumption. `ButtonManager` consumes those semantic events, owns the command map, updates slot/profile state through ConfigManager, feeds MIDIHandler, and flashes clues through LEDManager.

```
[Muxed buttons] --> ButtonScanner --> ButtonGestureInterpreter --> ButtonManager --> MIDIHandler
                                                                        \-> LEDManager
```

See the big picture in the [main firmware README](../../README.md).

## Key Methods

- `ButtonScanner::initHardware()` – wire up mux pins and reset physical scan state.
- `ButtonGestureInterpreter::updateButton()` – translate one stable state into semantic button events.
- `ButtonGestureInterpreter::updateControlMask()` – settle and classify control-button chords.
- `ButtonManager::initButtons()` – initialize the scanner and ready the gesture machines.
- `ButtonManager::processButtons(ctx)` – poll the surface and dispatch interpreted events.
- `isMuxButtonPressed(idx)` – peek a raw button for tests; works on `const`
  managers too.

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

## Faster, non-blocking scans

`ButtonScanner` uses a bounded `micros()`/`yield()` settling loop whenever it changes the CD74HC4067 address. The main loop keeps breathing and the scanner still catches every click. The interpreter contains no GPIO, display, persistence, or runtime dependencies, so its gesture grammar can also run as a native host test.

Mux select lines get smashed via a pre-baked lookup table and `digitalWriteFast()`, ditching the bit‑twiddling on every pass. Flip `BUTTON_MANAGER_PROFILE` in the build and you'll get Serial spam with average and max scan times—handy to prove we're not dropping states while chasing speed.

Dig deeper in [ButtonManager.h](../ButtonManager.h).

## Button Map

### Control Buttons

Need the cheat sheet for the six front-panel punks? Here it is.

_Long-press stunts ask for a quick confirm tap after you let go—no more accidental nukes._

| Button | Short Press                          | Long Press                      | Double Press                                                                |
| ------ | ------------------------------------ | ------------------------------- | --------------------------------------------------------------------------- |
| Ctrl0  | Toggle EF                            | Calibrate EF baseline           | Cycle EF filter forward                                                     |
| Ctrl1  | Next Slot                            | Reload active profile (cycle diagnostic page while diagnostics are active) | Cycle EF filter backward |
| Ctrl2  | Cycle EF assignment                  | Toggle Slot Active              | Cycle MIDI type (CC→Note→PitchBend→ProgramChange→Aftertouch→NRPN→RPN→SysEx) |
| Ctrl3  | Cycle MIDI Channel                   | Reload persisted configuration | Cycle EF oversampling (1x/2x/4x/8x/16x/32x)                                |
| Ctrl4  | Cycle registry number (CC/NRPN/RPN)  | Save active profile/config      | Toggle the active slot's ARG combiner                                       |
| Ctrl5  | Tap BPM immediately (exit diagnostics if active) | Enter diagnostics / cycle pages | None |

Ctrl3 and Ctrl4 use exclusive double presses. Their normal short-press action waits for the 300 ms double-press window to close, so changing oversampling does not also change the MIDI channel and toggling ARG does not increment `data1`. Ctrl5 fires tap tempo immediately on release and has no double-press action, so rapid tempo taps remain tempo taps. A chord consumes its participating button releases, preventing combo gestures from leaking these solo actions.

The `Ctrl0+Ctrl1+Ctrl4` chord edits the active slot's fixed LFO 1 lane. A lane that has never been tuned starts in **Centered** mode at **100%** so enabling it immediately produces live modulation. Turning it off preserves mode and amount for the next enable. These live edits are saved to the slot and emitted as `slot_patch` updates so an attached configurator follows the hardware state.
When you arm the persisted-config reload (**Ctrl3**) or diagnostic toggle (**Ctrl5**) with a long press, the LED strip throws a full-strip warning animation. The red-and-white reload warning gives you time to cancel before unsaved runtime edits are replaced; the teal shimmer marks diagnostics.

### Slot Buttons

| Move                 | Action                                                                                                      |
| -------------------- | ----------------------------------------------------------------------------------------------------------- |
| Short press          | Select the slot                                                                                             |
| Long press + confirm | Assign/cycle an Envelope Follower and flip it on. After confirming, punch Control 0‑5 to pick a specific EF |

### Combo Moves

| Combo                 | What happens                     |
| --------------------- | -------------------------------- |
| Ctrl0 + Ctrl1 + Ctrl2 | Panic-safe baseline reset (stop arp, disable EF follow, reload active profile) |
| Ctrl0 + Ctrl1 + Ctrl3 | Toggle LFO quick-tune mode |
| Ctrl0 + Ctrl1 + Ctrl4 | Toggle live LFO 1 modulation for the active slot |
| Ctrl0 + Ctrl2 + Ctrl3 + Ctrl5 | Toggle on-device config mode |
| Ctrl3 + Ctrl4 + Ctrl5 | Toggle USB MIDI output           |
| Ctrl0 + Ctrl1         | Cycle EF ARG mode method         |
| Ctrl0 + Ctrl2         | Cycle ARG envelope pair          |
| Ctrl3 + Ctrl4         | Cycle LED display modes          |
| Ctrl0 + Ctrl4         | Enable EF and randomize settings |
| Ctrl4 + Ctrl5         | Set slot to MIDI Note mode       |
| Ctrl3 + Ctrl5         | Set slot to Program Change       |
| Ctrl0 + Ctrl5         | Set slot to Pitch Bend           |
| Ctrl1 + Ctrl4         | Set slot to Aftertouch           |
| Ctrl1 + Ctrl5         | Toggle MIDI clock output         |
| Ctrl1 + Ctrl4 + Ctrl5 | Toggle clock source (EXT/INT)    |
| Ctrl2 + Ctrl5         | Set slot to NRPN                 |
| Ctrl1 + Ctrl3         | Set slot to RPN                  |
| Ctrl0 + Ctrl3         | Set slot to SysEx                |
| Ctrl2 + Ctrl4         | Toggle Arpeggiator for a profile-assigned slot (short), Arp Edit (long press) |
| Ctrl2 + Ctrl3         | Bump arpeggiator base note (short), Swing preset (long press) |
| Ctrl1 + Ctrl2         | Cycle configuration profiles (A-D) |

The ARG combos (`Ctrl0+Ctrl1` / `Ctrl0+Ctrl2`) edit the active slot's ARG method or source pair and enable its ARG lane immediately. They do not require that slot to own a separate EF assignment.

Chord recognition is finger-roll safe: adding fingers to reach a larger combo
silently supersedes an unfired two-button special chord, and releasing a fired
larger combo does not reinterpret its remaining buttons as a new subset chord.
In particular, every press and release order for
`Ctrl0+Ctrl2+Ctrl3+Ctrl5` enters or exits config mode without triggering the
short `Ctrl2+Ctrl3` arpeggiator-note action.
The suppression ends at the all-controls-up boundary; fresh short, long, and
ordinary sub-combos re-arm immediately after that neutral state.

On-device config mode remaps control buttons while active:

| Control | Action |
| ------- | ------ |
| Ctrl0 / Ctrl1 | Previous / next slot |
| Ctrl2 | Cycle slot type |
| Ctrl3 | Cycle channel |
| Ctrl4 | Cycle data1 (CC/NRPN/RPN) |
| Ctrl5 | Exit; autosave the active profile if config-mode edits are dirty |

LFO quick-tune mode remaps control buttons while active:

| Control | Action |
| ------- | ------ |
| Ctrl0 / Ctrl1 | Select LFO 1 / LFO 2 |
| Ctrl2 | Cycle shape |
| Ctrl3 | Toggle sync |
| Ctrl4 | Cycle internal route target (EF Gain -> Arp Swing -> Vel Shift -> Note Chance -> Arp Gate -> Jitter Depth -> Jitter Smooth) |
| CtrlPot2 | Set sync ratio (sync ON) or bipolar/unipolar (sync OFF) |
| Ctrl5 | Exit LFO quick-tune mode |

For deeper madness see the [firmware README](../../README.md).
