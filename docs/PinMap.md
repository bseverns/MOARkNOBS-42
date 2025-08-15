# Pin Map

Here's the lowdown on how the Teensy 4.0 talks to the outside world. For the full schematic chaos, peep the [hardware docs](../hardware/README.md).

![Board Pin Trace](trace.png)

| MCU Pin | Function | Label | Component | Notes |
| ------- | -------- | ----- | --------- | ----- |
| 6 | WS2812 data | `LED_PIN` | LED strip | 330 Ω series resistor on the line |
| 23 | Status LED | `STATUS_LED_PIN` | On‑board indicator | Tied low until we scream |
| 7 | Row driver | `PIN_ROW_DRV` | Button matrix | Kicks a row high for scanning |
| 2 | MUX row select 0 | `MUXR0` | CD74HC4067 #1 | |
| 3 | MUX row select 1 | `MUXR1` | CD74HC4067 #1 | |
| 4 | MUX row select 2 | `MUXR2` | CD74HC4067 #1 | |
| 5 | MUX row select 3 | `MUXR3` | CD74HC4067 #1 | |
| 8 | MUX column select 0 | `MUXC0` | CD74HC4067 #2 | |
| 9 | MUX column select 1 | `MUXC1` | CD74HC4067 #2 | |
| 10 | MUX column select 2 | `MUXC2` | CD74HC4067 #2 | |
| 11 | MUX column select 3 | `MUXC3` | CD74HC4067 #2 | |
| A4 | Button read | `BTN_MUX_OUT` | CD74HC4067 #1 | Analog sense for matrix |
| A5 | Pot read | `POT_MUX_OUT` | CD74HC4067 #2 | Analog sense for pots |
| A8 | VREF sense | `VREF_ADC` | Voltage divider | Tracks analog reference |
| A0 | Envelope input 0 | `EF0` | Envelope follower | |
| A1 | Envelope input 1 | `EF1` | Envelope follower | |
| A2 | Envelope input 2 | `EF2` | Envelope follower | |
| A3 | Envelope input 3 | `EF3` | Envelope follower | |
| A6 | Envelope input 4 | `EF4` | Envelope follower | |
| A7 | Envelope input 5 | `EF5` | Envelope follower | |
| 12 | Control button 0 | `C0` | Front‑panel switch | Direct GPIO |
| 13 | Control button 1 | `C1` | Front‑panel switch | Direct GPIO |
| 14 | Control button 2 | `C2` | Front‑panel switch | Direct GPIO |
| 15 | Control button 3 | `C3` | Front‑panel switch | Direct GPIO |
| 24 | Control button 4 | `C4` | Front‑panel switch | Direct GPIO |
| 25 | Control button 5 | `C5` | Front‑panel switch | Direct GPIO |
| 18 | I²C SDA | `SDA` | SSD1306 OLED | |
| 19 | I²C SCL | `SCL` | SSD1306 OLED | |
| 1 | MIDI out | `MIDI_TX` | DIN jack via AHCT245 | 220 Ω resistor on the line |
| 0 | MIDI in | `MIDI_RX` | DIN jack | optocoupler assembly |

If a pin isn't listed here, it's either unused or riding shotgun for future hacks.
