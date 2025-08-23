# Pin Map

Here's the lowdown on how the Teensy 4.0 talks to the outside world. For the full schematic chaos, peep the [hardware docs](../hardware/README.md).

![Board Pin Trace](trace.png)

Pins below are sorted by the Teensy's digital numbering. If you see "Analog read" in the notes, we're hitting that pin with `analogRead()` even though it's labeled by its digital ID.

| MCU Pin | Function | Label | Component | Notes |
| ------- | -------- | ----- | --------- | ----- |
| 0 | MIDI in | `MIDI_RX` | DIN & TRS Type‑A jacks | optocoupler assembly |
| 1 | MIDI out | `MIDI_TX` | DIN & TRS Type‑A jacks via AHCT245 | 220 Ω resistor on the line |
| 2 | MUX row select 1 | `MUXR1` | CD74HC4067 row mux | |
| 3 | MUX row select 2 | `MUXR2` | CD74HC4067 row mux | |
| 4 | MUX row select 3 | `MUXR3` | CD74HC4067 row mux | |
| 5 | MUX row select 4 | `MUXR4` | CD74HC4067 row mux | |
| 6 | WS2812 data | `LED_PIN` | LED strip | 330 Ω series resistor on the line |
| 7 | Column MUX read | `MUXC_IN` | CD74HC4067 column mux | Analog read |
| 8 | MUX column select 1 | `MUXC1` | CD74HC4067 column mux | |
| 9 | MUX column select 2 | `MUXC2` | CD74HC4067 column mux | |
| 10 | MUX column select 3 | `MUXC3` | CD74HC4067 column mux | |
| 11 | MUX column select 4 | `MUXC4` | CD74HC4067 column mux | |
| 12 | Not connected | — | — | leave it hanging |
| 13 | Not connected | — | — | leave it hanging |
| 14 | Envelope input 1 | `EF1` | Envelope follower | Analog read (A0) |
| 15 | Envelope input 2 | `EF2` | Envelope follower | Analog read (A1) |
| 16 | Envelope input 3 | `EF3` | Envelope follower | Analog read (A2) |
| 17 | Envelope input 4 | `EF4` | Envelope follower | Analog read (A3) |
| 18 | I²C SDA | `SDA` | SSD1306 OLED | |
| 19 | I²C SCL | `SCL` | SSD1306 OLED | |
| 20 | Envelope input 5 | `EF5` | Envelope follower | Analog read (A6) |
| 21 | Envelope input 6 | `EF6` | Envelope follower | Analog read (A7) |
| 22 | Row MUX read | `MUXR_IN` | CD74HC4067 row mux | Analog read (A8) |
| 23 | Power status LED | `STATUS_LED_PIN` | On‑board indicator | Tied low until we scream |

If a pin isn't listed here, it's either unused or riding shotgun for future hacks.
