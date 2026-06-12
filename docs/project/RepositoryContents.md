# Repository Contents

> **Doc class:** Orientation. This page explains what the main repository areas own and how to tell current evidence from historical or generated material.

MN42 is a hardware-test package around a specific reactive performance instrument. The repo contains firmware, browser App, Bridge, hardware references, validation docs, and release evidence. Not every folder is equally canonical.

## Current Working Areas

| Path                 | Owns                                                                                                   | Status                                                                                                                                        |
| -------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------- |
| `firmware/`          | Teensy 4.0 PlatformIO project, firmware source, Unity tests, full-system test runners                  | Canonical firmware project root. Build with `pio ... -d firmware`.                                                                            |
| `App/`               | Browser configurator, runtime modules, views, tests, presets                                           | Canonical browser/App implementation. Support boundary is in [App README](https://github.com/bseverns/MOARkNOBS-42/blob/main/App/README.md).  |
| `bridge/`            | Desktop Bridge CLI, console/server, device session cache, OSC/MIDI transport, tests, packaging scripts | Canonical Bridge implementation. Support boundary is in [Bridge README](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md). |
| `hardware/`          | Current hardware reference PDFs, fabrication exports, current-build notes                              | Reference and hardware-test evidence. Not a public fabrication-ready package by itself.                                                       |
| `docs/`              | MkDocs site source: Learn, Use, Build, Prove, Reference, Project, Archive                              | Canonical reader-facing documentation. Truth rules are in [Documentation Truth Map](../reference/DocumentationTruthMap.md).                   |
| `tools/`             | Repo checks, contract checks, release-readiness checks, serial/logging helpers                         | Guardrails and automation. Prefer these over manual interpretation when they exist.                                                           |
| `.github/workflows/` | CI, release, package, and preflight automation                                                         | Evidence automation. Passing CI does not automatically widen release status.                                                                  |

## Evidence And Generated Areas

| Path                                                    | Meaning                                                              | Caution                                                              |
| ------------------------------------------------------- | -------------------------------------------------------------------- | -------------------------------------------------------------------- |
| `docs/bench/`                                           | Observed receipts, methods, and templates                            | A receipt proves one setup, not universal compatibility.             |
| `docs/validation/`                                      | Testing story, validation flow, punch lists, audits                  | Use dated reports and current criteria before making release claims. |
| `docs/release/`                                         | Release checklist, criteria, boundary index, signing/packaging plans | Planning docs do not override release criteria.                      |
| `dist/`                                                 | Generated release/build artifacts                                    | Generated output; do not treat as source truth.                      |
| `site/`                                                 | Generated MkDocs output when present                                 | Generated output; edit `docs/`, not `site/`.                         |
| `.pio/`, `.pio-cache/`, `.platformio/`, `node_modules/` | Local build/dependency caches                                        | Local machine state, not documentation or product evidence.          |

## Current Status Snapshot

- Repository status: hardware-test package.
- Default firmware env: `teensy40_main`.
- Default power profile: `POWER_CHOKED_V1`.
- Rail topology verified by default: `false`.
- Strongest direct-browser evidence: Chromium-based WebSerial.
- Strongest desktop-host evidence: Node 24 Bridge with browser console and `/app/`.
- Bridge release artifacts: unsigned CI/evidence bundles unless a release explicitly says otherwise.

For release language, use [Release Boundary Index](../release/ReleaseBoundaryIndex.md).

## Where To Start

- Physical/object definition: [Object Card](../getting-started/ObjectCard.md)
- Whole-system diagram: [System Map](../getting-started/SystemMap.md)
- Role-based reading paths: [Guided Routes](../getting-started/GuidedRoutes.md)
- Build/bring-up: [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
- Performer workflow: [Quickstart for Performers](../getting-started/QuickstartForPerformers.md)
- Tests and proof: [TESTING](../validation/TESTING.md)

## File-Status Rules

- Treat App, Bridge, firmware, and contract docs as current truth when they agree with code/checks.
- Treat bench receipts as observed evidence for named setups.
- Treat generated folders as disposable unless a release process explicitly captures them as artifacts.
- Treat old audits, planning docs, and archive reports as historical context.
- If two docs disagree, use [Documentation Truth Map](../reference/DocumentationTruthMap.md).
