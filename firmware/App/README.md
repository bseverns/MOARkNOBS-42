# WebSerial Configuration App

`benzknobz.html` is a very small HTML page used to edit the MOARkNOBS controller configuration over USB. It relies on the Web Serial API so you need Chrome or Edge. The latest schema exposes brightness and colour settings for the new EF meters, control beacon and pot halos.

The page reads a JSON schema and the current settings from the board, builds a form and then lets you push changes back.

For an overview of the entire project see the [repo README](../../README.md).

## Usage

1. Flash the firmware and connect the device via USB.
2. Open `benzknobz.html` in Chrome or Edge.
3. Click **Connect** and select the MOARkNOBS serial port.
4. Wait for the settings to load.
5. Tweak values in the form.
6. Press **Save** to write everything back to EEPROM.

The schema used to build the form lives in `config_schema.json` in this folder.

*(Screenshot generation was not possible in this environment.)*
