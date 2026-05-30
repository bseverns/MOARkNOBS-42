# Bridge Bench Summary: Structured Session

Date: 2026-05-30
Commit: 5db84bd
Firmware git_sha: 5db84bd
Firmware version: 0.0.0
Schema version: 6
Board revision: unknown/not captured
Board rework state: unknown/not captured
Serial port: /dev/cu.usbmodem192460701
Host: Republican-Hivemind.local
Node: v20.20.2
Bridge mode: source
Firmware env: teensy40_main
Runner: firmware/system_test/mn42_bridge_session_runner.js
Artifact / report path: logs/bridge-session-20260530-121954.json

## Result

PASS

## Proven

- Source bridge console started on localhost and attached to the hardware serial lane.
- Structured session became ready using device-provided schema and live config.
- `/ws/events` emitted `device.ready`.
- Structured stage/apply promoted a live config change and returned an ACK checksum.
- Cleanup restored the original `idle_floor` baseline.

## Caveats

- This was a source-mode bridge run, not a packaged console or signed installer proof.
- The host Node runtime was `v20.20.2`; this run needed `node --experimental-websocket` because repo support remains `Node >=24 <25`.
- The paired attach-live boot-contract artifact recorded `{"warning":"display_init_failed"}` during standalone boot.
- The manifest for this run reported `POWER_CHOKED_V1`, LED cap `26`, and `rail_topology_verified=false`; do not treat higher-power rail states as validated from this receipt.

## PASS output

```text
[bridge-session:server] bridge console: http://127.0.0.1:8791/
[bridge-session:server] serial up on /dev/cu.usbmodem192460701 @115200
[bridge-session] wrote report to logs/bridge-session-20260530-121954.json
[bridge-session] PASS – Bridge server starts and exposes the console address: http://127.0.0.1:8791
[bridge-session] PASS – Structured bridge session becomes ready on hardware: schemaSource=device fw=0.0.0
[bridge-session] PASS – Structured websocket emits device.ready: device.ready observed on /ws/events
[bridge-session] PASS – Structured stage endpoint accepts a live device config: idle_floor 24 -> 25
[bridge-session] PASS – Structured apply returns ACK and promotes staged config: checksum=4421e295c789aab96446f86ec12be858c99d3da1dcfcb303c7ecb95b926de261
[bridge-session] PASS – Cleanup apply restores the original live config: idle_floor restored to 24
```
