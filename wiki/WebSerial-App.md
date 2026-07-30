# WebSerial App

The browser configurator lives in `App/` and provides schema-validated editing against the firmware protocol.
Canonical source: `App/README.md`

![Workflow graphic showing the WebSerial app connect, manifest, config, stage, then Apply confirmation or resynchronization.](assets/workflows/webserial-workflow-overview.png)

## Architecture

- `App/runtime.js` - transport, schema validation, staging/diff/rollback, simulator
- `App/views/benzknobz.js` - UI rendering + event wiring
- `App/config_schema.json` - configuration schema contract
- `App/views/form_renderer.js` - schema-driven editor controls

![Annotated top of the configurator showing transport quality, connection actions, profile controls, Apply and Rollback, recovery status, and power information.](assets/ui/configurator-top-annotated.png)

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
6. Confirm the receipt and device readback; ambiguous outcomes enter resynchronization.

For a finished instrument, start with [Getting Started](Getting-Started.md) or
the [First Playable Walkthrough](Playable-Walkthrough.md) before reading the
runtime architecture below.

![Annotated configurator workbench showing the 42 live slots, selected-slot editor, Basic and Advanced mode switch, utility rail, and staged diff.](assets/ui/configurator-workbench-annotated.png)

## User-facing capabilities

- Basic/Advanced modes
- Profile load/save/reset flow
- Staged diff visibility
- MIDI monitor and optional clock output
- Simulator transport for hardware-free testing

## Save, recall, and back up

1. Apply the staged configuration and wait until device authority is verified.
2. Save the clean live configuration into profile A-D.
3. Load a profile to replace live state with that saved device state.
4. Download a JSON copy when the setup also needs an off-device backup.

Current profiles are stored through generation-backed LittleFS persistence.
Preset selection is browser-side staging; it is not a saved device profile
until Apply and profile save both complete.

## If Apply or connection fails

Do not interpret a missing receipt as rollback. Follow the status shown by the
App, then use [Troubleshooting by Symptom](Troubleshooting.md).

![Annotated slot tile showing its selected state, slot number, MIDI type, persistent-control badge, and immediate-mode badge.](assets/ui/slot-tile-annotated.png)

## Tests

```bash
npm --prefix App test
```

## Reference docs

- `App/README.md`
- `docs/guides/WebSerial.md`
- `docs/reference/ConfigurationTransactionModel.md`
- `docs/reference/PersistenceContract.md`
