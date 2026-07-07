# Parts & Rationale

> The silicon misfits that make this controller sing; this is the canonical stash for why each part earned a seat on the board.

For the current hardware file status, start with [CurrentBuild.md](CurrentBuild.md). For sourcing risk and conservative alternates, see [Substitutions.md](Substitutions.md).

## Teensy 4.0

600 MHz ARM core with native USB MIDI. SparkFun's [Teensy 4.0 Hookup Guide](https://learn.sparkfun.com/tutorials/teensy-40-hookup-guide) walks through pinout, power rails, and flashing without bricking.

## CD74HC4067 analog mux

Collapses forty‑two buttons into one ADC read. The [16‑Channel Mux Breakout Guide](https://learn.sparkfun.com/tutorials/16-channel-analogdigital-multiplexer-breakout-guide) shows how to fan-in a forest of switches and breadboard it before spinning copper.

## SN74HCT245 level shifter

Keeps the 5 V button grid and LED strip from punching the 3 V3 brain. SparkFun's [Logic Level Shifting 101](https://learn.sparkfun.com/tutorials/logic-level-shifting) explains the voltage‑translation sleight of hand.

## Power rails + fuses

Splitting the 5 V logic rail from the 5 V LED rail keeps noise off the MCU while the strip parties. Inline PTC fuses (0.5 A for logic, 2.5 A for LEDs) fail safe if you cross-wire or short a pixel. SparkFun's [Fuse Tutorial](https://learn.sparkfun.com/tutorials/fuses) is the quick refresher on why these little resettable bricks earn their keep.

## MCP6002 op-amp

Rectifies and smooths incoming audio for the envelope followers. SparkFun's [Op-Amp Basics](https://learn.sparkfun.com/tutorials/op-amps/all) covers precision rectifiers and bias tricks.

## 6N138 optocoupler

Gives MIDI IN its own electrical bubble. SparkFun's [MIDI Tutorial](https://learn.sparkfun.com/tutorials/midi-tutorial/all) breaks down current loops, DIN vs. TRS jacks, and why opto‑isolation matters.

## WS2812 LEDs

One data pin, a riot of color on 52 diodes. SparkFun's [WS2812 Breakout Hookup Guide](https://learn.sparkfun.com/tutorials/ws2812-breakout-hookup-guide) dives into timing and power decoupling so you don't brown‑out the strip.

Need swap-friendly alternates or board placement notes? Hop back to the [hardware README](README.md) for the condensed build sheet and layout lore, then check [Substitutions.md](Substitutions.md) before making part swaps.
