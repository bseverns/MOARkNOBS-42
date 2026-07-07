# Hardware System Flow

> Seven tiny tours through the board, because one giant schematic is how learners quietly vanish.

Read these in debug order, not folder order:

1. [`power&protection.md`](power&protection.md) — rails, fuses, and “why did the LEDs eat my voltage?”
2. [`teensy&headers.md`](teensy&headers.md) — the brain, headers, and spare escape hatches.
3. [`buttonMatrix.md`](buttonMatrix.md) — 42 buttons through muxes without ghosting yourself into a corner.
4. [`envelopeFE.md`](envelopeFE.md) — audio/CV gets rectified, smoothed, and shoved into ADC land.
5. [`display.md`](display.md) — I²C pixels and the OLED bits learners can poke safely.
6. [`led&midiOut.md`](led&midiOut.md) — bright pixels plus MIDI OUT, aka timing gremlins in two flavors.
7. [`midiOpto.md`](midiOpto.md) — MIDI IN isolation, because other people's gear is electrically weird.

## Whiteboard trick

Trace one event: press a slot button, watch the matrix sense it, map it in firmware, then spit MIDI and LEDs back out. The files above give each stop a name so the code tour has a spine.
