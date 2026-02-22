# WebSerial App

The browser configurator lives in `App/` and provides schema-validated editing against the firmware protocol.

## Architecture

- `App/runtime.js` - transport, schema validation, staging/diff/rollback, simulator
- `App/views/benzknobz.js` - UI rendering + event wiring
- `App/config_schema.json` - configuration schema contract
- `App/views/form_renderer.js` - schema-driven editor controls

## Local run

```bash
python3 -m http.server -d App
```

Open `http://localhost:8000/`.

## Core workflow

1. Connect via WebSerial.
2. Read manifest/config from firmware.
3. Stage edits locally.
4. Validate against schema.
5. Apply and confirm checksum/ACK.
6. Roll back automatically on mismatch/failure.

## User-facing capabilities

- Basic/Advanced modes
- Profile load/save/reset flow
- Staged diff visibility
- MIDI monitor and optional clock output
- Simulator transport for hardware-free testing

## Tests

```bash
npm --prefix App test
```

## Reference docs

- `App/README.md`
- `docs/WebSerial.md`

