# Wiki Source Pack

This folder contains a ready-to-publish wiki for MOARkNOBS-42.

## Included pages

- `Home.md`
- `Getting-Started.md`
- `System-Architecture.md`
- `Firmware.md`
- `Hardware.md`
- `WebSerial-App.md`
- `OSC-Bridge.md`
- `Testing.md`
- `Release-Process.md`
- `History-and-Roadmap.md`
- `_Sidebar.md`

## Publish to GitHub Wiki

1. Clone the wiki repo (replace with your remote):
   ```bash
   git clone git@github.com:<owner>/<repo>.wiki.git /tmp/mn42-wiki
   ```
2. Copy this folder's contents into that clone:
   ```bash
   rsync -av --delete wiki/ /tmp/mn42-wiki/
   ```
3. Commit and push:
   ```bash
   cd /tmp/mn42-wiki
   git add .
   git commit -m "docs: update project wiki"
   git push
   ```

## Maintenance model

- Keep deep technical detail in canonical docs (`README.md`, `docs/`, `firmware/README.md`, `bridge/README.md`, `App/README.md`).
- Keep the wiki as a navigation and onboarding layer that points contributors to those sources.
- Keep generated visual aids reproducible. PNG illustrations live under `wiki/assets/signal-shapes/` and are regenerated with `python3 tools/generate_signal_shape_pngs.py`.
- Every top-level wiki page must include a `Canonical source: \`path\`` line near the top.
- CI enforces both local markdown link integrity and canonical-source declarations.
