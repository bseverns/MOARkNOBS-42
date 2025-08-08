# WebSerial Config Philosophy

This browser toy isn't a "real" app – it's a demo wearing a sneer. WebSerial lets you poke the controller with nothing but a USB cable and a semi-modern browser. No drivers, no compilers, no excuses.

## Why it exists

- **Expose the guts.** Every knob, LED, and envelope follower can be tweaked live. The config app is a map, not a maze.
- **Teach the flow.** The schema is plain JSON so you can see how firmware settings translate to UI fields.
- **Stay loose.** It's all client-side. View source, edit, reload, repeat until the board does your bidding.

## Common moves

1. **Spin up a server** – `python3 -m http.server` from this folder is plenty.
2. **Open `benzknobz.html`** in Chrome or Edge and smash **Connect**.
3. **Tweak stuff** – change brightness, remap slots, or juggle filter settings. The form mirrors `config_schema.json`.
4. **Punch **Save**** – settings blast over serial and land in EEPROM.
5. **Watch the live feed** – slots and envelopes stream updates every 100 ms so you know the board heard you.

## Troubleshooting

- **No serial ports listed?** Chrome only exposes them over `http://localhost` or HTTPS.
- **"Network Error" on connect?** You told the browser "no" once. Flip Serial access back to **Allow** in site settings.
- **Data looks frozen** – reload the page or yank the cable. The Teensy forgets fast when the port dies.
- **Save does nothing** – check the console. Errors show up there faster than you can say "why isn't this working?"

For the gritty protocol docs hit up [the WebSerial spec](../../docs/WebSerial.md). For a walkthrough of the app itself see [README.md](README.md).
