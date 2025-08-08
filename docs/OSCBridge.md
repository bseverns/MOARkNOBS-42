# OSC/WebMIDI Bridge

When MIDI wiring feels like a straightjacket, the bridge lets the box speak OSC and WebMIDI so you can poke it from across the room or from a script instead of the front panel.

## Setup

1. `npm install` – pull in the Node bits.
2. `node bridge/mn42_bridge.js` – spins up the translator. It listens for OSC on `127.0.0.1:8000` and chats WebMIDI to the first MN42 it finds. `--help` shows options for ports and devices.

## OSC Address & MIDI Mapping

All addresses live under `/mn42` and map to regular MIDI messages:

- `/mn42/slot/<slot>/cc/<cc>` `<value>` → Control Change.
- `/mn42/slot/<slot>/note/<note>` `<velocity>` → Note On/Off.
- `/mn42/raw/<status>/<data1>/<data2>` → fire raw three‑byte MIDI at the board.

Slot and controller numbers start at zero because counting from one is for humans.

## Push Updates Back to the Hardware

Any OSC poke is immediately flung over WebMIDI. When you're ready to make it stick:

- Send `/mn42/persist` to burn the current state into EEPROM.
- Or run `node bridge/mn42_bridge.js --push my_patch.json` to blast a saved profile.

Ride the bridge, break the rules, and remember you can always yank the USB cable if things get weird.
ts. Hack, remix, and make the bridge scream your tune.
