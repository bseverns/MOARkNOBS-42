# MOARkNOBS-42 Power Budget Quick Reference

Use this page as a quick sanity check before high-current LED testing.

## LED Current Estimates (WS2812)

- worst-case full white: `52 x 60 mA ~= 3.12 A`
- single-channel primaries (red-only, green-only, blue-only): `52 x 20 mA ~= 1.04 A`

These values are LED-only estimates.

Teensy 4.0, OLED, muxes, MIDI circuitry, and envelope follower circuits add additional current draw on top of LED load.

## Bench Supply Recommendation

- regulated `5 V / 4 A` minimum
- regulated `5 V / 5 A` preferred for white-load and burn-in testing

## Critical Reminder

Firmware brightness limits reduce risk, but they do not replace correct hardware power topology and fuse placement.
