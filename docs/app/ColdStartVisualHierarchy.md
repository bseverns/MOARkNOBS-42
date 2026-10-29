# Connection-First Visual Hierarchy

> **Doc class:** Design record. The behavioral rules remain in [App Behavior Contract](AppBehaviorContract.md).

## Before

The first viewport exposed the mode switch, transport lanes, a profile target, power placeholders, and the entire workspace tool inventory before a controller had completed a handshake. The empty state mixed connection, simulator, and mapping instructions while the tool inventory competed with the primary next action.

## After

### Disconnected

1. **Identity** — MOARkNOBS-42 only.
2. **Connection** — one primary action: **Connect controller**.
3. **Explicit alternatives** — **Start simulator** and a closed **Offline recovery and backups** disclosure.

Device identity, firmware, schema, profile, power, telemetry, mode controls, and workspace inventory are absent. The offline-recovery disclosure exposes only browser-local import/export recovery; it does not imply a device is attached or permit a write.

### Connected

1. **Verified connection** — the runtime-provided connection banner and connected status appear only after the `connected` event.
2. **Next action** — a short confirmed-state message explains that changes remain staged until Apply.
3. **Optional tools** — Workspace Tools is a closed disclosure. Opening it uses the existing workspace-panel controller; it neither changes the selected mode nor writes to the device.

## Exceptions and authority

- A `panel` or `panels` deep link is an explicit request, so it retains the requested-panel workflow rather than being replaced by the generic cold start.
- Simulator remains an explicit transport choice and uses its existing canonical/default or Demo Rig fixtures.
- Runtime connection events remain the source of truth. The cold-start shell forwards user gestures to existing controls and does not synthesize device state.
- Recovery continues to stage browser-local files; Apply, profile persistence, Bridge behavior, and firmware behavior are unchanged.

## Visual evidence

`App/tests/visual_regression.spec.js` maintains two production-hierarchy baselines at 1440 × 1000:

- `cold-start-disconnected-1440x1000.png`
- `cold-start-connected-1440x1000.png`
