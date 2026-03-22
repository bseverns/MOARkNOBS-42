# Connectivity Guide

Use this page when you are deciding between the browser configurator and the bridge.

## The short version

- Use the `configurator` when you want to talk directly to one board over USB for setup, monitoring, and profile management.
- Use the `bridge` when you need OSC or a virtual MIDI port for a DAW or host application.

## What the configurator does

The browser configurator talks directly to the board over WebSerial. It is the best path for:

- connecting to one board over USB
- viewing manifest/config data
- staging and applying edits
- loading and saving profiles
- exporting/importing JSON configs

See [App/README.md](../App/README.md).

## What the bridge does

The bridge is a Node.js process that sits between the board and other software. It is the best path for:

- turning board telemetry into OSC
- creating a virtual MIDI port for a DAW
- receiving OSC or MIDI commands and forwarding them back to the board

See [bridge/README.md](../bridge/README.md).

## When WebSerial alone is enough

WebSerial alone is enough when all of these are true:

- the board is connected directly over USB
- you only need configuration, monitoring, and profile work
- you do not need OSC routing
- you do not need a virtual MIDI device in another host

## When the bridge is needed

Use the bridge when any of these are true:

- you want OSC in Max, Pure Data, TouchOSC, or another OSC-capable host
- you want a DAW to see a virtual MIDI port named `MN42 Bridge`
- your workflow is built around OSC/MIDI software instead of direct browser configuration

## What other software sees

### What a DAW sees

The bridge exposes a virtual MIDI port named `MN42 Bridge`.

Bridge MIDI mapping documented in the repo:

- Channel 1 CC `0..41` = slot updates
- Channel 2 CC `0..5` = envelope follower updates

### What an OSC host sees

The bridge publishes:

- `/mn42/slots`
- `/mn42/envelopes`

And it accepts commands at:

- `/mn42/cmd`

## Minimal compatibility matrix

| Workflow | What is required | Status in this repo pass | Notes |
| --- | --- | --- | --- |
| Browser configurator over WebSerial | Browser with WebSerial support plus `http://localhost` or another allowed secure context | Verified documentation path | The repo documents this flow and the app tests run in Chromium-based automation. |
| Browser configurator in Chromium-based test path | Chromium-based browser | Verified in repo CI/docs | This is based on the Playwright setup and local app docs. |
| Browser configurator in Firefox or Safari | Browser support for the same WebSerial flow | `TODO — not verified in this repo pass` | Use the bridge if your browser path is uncertain. |
| Bridge CLI on desktop host | Node.js plus USB serial access | Verified documentation path | The repo documents macOS, Linux, and Windows serial-port discovery commands, but this pass did not bench-test all three. |
| OSC host integration | Running bridge plus UDP OSC host | Verified documentation path | Address and port contract are documented in `bridge/README.md`. |
| DAW integration | Running bridge plus DAW MIDI device enablement | Verified documentation path | The bridge advertises `MN42 Bridge`; this pass did not bench-test DAW-specific behavior. |

## Recommended decision rule

1. Start with the configurator if your goal is setup or profile editing.
2. Add the bridge only when you need OSC or DAW-facing virtual MIDI.
3. If a browser path is uncertain, use the bridge rather than guessing.
