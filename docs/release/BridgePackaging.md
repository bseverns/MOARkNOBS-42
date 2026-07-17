# Bridge Packaging Plan

This document defines how to ship the MN42 Bridge for users who do not want to run Node commands manually.

This is a release-planning doc, not the source of current bridge/runtime truth.

For what is true now, defer to [bridge/README.md](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md), [Host Compatibility](../reference/HostCompatibility.md), [Bridge Signing Plan](BridgeSigningPlan.md), and [Documentation Truth Map](../reference/DocumentationTruthMap.md).

## Goals

- Ship a double-click install/run path on macOS, Windows, and Linux.
- Keep the current CLI behavior and flags intact for advanced users.
- Preserve OSC/MIDI behavior exactly as documented in [bridge/README.md](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md).

## Non-goals

- Rewriting bridge logic in another language.
- Changing bridge protocol or message schema.
- Replacing the existing CLI for development workflows.

## Current state (as of 2026-05-25)

- Bridge entrypoints:
  - `bridge/mn42_bridge.js` for the CLI lane
  - `bridge/mn42_bridge_server.js` for the browser-console/App-over-bridge lane
- Runtime requirement: Node `>=24 <25`
- Native dependency: `serialport`
- Tests: `npm --prefix bridge test`
- `pkg` is pinned in `bridge/package.json` (`devDependencies`)
- `.github/workflows/release.yml` builds unsigned bridge artifacts for:
  - `node24-macos-x64`
  - `node24-macos-arm64`
  - `node24-linux-x64`
  - `node24-win-x64`
- Per-target bundles now carry both packaged programs, checksum file, a target README, artifact manifest metadata, and the bridge third-party license bundle.
- CI now does an artifact-realism smoke pass against the packaged console binary:
  - `/` serves the bridge UI
  - `/app/` serves the App shell
  - `/api/presets` returns bundled recipes
  - `/api/device/session?warm=1` proves the App-derived schema authority can load inside the packaged artifact
  - `/api/device/stage` rejects bad writes with a machine-readable error object
  - valid staged-config acceptance still depends on a real manifest/live-config handshake, so that proof remains HIL-only unless the smoke run is given a working serial device
- Bridge release uploads are conditional on an existing GitHub release for the tag, and current CI uploads are labeled as hardware-test/prerelease assets.
- Outward bridge builds must set `REQUIRE_BRIDGE_SIGNING=1` and provide signing/notarization credentials or hooks; otherwise the packaging script fails instead of silently producing unsigned binaries.

For the current support boundary, see [Host Compatibility](../reference/HostCompatibility.md).

## Packaging options considered

| Option | Why use it                                         | Risks / caveats                                                                        | Status        |
| ------ | -------------------------------------------------- | -------------------------------------------------------------------------------------- | ------------- |
| `pkg`  | Mature single-binary workflow, simple output model | Target/runtime support can lag newest Node versions; native modules need validation    | current path  |
| `nexe` | Flexible build pipeline, can embed assets          | Build setup can be heavier; native dependency handling still needs platform validation | fallback only |

Decision rule: keep `pkg` as the active path unless target/runtime support blocks Node 24 + `serialport` reliability badly enough to justify switching.

## Release architecture to preserve

- Keep `mn42_bridge.js` and `mn42_bridge_server.js` as the source entrypoints for the packaged CLI and console lanes.
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
  - CLI and console binaries launch,
  - `--help` works,
  - invalid args fail predictably,
  - the packaged console artifact can serve the browser console, `/app/`, presets, warmed schema authority, and fail-closed staged-write errors without hardware.

Deliverable: reproducible baseline before packaging changes.

### Phase 1 - Packaging prototype (implemented)

- Build candidate artifacts with `npm --prefix bridge run package:bridge`.
- Keep `pkg` as the active packager path and keep target strings configurable (tool support changes over time).
- Produce unsigned local builds for the current release targets.

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

### Phase 4 - Signing and release integration (implemented as gated hooks)

- Use `REQUIRE_BRIDGE_SIGNING=1` for beta/public bridge artifacts.
- macOS DMGs are built with `npm --prefix bridge run package:macos`; signed builds require `APPLE_CODESIGN_IDENTITY`, `APPLE_NOTARY_KEY`, `APPLE_NOTARY_KEY_ID`, and `APPLE_NOTARY_ISSUER_ID`.
- Windows/Linux or installer-specific signing can be supplied through `BRIDGE_SIGNING_COMMAND`; notarization/stapling-style post-processing can be supplied through `BRIDGE_NOTARIZE_COMMAND`.
- Keep current unsigned CI packaging in `.github/workflows/release.yml`.
- Upgrade release upload runs to use those hooks before attaching outward artifacts.

Deliverable: trusted installer/binary assets in each beta/public release.

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
- **Code-signing friction**: keep unsigned internal builds available, but require the signing gate for beta/public artifacts.
- **Behavior drift from CLI docs**: keep `bridge/README.md` examples in sync as a release gate.

## Related docs

- User quickstart: [`docs/guides/OSCBridge.md`](../guides/OSCBridge.md)
- Performer cheat sheet: [`docs/guides/BridgeForPerformers.md`](../guides/BridgeForPerformers.md)
- Bridge reference: [bridge/README.md](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md)
- Release checklist: [`docs/release/ReleaseGuide.md`](ReleaseGuide.md)
