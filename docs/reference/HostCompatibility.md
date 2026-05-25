# Host Compatibility

Use this page when you need the conservative answer to "what host paths are actually supported by repo evidence right now?"

This is a pre-production compatibility note, not a mass-market promise sheet.

## Compatibility buckets

- `Verified in repo evidence`
  - The repo has direct automated coverage, explicit validation steps, or both.
- `Documented path, bench validation still required`
  - The contract and operator path are documented, but this repo pass does not claim broad real-host validation.
- `Not claimed`
  - The repo does not present this as a verified path yet.

## Current matrix

| Surface                                                        | Current status                                   | Why                                                                                                                                                       |
| -------------------------------------------------------------- | ------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Browser configurator in Chromium-based browsers over WebSerial | Verified in repo evidence                        | Playwright coverage and the configurator docs treat this as the strongest direct-browser path.                                                            |
| Browser configurator served through the bridge at `/app/`      | Verified in repo evidence                        | The bridge UI and app tests cover the browser-driven bridge path and shared configurator runtime.                                                         |
| Structured bridge session API (`/api/device/*`, `/ws/events`)  | Verified in repo evidence                        | Bridge contract tests now cover the cached session state, staged apply flow, and structured event envelope.                                               |
| Browser configurator in Firefox or Safari over WebSerial       | Not claimed                                      | This repo does not present Firefox/Safari WebSerial as a verified production path.                                                                        |
| Bridge on a Node.js 24 desktop host                            | Verified in repo evidence                        | The bridge runtime, docs, and test suite are aligned around the Node 24 desktop path.                                                                     |
| OSC host integration through the bridge                        | Documented path, bench validation still required | The OSC command and telemetry contract are documented and tested at the software seam, but host-specific bench proof still depends on the operator setup. |
| DAW integration through the bridge virtual MIDI port           | Documented path, bench validation still required | The bridge exposes the documented virtual MIDI contract, but this repo does not claim broad DAW-by-DAW validation.                                        |
| Unsigned CI-generated bridge artifact                          | Verified in repo evidence                        | Release workflow packages per-target unsigned artifacts with checksums and metadata, but that is not presented as a signed/public installer path.         |
| Signed one-click bridge installer                              | Not claimed                                      | Signing/notarization is still planning and documentation work, not a shipped support claim.                                                               |

## Practical decision rule

1. Use a Chromium-based browser when you want the shortest direct configurator path.
2. Use the bridge when you need OSC, a virtual MIDI port, or you want to avoid betting on browser WebSerial support.
3. Treat DAW and OSC host behavior as setup-specific until you have your own bench proof.

## Where to go next

- [Connectivity Guide](../getting-started/ConnectivityGuide.md)
- [Quickstart for Performers](../getting-started/QuickstartForPerformers.md)
- [OSC Bridge](../guides/OSCBridge.md)
- [Configurator Tour](../guides/Configurator.md)
