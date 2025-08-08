# WebSerial Configuration App

`benzknobz.html` is a very small HTML page used to edit the MOARkNOBS controller configuration over USB. It relies on the Web Serial API so you need Chrome or Edge. The latest schema now covers **ARG method selection**, **envelope‑follower slot assignments**, and **per‑LED colour tweaks** alongside brightness for the EF meters, control beacon and pot halos.

The page reads a JSON schema and the current settings from the board, builds a form and then lets you push changes back.

For an overview of the entire project see the [repo README](../../README.md).

## Contents

- `benzknobz.html` – barebones WebSerial editor.
- `config_schema.json` – describes every field the app can twiddle.

## Usage

1. Flash the firmware and connect the device via USB.
2. From this directory run a quick server:
   ```bash
   python3 -m http.server
   ```
   then open <http://localhost:8000/benzknobz.html> in Chrome or Edge.
3. Click **Connect** and select the MOARkNOBS serial port.
4. Wait for the settings to load.
5. Tweak values in the form.
6. Press **Save** to write everything back to EEPROM.

The schema used to build the form lives in `config_schema.json` in this folder.

## What You Can Tweak

This little app lets you boss the board around without touching code. Dial in:

- **Brightness** – decide how blinding the EF meters blaze. Dim for stealth or crank it to stage-lamp levels.
- **Colours for every LED** – paint each halo or meter however you like. Psychedelic rainbows encouraged.
- **Slot mappings** – reshuffle which physical knob controls which slot. Break the factory order and claim your own layout.
- **Envelope routing** – point each envelope follower at a slot and pick an ARG method with your favourite A/B combo. Chaos becomes configurable.

### LED Colour Picker

Scroll past the filter and ARG blocks and you'll find a fresh **LED Colors** fieldset. Each of the 42 little squares is a color picker tied to a specific LED on the rig. Click one, pick your shade – from black-hole subtle to retina-searing neon – and it's baked into the board the moment you slam **Save**. Mix a rainbow or go full goth; the firmware now slurps these hex codes straight into its `SET_ALL` payload and lights the strip accordingly.

## Troubleshooting

If the browser throws a permission tantrum when you hit **Connect**:

- Chrome or Edge will only talk WebSerial over secure origins. Use `http://localhost` or host it with HTTPS.
- If the permission prompt ghosts you, click the little plug icon in the address bar and flip Serial to **Allow**.
- Said “no” once? The browser holds grudges. Clear the block in site settings, then reconnect.
- Still stuck? Yank the USB cable and plug it back in. Sometimes hardware needs a quick reality check.

For the gritty details of the serial stream, peep the [WebSerial protocol doc](../../docs/WebSerial.md).

*(Screenshot generation was not possible in this environment.)*
