# WebSerial Configuration App

`benzknobz.html` is a very small HTML page used to edit the MOARkNOBS controller configuration over USB. It relies on the Web Serial API so you need Chrome or Edge. The latest schema exposes brightness and colour settings for the new EF meters, control beacon and pot halos.

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

### LED swagger

Want the board to glow like a radioactive jellyfish or chill in dim doom? `config_schema.json` now exposes an `led` group with
`brightness` (0-255) and `color` (hex). Drop a snippet like this into your config and the halos obey:

```json
{
  "led": {
    "brightness": 64,
    "color": "#00FF00"
  }
}
```

This example mellows the LEDs to quarter power and paints them acid green.

For the gritty details of the serial stream, peep the [WebSerial protocol doc](../../docs/WebSerial.md).

*(Screenshot generation was not possible in this environment.)*
