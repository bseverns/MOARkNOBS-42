# MOARkNOBS-42 Browser Configurator

Use the browser configurator to connect directly over WebSerial, monitor the instrument, edit mappings, and manage
profiles. If you need OSC, host MIDI, or a browser without WebSerial, use the [Bridge](../bridge/README.md) path.

The fastest hardware-free introduction is the [hosted configurator](https://bseverns.github.io/MN42/): select
**Start simulator**, change a slot, review the draft, and apply it.

## Support boundary

- Direct USB is best evidenced on Chromium-based browsers with WebSerial.
- App-over-Bridge is best evidenced with the Node 24 Bridge on a desktop host.
- Firefox/Safari WebSerial and universal browser support are not claimed.
- The simulator rehearses App behavior; it is not hardware, electrical, MIDI-wiring, or latency evidence.

See [Host Compatibility](../docs/reference/HostCompatibility.md) before widening those claims.

## Quick start: direct USB

1. Flash the controller and connect it with a USB data cable.
2. From `App/`, serve the files from a secure or localhost origin:

   ```bash
   python3 -m http.server 8000
   ```

3. Open <http://localhost:8000/> in a Chromium-based browser.
4. Select **Check compatibility**, then **Connect**, and choose the MN42 serial device.
5. Wait for the identity, manifest, schema, and configuration handshake to finish.
6. Stage an edit, review the diff, select **Apply staged changes**, and wait for verified readback.

The legacy `benzknobz.html` URL remains a supported local/test entry point.

## Quick start: App over Bridge

```bash
npm --prefix bridge ci
npm --prefix bridge start
```

Open <http://127.0.0.1:8787/>, start the device session, then select **Open configurator**. The App uses the structured
Bridge session for configuration while OSC and MIDI routing remain active.

## Everyday workflow

- Use **Configure** for mapping and profile work.
- Use **Stage** for a compact performance dashboard and safe recall controls.
- Use **Lab** for envelope, ARG, LFO, filter, scope, monitor, import/export, and diagnostic surfaces.
- Importing JSON stages a draft; it does not write the device until Apply succeeds.
- Export saves the current staged state, including unsent changes.
- Profile, scene, macro, and transport actions that could replace a dirty draft are blocked until it is applied or discarded.
- If Apply becomes uncertain, keep the candidate visible and let authoritative readback resolve device truth.

## Troubleshooting

- **Connect is unavailable:** wait for the page to finish initializing, then run **Check compatibility**.
- **No device selected:** the browser picker was cancelled; select **Connect** again and choose the device.
- **WebSerial is blocked:** use HTTPS or `http://localhost`, and confirm the browser supports WebSerial.
- **Apply is disabled:** fix the validation errors shown in the staged-diff panel.
- **State is uncertain/resynchronizing:** do not repeat writes blindly; wait for authoritative readback.
- **Need to recover during a set:** use **Panic Help**; the hardware baseline combo is `Ctrl0 + Ctrl1 + Ctrl2`.

## Develop and test

```bash
npm --prefix App test
```

The Playwright suite serves the real runtime and view modules, then exercises simulator, schema, staged Apply,
uncertainty, migration, profile, and UI behavior.

## Reference

- [App Behavior Contract](../docs/app/AppBehaviorContract.md) — runtime ownership, modes, schema, staged state, telemetry, and simulator boundaries
- [Configuration Transaction Model](../docs/reference/ConfigurationTransactionModel.md) — candidate, receipt, uncertainty, and readback semantics
- [App Transport Truth Table](../docs/app/AppTransportTruthTable.md) — direct, structured Bridge, raw Bridge, and simulator labels
- [Configurator Tour](../docs/guides/Configurator.md) — operator-facing UI walkthrough
- [App bench receipts](../docs/bench/app/README.md) — observed host/device evidence
- [Documentation Truth Map](../docs/reference/DocumentationTruthMap.md) — tie-break rules
