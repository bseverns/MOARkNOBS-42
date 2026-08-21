# Documentation Compaction Plan

This plan is a non-destructive first pass. It changes the reader experience before it changes the filesystem.

The goal is not fewer documents today. The goal is fewer first doors.

## Implementation Status

The first-door pass is now implemented:

- the root README introduces the complete instrument before hardware-test evidence
- performers, builders, contributors, and evaluators each have one designated landing page
- the visible Learn syllabus is five pages; deeper learning guides remain linked through progressive disclosure
- generated MkDocs path breadcrumbs orient readers in deep sections
- hardware-test status and board photos live in `HARDWARE_TEST_README.md`
- App and Bridge READMEs are operational quickstarts backed by focused behavior, transport, and operator references
- the wiki source pack is frozen and clearly labeled as historical

This remains a non-destructive compaction: deep guides, contracts, and evidence were demoted from the first door rather
than deleted. Future consolidation should be driven by observed navigation problems and content overlap.

## Current Diagnosis

MOARkNOBS-42 has useful docs, but too many pages are equally visible. A new reader sees learning guides, firmware contracts, release caveats, bench receipts, bridge internals, hardware sketches, and historical reports as peers.

That makes the project look less intentional than it is.

The current docs already contain the right material:

- a specific object and hardware-test boundary
- performer and builder quickstarts
- App, Bridge, firmware, manifest, schema, and line-protocol contracts
- validation and release-readiness evidence
- teaching-oriented guides for EF, ARG, filters, LFO routes, MIDI, and firmware reading

The missing layer is editorial hierarchy.

## Contrast Models

### What MN42 Should Learn From OpenDeck

OpenDeck gives readers a clear first promise: configurable MIDI/OSC controller behavior without coding, with browser configuration as the front door.

MN42 should borrow that clarity, not the generic-platform identity. The useful lesson is:

- say plainly what can be configured without recompiling
- keep browser configuration visible early
- separate everyday setup from firmware implementation detail
- make supported-path claims conservative and test-backed

### What MN42 Should Learn From 16n

16n starts with a clean physical object: what it is, what is in the repo, what the controls are, what comes out, and where to build it.

MN42 needs that same object card before readers fall into App/Bridge/Firmware architecture. The useful lesson is:

- define the instrument as a concrete object first
- show controls, outputs, connection surfaces, and current version/status
- keep build documents discoverable but not mixed with performer workflows
- let deep hardware evidence stay available without becoming the landing page

### What MN42 Should Learn From MIDIBox

MIDIBox is compact enough that code and hardware act as a lesson. Its premise is small, direct, and educational.

MN42 should preserve that teaching heart. The useful lesson is:

- teach one signal path at a time
- show how a pot, EF, LFO, App Apply, and Bridge session each move one value
- keep some docs short enough to be read before opening source code

## What MN42 Should Not Copy

MN42 should not copy OpenDeck's generic controller-platform posture. MN42 is a specific reactive performance instrument.

MN42 should not copy 16n's minimal surface model. MN42 has more live-state machinery: 42 slots, six envelope followers, EF/ARG/LFO modulation, profiles, browser configurator, bridge, manifests, schemas, and validation evidence.

MN42 should not copy MIDIBox's tiny-doc footprint. MN42 needs more contracts because it has App, Bridge, firmware, hardware-test, and release boundaries.

MN42 should not hide caveats, bench receipts, or release boundaries to make the site feel simpler.

## Proposed Reader-Facing Doc Model

Use these categories in the site and in future doc decisions:

- **Learn:** conceptual and educational docs.
- **Use:** performer/operator docs.
- **Build:** hardware, firmware, bring-up, and flashing docs.
- **Prove:** validation, bench, release-readiness, and evidence docs.
- **Reference:** protocol, manifest, pinout, bridge contracts, compatibility, and lookup docs.
- **Project:** license, support, history, pilot run, process, and documentation governance.
- **Archive:** generated reports, old audits, changed-files reports, agent reports, and historical planning notes.

## Proposed Nav Model

Use the visible navigation as a curated syllabus:

- **Home** answers what MN42 is.
- **Learn** explains the object, vocabulary, and signal paths.
- **Use** helps performers operate the instrument.
- **Build** helps builders bring up hardware and firmware.
- **Prove** keeps evidence and release readiness findable without overwhelming learners.
- **Reference** holds contracts and lookup tables.
- **Project** holds governance, support, release framing, and history.
- **Archive** exposes old reports and planning notes intentionally.

## Front-Door Pages

These should remain visible or near-visible:

- `docs/index.md`
- `docs/getting-started/StartHere.md`
- `docs/getting-started/ObjectCard.md`
- `docs/getting-started/SystemMap.md`
- `docs/getting-started/WhyMN42.md`
- `docs/getting-started/GuidedRoutes.md`
- `docs/getting-started/ConfigureWithoutRecompiling.md`
- `docs/learn/OneSignalPath.md`
- `docs/guides/ReactiveModulationMatrix.md`
- `docs/getting-started/QuickstartForPerformers.md`
- `docs/getting-started/QuickstartForBuilders.md`
- `docs/guides/Configurator.md`
- `docs/guides/ProfileWorkflow.md`
- `docs/getting-started/ConnectivityGuide.md`
- `docs/project/DocsGuide.md`
- `docs/project/RepositoryContents.md`
- `docs/reference/DocumentationTruthMap.md`

## Deep Reference Pages

These are important, but should not all be first-contact pages:

- `App/README.md`
- `bridge/README.md`
- `firmware/README.md`
- `docs/reference/SerialProtocol.md`
- `docs/reference/MN42LineProtocol.md`
- `docs/reference/ManifestContract.md`
- `docs/reference/ModulationMatrixContract.md`
- `docs/bridge/BridgeTransportContract.md`
- `docs/bridge/BridgeWriteLanes.md`
- `docs/reference/EEPROMLayout.md`
- `docs/reference/PinMap.md`
- `docs/reference/HostCompatibility.md`
- `docs/reference/KnownGoodHostRecipes.md`
- `docs/reference/HardwareSubstitutions.md`
- `docs/reference/EF_Frontend_v2.md`
- `docs/reference/Options_DNI.md`
- `docs/reference/assumption-ledger.md`
- `docs/firmware/FirmwareMainReadingPath.md`
- `docs/firmware/ProtocolStackReadingPath.md`

## Evidence And Archive Pages

These should remain intact, but only the index or story pages should be prominent:

- `docs/validation/TESTING.md`
- `docs/validation/TestingStory.md`
- `docs/validation/ValidationFlow.md`
- `docs/validation/FailureFirst.md`
- `docs/release/ReleaseCriteria.md`
- `docs/release/ReleaseBoundaryIndex.md`
- `docs/release/ReleaseGuide.md`
- `docs/bench/README.md`
- `docs/hardware-test/Bringup.md`
- `docs/hardware-test/TestMatrix.md`
- `docs/hardware-test/KnownIssues.md`
- `docs/tools/*report-template.md`
- `docs/agents/_reports/`
- `docs/validation/repo-health-audit-2026-03.md`
- `docs/project/TODO.md`
- old app/runtime upgrade plans and release-packaging plans

## May Be Merged Later, But Not Yet

Do not merge these in the first pass. They need editorial review after the nav proves itself:

- `docs/guides/ReactiveControlGuide.md`, `ARGGuide.md`, `FilterFeelGuide.md`, and `LfoRouteGuide.md` should stay separate for now. `docs/guides/ReactiveModulationMatrix.md` is the chapter index; merge only after readers prove they want one longer reactive-control handbook.
- `docs/guides/OSCBridge.md`, `BridgeForPerformers.md`, and `docs/bridge/BridgeConsoleTour.md` should stay separate for now. `docs/bridge/BridgeDocsMap.md` is the cluster map; a single bridge handbook would hide useful performer/operator/contract boundaries.
- `docs/release/ReleaseStory.md`, `ReleaseGuide.md`, `ReleaseCriteria.md`, and `Reproducibility.md` could become a release area with one summary page and several contract pages.
- `docs/hardware-test/Bringup.md`, `TestMatrix.md`, and `docs/validation/ValidationFlow.md` may need a clearer split between first-boot and proof gates.
- `docs/project/ProcessOverview.md` and pieces of the old `DocsGuide.md` may overlap.
- `docs/Primers/MIDI-DSP101.md` and `docs/learn/OneSignalPath.md` may later become a compact teaching sequence.

## Closed First Gaps

These gaps from the first compaction pass now have short reader-facing pages or indexes:

- `docs/getting-started/SystemMap.md` shows hardware, App, Bridge, firmware contracts, and validation lanes.
- `docs/guides/ReactiveModulationMatrix.md` explains EF, ARG, LFO, slot routes, bridge writes, and conflict warnings.
- `docs/bench/README.md` is now a bench receipt index.
- `docs/release/ReleaseBoundaryIndex.md` separates hardware-test, demo, beta, and public release claims.
- `docs/bridge/BridgeDocsMap.md` maps performer, console, contract, packaging, and evidence Bridge docs.
- `docs/project/RepositoryContents.md` maps source, evidence, generated, and archive areas.
- `docs/project/AdjacentControllerLessons.md` records the OpenDeck/16n/MIDIBox comparison as a hardening backlog.
- `docs/bridge/BridgeDocsMap.md` now records the completed Bridge runtime upgrade status, replacing the stale upgrade plan.

## Missing Recommended Pages

No missing first-pass index pages remain. The remaining work is editorial review, not emergency page creation.

## Next Recommended Edits

1. Keep the simplified MkDocs nav for at least one iteration and watch which pages still feel misplaced.
2. Add doc-class notes to remaining planning and archive pages where reader confusion is likely.
3. Audit `hardware/` docs for current, legacy, reference-only, and fabrication-ready labels.
4. Add short "read next in code" links to firmware/App/Bridge teaching pages where useful.
5. Watch whether `ReactiveModulationMatrix.md` solves the reactive-control orientation problem before merging any reactive guides.
6. Watch whether `BridgeDocsMap.md` solves bridge-doc navigation before creating a single bridge handbook.
7. Only after that, merge overlapping guides. Do not delete evidence docs just because they are no longer top-nav pages.
