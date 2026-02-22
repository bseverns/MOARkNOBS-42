# Release Process

Use this as the release checklist summary. Detailed runbooks remain in `docs/ReleaseGuide.md`.

## Firmware release steps

1. Build and test:
   ```bash
   pio -d firmware run -e teensy40_main
   pio -d firmware test -e teensy40_unity -vvv
   npm --prefix bridge test
   npm --prefix App test
   ```
2. Update version constants (for example `firmware/include/Globals.h`).
3. Update `CHANGELOG.md`.
4. Commit release prep changes.
5. Tag release:
   ```bash
   git tag -a vX.Y.Z -m "vX.Y.Z"
   git push origin vX.Y.Z
   ```
6. Draft GitHub release and attach required artifacts/licenses.

## Bridge packaging lane

When shipping non-CLI bridge artifacts:

1. Follow `docs/BridgePackaging.md`.
2. Run bridge smoke tests per target OS.
3. Publish checksums and artifacts.
4. Complete `docs/release/bridge-artifacts-checklist.md`.

## Reference docs

- `docs/ReleaseGuide.md`
- `docs/release/bridge-artifacts-checklist.md`
- `CHANGELOG.md`

