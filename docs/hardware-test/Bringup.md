# Hardware Bring-Up

This checklist is for the current MOARkNOBS-42 prototype hardware-test package.

## 1. Power Inspection Before First Boot

Before applying power:

- inspect for shorts, reversed polarized parts, missing regulator parts, and solder bridges
- confirm Teensy 4.0 orientation and header seating
- confirm LED power injection and ground continuity
- confirm the OLED module is installed on the expected I2C header
- confirm the button matrix and potentiometer mux wiring are populated

Recommended pre-power measurements:

- continuity from ground to ground across board sections
- no hard short between `5V` and `GND`
- no hard short between `3V3` and `GND`

Fail the board immediately if any rail is shorted.

## 2. Teensy Detection And USB Serial Check

Build and upload the main firmware:

```bash
pio -d firmware run -e teensy40_main -t upload
```

Open a serial monitor at `115200` and confirm the board enumerates as a Teensy serial device.

Known-good serial banner examples:

```text
MN42 FW <version> <git-sha>
MN42 FW <version> schema <hex> UID <uid words>
Reset 0x<reset-cause> Brownouts <count>
```

Pass:

- the host sees a serial device
- the firmware prints the boot banner above

Fail:

- no serial device appears
- the device resets continuously
- no boot banner appears

## 3. OLED I2C Address Check, Expected `0x3C`

Upload the OLED and LED exerciser:

```bash
pio -d firmware run -e teensy40_display_led_hw -t upload
```

Expected behavior:

- the OLED initializes and shows the hardware-test intro screen
- pressing control button 0 advances phases
- no serial failure line appears

Known failure message:

```text
[HW TEST] OLED init failed at 0x3C
```

Pass:

- the OLED displays the intro and phase screens
- the expected address is `0x3C`

Fail:

- the failure line above appears
- the OLED stays blank while the rest of the board runs

## 4. LED Strip And Status LED Test

Use the same `teensy40_display_led_hw` lane first, then the burn-in lane if needed:

```bash
pio -d firmware run -e teensy40_power_burnin -t upload
```

Known-good serial lines from the burn-in lane look like:

```text
[BURN] uptime=<ms> phase=<name> ... vref=<volts> ... brownouts=<count> lock=<mode>
[CMD] phase <idle|white25|white50|white100|red|green|blue|sweep|wash|blast>
```

Pass:

- slot LEDs, EF LEDs, control LED, and pot halo LEDs all respond
- no unexpected resets or brownout spikes appear during `white100` or `blast`

Fail:

- missing LED regions
- corrupted colors
- resets or rising brownout count under load

## 5. Button Matrix And Mux Test

Upload the unified hardware test:

```bash
pio -d firmware run -e teensy40_unified_test -t upload
```

Expected serial prompts include:

```text
=== Button Test ===
Press V-Button #<n> now...
Detected slot <n> OK.
Press C-Button #<n> (pin <pin>)...
Detected control <n> OK.
```

Pass:

- all virtual slot buttons are detected in order
- all control buttons are detected in order

Fail:

- missing positions
- incorrect index mapping
- repeated or ghosted presses

## 6. Potentiometer Scan Test

Use `teensy40_unified_test`.

Expected operator flow:

- the sketch prompts for min and max positions
- the main pot and tuning pots report changing raw values when moved

Pass:

- each pot moves over a wide range with stable response
- no pot is stuck high or low

Fail:

- no change in raw value
- heavy jitter at rest beyond normal ADC noise
- wrong pot responds to the prompt

## 7. MIDI USB Test

For a minimal USB MIDI lane:

```bash
pio -d firmware run -e teensy40_button_ef_demo -t upload
```

Known-good startup lines:

```text
MOARkNOBS: 1-button/1-EF USB MIDI demo
Hold things quiet for a sec — calibrating baseline...
Baseline locked. Mash the button, feed the EF, and watch MIDI Monitor.
```

Pass:

- the host sees a USB MIDI device
- button presses emit note on/off
- the envelope input emits CC changes

Fail:

- no USB MIDI enumeration
- no note messages on button press
- no CC activity from the EF input

## 8. MIDI DIN Test

If the prototype hardware includes the DIN/TRS MIDI path, use the production firmware:

```bash
pio -d firmware run -e teensy40_main -t upload
```

Pass:

- MIDI activity appears on the downstream DIN/TRS-connected receiver when slots are exercised
- USB and board-side MIDI activity do not destabilize the device

Fail:

- no downstream MIDI output
- unstable board behavior when the DIN path is active

Known limitation:

- this package does not claim broad external-device compatibility testing across all MIDI receivers

## 9. Envelope Follower Baseline And Calibration Test

Use `teensy40_unified_test` for the integrated path and `teensy40_button_ef_demo` for the minimal path.

Pass:

- idle inputs settle near a stable baseline
- applied signal causes visible LED or telemetry response
- EF-driven MIDI changes track signal amplitude without obvious latch-up

Fail:

- follower is pinned high or low at idle
- large drift with quiet input
- no response to known-good signal injection

## 10. WebSerial Telemetry Test

Upload `teensy40_main`, then serve the configurator:

```bash
python3 -m http.server --directory App 8000
```

Open `http://localhost:8000/`, click **Connect**, and confirm the handshake and telemetry flow.

Known-good host/device flow:

```text
HELLO
{"hello":"mn42"}
GET_MANIFEST
GET_SCHEMA
GET_CONFIG
```

Pass:

- browser connects over WebSerial
- live values update while buttons, pots, or EF inputs move
- no repeated reconnect loop is required

Fail:

- `HELLO` does not receive `{"hello":"mn42"}`
- manifest or config fetch fails
- telemetry never updates

## 11. Bridge Connection Test

Upload `teensy40_full_system`, install bridge dependencies, then run:

```bash
npm --prefix bridge ci
node firmware/system_test/mn42_fullstack_runner.js --serial /dev/ttyACM0 --report logs/system-test.json
```

Known-good expectations:

- the bridge sends `HELLO`
- the board replies with `{"hello":"mn42"}`
- slot and envelope updates begin forwarding

Pass:

- the full-stack runner exits successfully
- the report file records a passing run

Fail:

- handshake timeout
- OSC or serial reconnect loop
- missing slot or envelope updates

## 12. Pass/Fail Table

| Area               | Pass condition                                         | Fail condition                                        |
| ------------------ | ------------------------------------------------------ | ----------------------------------------------------- |
| Power              | no rail short, stable power-on                         | rail short, hot parts, immediate brownout             |
| Teensy USB serial  | device enumerates and prints boot banner               | no serial device or no banner                         |
| OLED               | display initializes at `0x3C` and renders test screens | `[HW TEST] OLED init failed at 0x3C` or blank display |
| LEDs               | all LED groups respond and survive load test           | dead regions, corrupted colors, resets                |
| Buttons            | all slot and control buttons detect in order           | missing or ghosted presses                            |
| Pots               | each pot scans across expected range                   | stuck or cross-wired response                         |
| USB MIDI           | device enumerates and emits note/CC events             | no MIDI events                                        |
| DIN MIDI           | downstream device receives board-side MIDI             | no output or unstable behavior                        |
| Envelope followers | stable idle baseline and clear response to signal      | pinned or nonresponsive follower                      |
| WebSerial          | `HELLO` and live telemetry work in browser             | no handshake or no live updates                       |
| Bridge             | full-stack runner completes without timeout            | handshake or routing failure                          |
