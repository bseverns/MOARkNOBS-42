# System Flow

> The machine sliced into chunks small enough to explain before the soldering iron gets cold.

This folder is the bridge between “cool controller” and “I know what every signal is doing.” It keeps the hardware tour close to the firmware story without making learners read every source file first.

## Where to go

- [`hw/`](hw/) — physical subsystems: button matrix, display, envelope front end, MIDI, power, Teensy headers.

## Good workshop order

1. Power first. Nothing else gets to be interesting until the rails behave.
2. Inputs next: buttons, pots, envelope followers.
3. Brain after that: Teensy headers and firmware pin assumptions.
4. Outputs last: LEDs, display, MIDI.

That order matches real debugging: feed it, sense it, think it, scream it.
