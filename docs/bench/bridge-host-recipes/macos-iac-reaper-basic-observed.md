# Bridge Host Recipe Receipt: macOS IAC + REAPER Basic (Observed)

Date/time: 2026-05-31 22:07 CDT (`logs/bridge-host-recipe-macos-iac-reaper-20260531-220724.json` captured at `2026-06-01T03:07:37.007Z`)
Git commit/tag: `f15aa3a`
Host OS: macOS 26.5 (build 25F71)
Bridge mode: source
Node version or artifact name: `v24.13.0`
DAW/app/version: REAPER `7.59.0_ae874bau`
OSC ports: send `9000`, listen `9000`, host `127.0.0.1`, bind `127.0.0.1`
MIDI port label: `IAC 1 Bus 1`
Serial device or simulator: `/dev/tty.usbmodem192460701`
Artifact paths: [logs/bridge-host-recipe-macos-iac-reaper-20260531-220724.json](/Users/bseverns/Documents/GitHub/benzknober/logs/bridge-host-recipe-macos-iac-reaper-20260531-220724.json)

## Actions tested

1. Confirmed the board was attached as `/dev/cu.usbmodem192460701`.
2. Confirmed REAPER was running on the host and captured the reported app version `7.59.0_ae874bau`.
3. Started the Bridge from source with `npm --prefix bridge start`.
4. Connected the Bridge to serial `/dev/tty.usbmodem192460701` with MIDI label `IAC 1 Bus 1`.
5. Waited for the Bridge `/api/state` snapshot to report `ready: true`, `serialConnected: true`, and a populated device session.
6. Captured the resulting Bridge state JSON and route-trace summary to a local artifact.

## Result

PARTIAL

## Proven

- A live macOS host with REAPER `7.59.0_ae874bau` was present during the run.
- The Bridge source runtime on Node `v24.13.0` reached `ready` against the attached board while configured for MIDI label `IAC 1 Bus 1`.
- The Bridge emitted active route traces and maintained a populated cached device session during that run.

## Caveats

- This receipt does not prove that REAPER successfully ingested a recorded CC lane from `IAC 1 Bus 1`.
- This receipt does not prove Reaper-to-Bridge return CC traffic.
- This is an observation receipt, not a verified recipe receipt.
- There is no screenshot or DAW project artifact in this pass.

## Supporting evidence

- Recipe definition: [Known Good Host Recipes](../../reference/KnownGoodHostRecipes.md)
- Supporting bridge receipt: [2026-05-30-structured-bridge-session-summary.md](../bridge/2026-05-30-structured-bridge-session-summary.md)
- Supporting firmware receipt: unknown/not captured

## Artifact notes

- Local artifact JSON: [logs/bridge-host-recipe-macos-iac-reaper-20260531-220724.json](/Users/bseverns/Documents/GitHub/benzknober/logs/bridge-host-recipe-macos-iac-reaper-20260531-220724.json)
- The artifact includes the Bridge `ready` snapshot, live manifest/session state, route-count summary, and the observed REAPER version metadata.
