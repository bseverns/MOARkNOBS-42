# Parts Substitutions

This page is a conservative sourcing guide for builders. It only records what can be stated without inventing electrical equivalence.

If a substitute is not documented in repo evidence, it is marked `UNVERIFIED — needs bench validation`.

## Substitution Table

| Function | Current part | Sourcing risk | Known substitute | Verification status | Notes |
| --- | --- | --- | --- | --- | --- |
| MCU / USB MIDI host brain | Teensy 4.0 | High | No repo-documented direct substitute | No substitute documented in repo | Board docs and firmware docs assume Teensy 4.0 specifically. |
| Button matrix analog mux | CD74HC4067 | Medium | No repo-documented direct substitute | No substitute documented in repo | Treat family swaps as risky until channel behavior and control logic are bench-validated. |
| 5 V to 3V3 logic translation | SN74HCT245 | Medium | No repo-documented direct substitute | No substitute documented in repo | Direction, thresholds, and timing should be treated as validation items, not assumptions. |
| Envelope follower op-amp stage | MCP6002 | Medium | `UNVERIFIED — needs bench validation` | Unverified | Any alternate op-amp must be checked for supply range, input behavior, and envelope response on the bench. |
| MIDI IN isolation | 6N138 | Medium | `UNVERIFIED — needs bench validation` | Unverified | The repo mentions the 6N138 path but does not document a validated alternate optocoupler. |
| Addressable indicator LEDs | WS2812-style LEDs | Medium | `UNVERIFIED — needs bench validation` | Unverified | The repo documents a WS2812-style family, not a single verified purchasing SKU or package variant. |
| OLED display | SSD1306 OLED | Medium | `UNVERIFIED — needs bench validation` | Unverified | The repo documents SSD1306 usage, but not a validated alternate module list. |
| MIDI connectors | 5-pin DIN plus 1/8 in TRS Type-A | Low | No repo-documented substitute list | No substitute documented in repo | Mechanical footprint and Type-A wiring should be treated as fit checks. |
| Power protection | 0.5 A logic PTC and 2.5 A LED PTC | Medium | `UNVERIFIED — needs bench validation` | Unverified | Hold current, trip current, and physical footprint should be confirmed before substitution. |

## How To Use This Page

1. Prefer the documented part when you can source it.
2. If you must substitute, treat `UNVERIFIED` literally and test the affected subsystem before a full build.
3. If you validate a substitute on real hardware, update this file with the exact part number and the test evidence that proved it.
