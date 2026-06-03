# Adjacent Controller Lessons

> **Doc class:** Planning doc. This page records what MN42 should learn from the local reference projects without copying their identities.

This comparison was made against local clones under `~/Code/controller-references`:

- `shanteacontrols/OpenDeck`
- `16n-faderbank/16n`
- `jescriba/MIDIBox`

The useful question is not "how can MN42 look like those projects?" The useful question is "what project-hardening habits do they expose that MN42 should adapt to its own identity?"

## OpenDeck Lesson: Promise, Configurator, Boundaries

OpenDeck leads with a clear promise: build MIDI/OSC controller behavior without coding, with configuration exposed through a browser. Its front door also separates the official board from other supported boards.

MN42 should adapt:

- keep [Configure Without Recompiling](../getting-started/ConfigureWithoutRecompiling.md) visible early
- state exactly which behaviors can move through App/Bridge/profile workflows
- keep support boundaries conservative and evidence-backed
- separate "this is configurable" from "this is universally supported"

MN42 should not copy:

- the generic controller-platform identity
- broad board-support language
- claims that configuration replaces understanding the instrument

## 16n Lesson: Object, Repository, Build Artifacts

16n is clean because it defines the object first: what the faders are, what comes out, what hardware files exist, what is legacy, and where the build guide lives.

MN42 should adapt:

- keep [Object Card](../getting-started/ObjectCard.md) short and physical
- maintain a clear [Repository Contents](RepositoryContents.md) page
- separate current hardware references from legacy or historical files
- keep fabrication caveats attached to hardware file descriptions

MN42 should not copy:

- the minimal control-surface model
- the assumption that the repo is fabrication-ready
- the idea that the wiki can carry all build truth while repo docs stay thin

## MIDIBox Lesson: Small Code-As-Lesson Surface

MIDIBox is compact enough that the repo reads like a learning artifact. It names its premise directly and keeps the educational path close to the code.

MN42 should adapt:

- keep [One Signal Path](../learn/OneSignalPath.md) as a compact teaching page
- add short "read this code next" links when docs introduce a subsystem
- keep at least some pages small enough to read before opening source
- explain why App Apply, Bridge session, firmware config, and modulation each exist

MN42 should not copy:

- the tiny documentation footprint
- the single-purpose embedded-demo framing
- the lack of separate App/Bridge/firmware contracts

## Hardening Backlog

| Priority | Gap                                                        | Adapted Lesson                                            | Next Move                                                                                                                    |
| -------- | ---------------------------------------------------------- | --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| High     | Readers need a repo-level contents map.                    | 16n repository contents and artifact separation.          | Maintain [Repository Contents](RepositoryContents.md) and link it from README/Docs Guide.                                    |
| High     | Configuration claims need precise boundaries.              | OpenDeck browser-config clarity, but narrower.            | Keep [Configure Without Recompiling](../getting-started/ConfigureWithoutRecompiling.md) tied to contracts and host evidence. |
| High     | Release status can be confused with working software.      | 16n version/status clarity plus MN42 evidence discipline. | Keep [Release Boundary Index](../release/ReleaseBoundaryIndex.md) visible in Prove.                                          |
| Medium   | Reactive modulation needs one map without one giant guide. | OpenDeck feature discoverability, MN42-specific routing.  | Let [Reactive Modulation Matrix](../guides/ReactiveModulationMatrix.md) act as the map before merging guides.                |
| Medium   | Bridge docs are useful but split across audiences.         | 16n directory map style.                                  | Keep [Bridge Docs Map](../bridge/BridgeDocsMap.md) current instead of merging everything.                                    |
| Medium   | Educational code paths should remain short.                | MIDIBox code-as-lesson.                                   | Add short "read next in code" links to firmware/App/Bridge teaching pages where useful.                                      |
| Low      | Hardware files need stronger legacy/current labeling.      | 16n electronics/panel README style.                       | Audit `hardware/` docs for current, legacy, reference-only, and fabrication-ready labels.                                    |

## Guardrails

- Do not turn MN42 into a generic controller platform.
- Do not make the docs look more release-ready than the evidence supports.
- Do not hide bench receipts, caveats, or release blockers to make the site cleaner.
- Do not merge educational guides until the new index pages have proven insufficient.
- Do not claim fabrication readiness without the required release artifacts.
