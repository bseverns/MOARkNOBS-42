# Hardware Substitutions

This page is the docs-site version of the conservative substitutions guide.

The repo-root canonical source remains `hardware/Substitutions.md`. This page mirrors the same support stance so the docs site can link to it cleanly.

## How to read this page

- If a substitute is not documented with repo evidence, it is treated as unverified.
- `UNVERIFIED — needs bench validation` means exactly that.
- Do not infer electrical equivalence from package similarity or vendor-family naming alone.

## Substitution summary

| Function | Current part | Known substitute | Verification status | Notes |
| --- | --- | --- | --- | --- |
| MCU / USB MIDI host brain | Teensy 4.0 | No repo-documented direct substitute | No substitute documented in repo | Firmware and board docs assume Teensy 4.0 specifically. |
| Button matrix analog mux | CD74HC4067 | No repo-documented direct substitute | No substitute documented in repo | Treat family swaps as risky until bench-validated. |
| Logic translation | SN74HCT245 | No repo-documented direct substitute | No substitute documented in repo | Direction, thresholds, and timing need validation. |
| Envelope follower op-amp stage | MCP6002 | `UNVERIFIED — needs bench validation` | Unverified | Supply range and envelope behavior need bench confirmation. |
| MIDI IN isolation | 6N138 | `UNVERIFIED — needs bench validation` | Unverified | No validated alternate optocoupler is documented in repo evidence. |
| Addressable LEDs | WS2812-style LEDs | `UNVERIFIED — needs bench validation` | Unverified | The repo documents a family, not a validated substitute SKU list. |
| OLED display | SSD1306 OLED | `UNVERIFIED — needs bench validation` | Unverified | No validated alternate module list is documented in repo evidence. |
| Power protection | 0.5 A / 2.5 A PTCs | `UNVERIFIED — needs bench validation` | Unverified | Hold current, trip current, and footprint must match the design intent. |

## Related pages

- [Hardware Current Build](HardwareCurrentBuild.md)
- [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
