# Documentation Truth Map

This page defines which docs are authoritative for which claims.

The goal is simple: if two pages disagree, contributors should know which one wins without guessing.

## Truth classes

### 1. Contract docs

These pages define current behavior and support boundaries. They are the first place to update when code, protocol shape, or release posture changes.

- [App README](https://github.com/bseverns/MOARkNOBS-42/blob/main/App/README.md) for App runtime behavior, transport paths, staged/apply semantics, and the browser support boundary
- [Bridge README](https://github.com/bseverns/MOARkNOBS-42/blob/main/bridge/README.md) for bridge operating modes, structured vs raw transport, and unsigned artifact posture
- [Firmware README](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/README.md) for firmware build/run behavior and the host/device contract from the firmware side
- [Host Compatibility](HostCompatibility.md) for conservative supported-path claims
- [Manifest Contract](ManifestContract.md) for `GET_MANIFEST` semantics
- [MN42 Line Protocol](MN42LineProtocol.md) and [Serial Protocol](SerialProtocol.md) for the line-level command/response contract
- [Bridge Transport Contract](../bridge/BridgeTransportContract.md) for structured bridge event and API shapes
- [Bridge Write Lanes](../bridge/BridgeWriteLanes.md) for staged config writes versus live bridge performance writes
- [Configuration Transaction Model](ConfigurationTransactionModel.md) for Apply, uncertainty, resynchronization, verification, and local draft discard

Rule: these pages describe what is true now, not what is planned later.

### 2. Evidence docs

These pages describe what has been tested, observed, or bench-validated.

- [TESTING](../validation/TESTING.md)
- [Validation Flow](../validation/ValidationFlow.md)
- [Test Matrix](../hardware-test/TestMatrix.md)
- `docs/bench/` summaries and measurements
- release verification artifacts such as `dist/mn42_vX.Y.Z_hardware-test_verification.json`

Rule: evidence docs may be narrower than the contract docs, but they should never imply broader support than the evidence actually proves.

### 3. Orientation docs

These pages help humans choose a path through the project.

- [docs/index.md](../index.md)
- `docs/getting-started/`
- `docs/guides/`
- [License and Support](../project/LicenseAndSupport.md)
- [Pilot Run](../project/PilotRun.md)

Rule: orientation docs must defer to contract docs for specifics. They explain; they do not redefine protocol or support truth.

### 4. Historical and archived docs

These pages preserve narrative, prior reasoning, or audit snapshots.

- [Project History](../project/HISTORY.md)
- `docs/agents/_reports/`
- dated audit notes and archived planning reports

Rule: historical docs are useful context, but they lose tie-break authority whenever they conflict with a contract or evidence page.

## Reader categories

The MkDocs site uses reader-facing categories. These categories do not change the truth hierarchy above; they only help readers choose a path.

| Category  | Purpose                                         | Typical truth class                  |
| --------- | ----------------------------------------------- | ------------------------------------ |
| Learn     | conceptual and educational docs                 | orientation                          |
| Use       | performer/operator workflows                    | orientation, with links to contracts |
| Build     | hardware/firmware bring-up docs                 | orientation or evidence              |
| Prove     | validation, bench, release-readiness docs       | evidence                             |
| Reference | protocol, manifest, pinout, bridge contracts    | contract                             |
| Project   | support, process, history, pilot-run framing    | orientation or historical            |
| Archive   | generated reports, old audits, historical plans | historical                           |

If a category and a truth class seem to disagree, the truth class wins.

## Tie-break order

When two pages disagree, use this order:

1. Code and automated checks
2. Contract docs
3. Evidence docs
4. Orientation docs
5. Planning docs
6. Historical or archived docs

If code and docs disagree, update the docs or the code in the same change whenever practical.

The machine-readable source-to-doc mapping lives in [`docs/contracts.yml`](../contracts.yml).

## Update triggers

Update the contract/evidence docs when any of the following change:

- Node support boundary
- browser support boundary
- bridge packaging or signing posture
- handshake order or transport paths
- schema/manifest fields
- HIL coverage or release gating
- current verified hardware/runtime claims

## Practical editing rule

Before adding a new doc page, decide which class it belongs to.

- If it defines current behavior, it is a contract doc.
- If it records proof, it is an evidence doc.
- If it teaches a workflow, it is an orientation doc.
- If it proposes work, it is a planning doc.
- If it preserves past context, it is historical.

That classification should be visible in the page language, not hidden in the folder name alone.
