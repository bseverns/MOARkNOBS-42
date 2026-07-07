# Why These Specific Parts?

Here’s the long-form rant the landing page no longer has space for: every part on this board is picked because it either keeps the rig stable under demo abuse or makes the learning story cleaner. Grab the cheat-sheet vibe from the README, then come here when you want the receipts.

## Teensy 4.0: the loud brain

Teensy 4.0 gives us a 600 MHz core, USB MIDI baked in, and a dev community that actually debugs in public. It’s overkill on clock speed, which means we can waste cycles on clarity—more comments, more guard rails—without dropping frames on the LED strip or choking MIDI. If you want to contrast it with STM32 or RP2040 in a workshop, the code paths here make the trade-offs obvious.

## CD74HC4067 analog mux: button crowd control

Forty-two buttons would eat every pin on the board, so a 16-channel analog mux does the crowd control. It keeps the PCB routing sane, demonstrates address-line timing, and still leaves room to tack on extra sensors if you’re hacking live. Students see the mux logic in `ButtonManager` and immediately grok why scan timing matters.

## SN74HCT245 level shifter: peace between voltage islands

The button matrix and LEDs live at 5 V; the MCU chills at 3.3 V. A simple HCT245 keeps everyone polite without resorting to exotic bidirectional boards. It’s cheap, forgiving, and a perfect excuse to teach why voltage translation belongs near the noisy peripherals instead of in firmware hacks.

## Power rails + fuses: controlled chaos

Splitting the logic and LED rails keeps digital noise out of the audio and UI paths. PTC fuses (0.5 A for logic, 2.5 A for LEDs) add idiot-proofing so beginners can botch a solder joint without letting the magic smoke out. The layout leaves clear test points so you can probe sag and ripple when you want to nerd out about decoupling.

## MCP6002 op-amp: envelope honesty

The MCP6002 rectifies and smooths the incoming audio so the envelope follower doesn’t lie. It runs happily on the same 5 V rail as the rest of the front-end and tolerates the clumsy inputs you get during workshops. It’s also a great teaching moment for precision rectifiers without dragging in exotic op-amps.

## 6N138 optocoupler: MIDI quarantine

MIDI IN gets its own electrical bubble via a 6N138. It keeps laptop ground loops from turning into hum on the LEDs and doubles as a live demo of why opto-isolation exists. DIN and TRS wiring both behave once you follow the datasheet, so you can hot-swap cables mid-demo without fear.

## WS2812 LEDs: maximal spectacle

One data pin drives 52 diodes and the timing quirks make for excellent debugging theater. They’re noisy enough to justify the power rail isolation, and the DMA tricks in firmware show exactly how to feed them without starving MIDI. If you want calmer lights, swap the strip, but the WS2812 drama makes the lessons stick.

Need alternate part numbers, footprints, or sourcing links? The shopping list and board notes stay in the [hardware README](README.md) and the quick reference tables live in [hardware/Parts.md](Parts.md). Grab those when you’re ordering; come back here when you want to explain why this BOM earns its keep.
