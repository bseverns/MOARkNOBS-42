# Bridge Packaging Plan

This document defines how to ship the MN42 Bridge for users who do not want to run Node commands manually.

## Goals

- Ship a double-click install/run path on macOS, Windows, and Linux.
- Keep the current CLI behavior and flags intact for advanced users.
- Preserve OSC/MIDI behavior exactly as documented in [`bridge/README.md`](../bridge/README.md).

## Non-goals

- Rewriting bridge logic in another language.
- Changing bridge protocol or message schema.
- Replacing the existing CLI for development workflows.

## Current state (as of 2026-02-04)

- Bridge entrypoint: `bridge/mn42_bridge.js`
- Runtime requirement: Node `>=20 <21`
- Native dependency: `serialport`
- Tests: `npm --prefix bridge test`

## Packaging options

| Option | Why use it | Risks / caveats | Recommendation |
| --- | --- | --- | --- |
| `pkg` | Mature single-binary workflow, simple output model | Target/runtime support can lag newest Node versions; native modules need validation | Prototype first |
| `nexe` | Flexible build pipeline, can embed assets | Build setup can be heavier; native dependency handling still needs platform validation | Backup path |

Decision rule: start with `pkg`; if target/runtime support blocks Node 20 + serialport reliability, switch to `nexe`.

## Release architecture

- Keep `mn42_bridge.js` as the source of truth.
- Add packaging scripts under `bridge/scripts/`.
- Emit per-platform artifacts into `bridge/dist/` during build.
- Keep a plain CLI distribution path for developers.

## Delivery format

- **macOS**: signed `.app` wrapper plus embedded bridge binary (or signed standalone binary + launcher script).
- **Windows**: signed `.exe` with installer (MSI or Inno Setup wrapper).
- **Linux**: tarball and optional AppImage.

All packages should expose the same defaults:
- OSC out `9000`
- OSC listen `9000`
- MIDI label `MN42 Bridge`

## Implementation phases

### Phase 0 - Baseline and guardrails

- Freeze bridge behavior with test coverage:
  - `npm --prefix bridge run release:prep`
- Smoke checks validate:
  - binary launches,
  - `--help` works,
  - invalid args fail predictably.

Deliverable: reproducible baseline before packaging changes.

### Phase 1 - Packaging prototype

- Build candidate artifacts with `npm --prefix bridge run package:bridge`.
- Try `pkg` first; keep target strings configurable (tool support changes over time).
- Produce unsigned local builds for all three platforms.

Deliverable: locally runnable binaries on macOS, Windows, Linux.

### Phase 2 - Native dependency validation

- Validate serial open/close/reconnect on real hardware for each platform.
- Validate MIDI port creation and OSC round-trip.
- Confirm command validation limits still enforced.

Deliverable: platform validation checklist with pass/fail evidence.

### Phase 3 - Installer UX

- Add minimal launcher UX options:
  - serial port field,
  - OSC ports,
  - start/stop,
  - log pane.
- Persist last-known settings per user.

Deliverable: non-CLI-friendly launcher experience.

### Phase 4 - Signing and release integration

- Add code-signing steps per OS.
- Add CI job to produce signed artifacts from tags.
- Attach bridge artifacts to GitHub release.

Deliverable: trusted installer/binary assets in each release.

## CI/CD checklist

For tag `vX.Y.Z`:

1. Build firmware assets.
2. Run bridge tests.
3. Build bridge packages for each OS.
4. Sign/notarize.
5. Publish artifacts.
6. Publish checksums + third-party license bundle.

## Acceptance criteria

A release is complete when all are true:

- Non-technical user can install and launch bridge in under 5 minutes.
- DAW detects `MN42 Bridge` with no manual Node install.
- OSC command round-trip works with documented example command.
- At least one reconnect scenario (USB unplug/replug) is handled gracefully.
- Documentation matches the shipped installer flow.

## Risks and mitigations

- **Node target mismatch in packager**: keep toolchain matrix pinned and tested in CI.
- **Native module breakage (`serialport`)**: run platform smoke tests against real hardware each release.
- **Code-signing friction**: stage unsigned internal builds first, then add signing in a dedicated phase.
- **Behavior drift from CLI docs**: keep `bridge/README.md` examples in sync as a release gate.

## Related docs

- User quickstart: [`docs/OSCBridge.md`](OSCBridge.md)
- Performer cheat sheet: [`docs/BridgeForPerformers.md`](BridgeForPerformers.md)
- Bridge reference: [`bridge/README.md`](../bridge/README.md)
- Release checklist: [`docs/ReleaseGuide.md`](ReleaseGuide.md)
