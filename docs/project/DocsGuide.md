# Docs Guide

This page is the sitemap and governance guide for MN42 documentation.

If two pages disagree, use the [Documentation Truth Map](../reference/DocumentationTruthMap.md). That page defines the truth hierarchy; this page only tells you where to go.

## Where Do I Start?

- New to the instrument: [Start Here](../getting-started/StartHere.md)
- Need the physical definition: [Object Card](../getting-started/ObjectCard.md)
- Need the repo map: [Repository Contents](RepositoryContents.md)
- Want a reading path: [Guided Routes](../getting-started/GuidedRoutes.md)
- Performing with the device: [Quickstart for Performers](../getting-started/QuickstartForPerformers.md)
- Building or flashing: [Quickstart for Builders](../getting-started/QuickstartForBuilders.md)
- Lost in terms: [Glossary](../reference/Glossary.md)

## Site Categories

Use these categories when deciding where a doc belongs:

| Category  | Use it for                                      | First pages                                                                                                   |
| --------- | ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| Learn     | concepts, vocabulary, and teaching paths        | [Why MN42](../getting-started/WhyMN42.md), [One Signal Path](../learn/OneSignalPath.md)                       |
| Use       | performer/operator workflows                    | [Configurator Tour](../guides/Configurator.md), [Profile Workflow](../guides/ProfileWorkflow.md)              |
| Build     | hardware, firmware, bring-up, flashing          | [Builder's Handbook](../getting-started/BuildersHandbook.md), [Bringup](../hardware-test/Bringup.md)          |
| Prove     | tests, validation, release readiness, receipts  | [TESTING](../validation/TESTING.md), [Release Criteria](../release/ReleaseCriteria.md)                        |
| Reference | protocol, contracts, compatibility, pin maps    | [MN42 Line Protocol](../reference/MN42LineProtocol.md), [Manifest Contract](../reference/ManifestContract.md) |
| Project   | support, process, history, pilot-run framing    | [License and Support](LicenseAndSupport.md), [Pilot Run](PilotRun.md)                                         |
| Archive   | old audits, generated reports, historical plans | [Changed Files Report](../agents/_reports/changed-files.md)                                                   |

## Which Docs Are Canonical?

Canonical behavior and support boundaries live in contract docs:

- [App README](../../App/README.md)
- [Bridge README](../../bridge/README.md)
- [Firmware README](../../firmware/README.md)
- [Host Compatibility](../reference/HostCompatibility.md)
- [Manifest Contract](../reference/ManifestContract.md)
- [MN42 Line Protocol](../reference/MN42LineProtocol.md)
- [Serial Protocol](../reference/SerialProtocol.md)
- [Bridge Transport Contract](../bridge/BridgeTransportContract.md)
- [Bridge Write Lanes](../bridge/BridgeWriteLanes.md)
- [Modulation Matrix Contract](../reference/ModulationMatrixContract.md)

Orientation pages may simplify these contracts, but they do not override them.

For folder ownership and generated-output boundaries, use [Repository Contents](RepositoryContents.md).

## Where Are Evidence Docs?

Evidence docs live under validation, release, bench, and hardware-test areas:

- [Testing Story](../validation/TestingStory.md)
- [TESTING](../validation/TESTING.md)
- [Validation Flow](../validation/ValidationFlow.md)
- [Release Criteria](../release/ReleaseCriteria.md)
- [Bench Receipts](../bench/README.md)
- [Hardware Test Matrix](../hardware-test/TestMatrix.md)
- [Known Issues](../hardware-test/KnownIssues.md)

Bench receipts should remain in the repo even when they are not all visible in top navigation.

## Where Are Historical Docs?

Historical and planning docs are useful context, not current truth:

- [History](HISTORY.md)
- [Lineage](lineage.md)
- [TODO](TODO.md)
- `docs/agents/_reports/`
- dated audits such as [Repo Health Audit 2026-03](../validation/repo-health-audit-2026-03.md)
- old app/runtime upgrade plans and release-packaging plans

If a historical page conflicts with a contract or evidence doc, the historical page loses.

## What Do I Read If I Am Lost?

Read in this order:

1. [Object Card](../getting-started/ObjectCard.md)
2. [Guided Routes](../getting-started/GuidedRoutes.md)
3. [One Signal Path](../learn/OneSignalPath.md)
4. [Glossary](../reference/Glossary.md)
5. [Documentation Truth Map](../reference/DocumentationTruthMap.md)

Then choose one lane: Learn, Use, Build, Prove, Reference, Project, or Archive.

## Editing Rules

- Put new conceptual/teaching pages in Learn.
- Put operator workflows in Use.
- Put first-boot and flashing workflows in Build.
- Put proof, checklists, receipts, and release gates in Prove.
- Put protocol and compatibility details in Reference.
- Put support, process, history, and governance in Project.
- Put generated reports, old audits, and stale plans in Archive.

Before adding a new page, decide whether it is contract, evidence, orientation, planning, or history. If that is unclear, update the [Documentation Truth Map](../reference/DocumentationTruthMap.md) or the [Documentation Compaction Plan](DocumentationCompactionPlan.md).
