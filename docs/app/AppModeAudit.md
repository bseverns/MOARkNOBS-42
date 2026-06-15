# App Mode Audit

> **Doc class:** Internal UI audit. This page summarizes source-inspected App mode boundaries. It does not define transport support, release readiness, or hardware evidence.

Current source inspected: `App/index.html`, `App/views/`, `App/runtime/`, `App/tests/`, [App Transport Truth Table](AppTransportTruthTable.md), [Configurator Tour](../guides/Configurator.md), [Quickstart for Performers](../getting-started/QuickstartForPerformers.md), and `docs/bench/app/`.

## Stage Mode

**Visible panels**

- Performance Dashboard: connection, dirty state, device, firmware, profile, last event.
- Power summary, slot activity grid, envelope meters.
- Stage-safe actions: Connect, Load Profile, Recall Scene, Panic Help.

**Hidden panels**

- Live slot editor, schema forms, staged diff, utility tabs, device monitor, MIDI monitor, EF scope/LFO, LED controls, import/export stack, debug log.

**Writes-now controls**

- Load Profile and Recall Scene are live device actions when firmware reports support.

**Apply-required controls**

- None exposed in Stage. Staged editor and Apply/Rollback are hidden.

**Browser-only controls**

- Mode switch buttons. Stage displays browser-rendered status, but browser-only slot label/takeover editing is hidden.

**Possible confusion risks**

- Stage is not purely read-only because profile/scene recall can write to the device. The UI copy should say performance-safe instead of implying "no writes ever."

## Basic Mode

**Visible panels**

- Header connection controls: Connect and Apply.
- Power summary pill.
- Recovery & Profiles drawer with profile switch/save/reset and file backup actions.
- Live Slots, Envelope Levels, Selected Slot mapping editor.
- Console status, import/export, preset picker, Save staged edits.

**Hidden panels**

- Compatibility/config boot/rollback controls, guided profile wizard, macro snapshots, scenes panel.
- EF/ARG/filter/LED editors, utility tabs, staged diff panel, MIDI monitor, Device Monitor, scope, arp, LFO, modulation matrix, debug log.
- Slot Details snapshot is now Advanced-only to avoid duplicating the Basic editor surface.

**Writes-now controls**

- Device-backed profile switch/save/reset when firmware reports support.

**Apply-required controls**

- Slot mapping edits, preset/imported config changes, and Save staged edits flow.

**Browser-only controls**

- Slot labels, MIDI badges, and Take Control/pickup guards remain local browser state and do not require Apply.

**Possible confusion risks**

- Basic still shows both "live firmware" and "staged config" lane labels. That is useful, but the visible surface should avoid extra duplicate inspectors.

## Advanced Mode

**Visible panels**

- Everything in Basic, plus rollback, compatibility/config boot, guided profile save, macro snapshots, scenes, staged diff, MIDI monitor, Device Monitor, LED controls, EF/ARG/filter editors, scope, arp, LFO, modulation matrix, Slot Details, and debug log.

**Hidden panels**

- No intentional machine-room panels are hidden; Stage-only simplified actions are not the primary surface.

**Writes-now controls**

- USB MIDI toggle/test, Device Clock, Live Arp, Jitter, Note Dynamics, macro/scene/profile device actions, modulation matrix refresh/export/copy.

**Apply-required controls**

- Schema-backed config edits, profile-slot saved arp/LFO edits, preset/imported config changes, and staged full-config Apply.

**Browser-only controls**

- Slot labels, MIDI badges, Take Control/pickup guards, debug/session log actions, screenshots/exports, and local mode preference.

**Possible confusion risks**

- Advanced is intentionally the full machine room. Its risk is not hidden power; it is density. Keep it available, but do not let Basic drift back into this shape.

## Change Made From This Audit

- Made `#slot-detail-panel` Advanced-only. Basic already has the live slot grid and selected-slot editor; hiding the duplicate snapshot makes Basic calmer without removing the deep inspector from Advanced.
- Added Stage header copy: "Performance-safe status and recovery. No staged editors or Apply controls here." This clarifies that Stage is safe for performance use without pretending profile/scene actions are read-only.
