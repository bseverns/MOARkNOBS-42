# Bridge Bench Summary: Structured Session

> Historical diagnostic receipt. The authentication and chunked-read issues recorded here were resolved later the same day; see the [passing rerun](2026-08-26-structured-bridge-session-summary.md).

Date: 2026-08-26
Commit: 407c271709d363ce31a48798f9ab3c114bdf82f8
Firmware git_sha: 407c2717
Firmware version: 0.0.0
Schema version: 9
Board revision: unknown/not captured
Board rework state: unknown/not captured
Serial port: /dev/cu.usbmodem192460701
Host: Mac.lan
Node: v24.18.1
Bridge mode: source
Firmware env: teensy40_main
Runner: firmware/system_test/mn42_bridge_session_runner.js
Artifact / report path: logs/bridge-session-2026-08-26.json
Raw log: logs/bridge-session-2026-08-26.log

## Result

FAIL — runner authentication mismatch before device connection (subsequently resolved)

## Observed

- The source Bridge console started successfully on `127.0.0.1:8791` and generated a control-token URL.
- The HIL runner attempted `/ws/events` without the generated control token.
- The Bridge rejected the unauthenticated WebSocket connection, and the runner stopped with `Structured websocket failed`.
- No `/api/connect`, device-session readiness, staged config, apply, or cleanup step ran in this attempt.

## Impact

- This receipt does not prove or disprove Bridge-to-device behavior on the attached board.
- The failure exposed drift between the token-protected Bridge server and the runner. The runner now consumes the generated token, authenticates its WebSocket and protected API requests, and redacts the credential from logs.
- Because failure happened before device connection, this attempt did not mutate device configuration.

## Runner output

The ephemeral control token was redacted from the console URL.

```text
[bridge-session:server] bridge console: http://127.0.0.1:8791/?token=[redacted]
[bridge-session] Scenario failed: Structured websocket failed
[bridge-session] wrote report to logs/bridge-session-2026-08-26.json
[bridge-session] PASS – Bridge server starts and exposes the console address: http://127.0.0.1:8791
[bridge-session] FAIL – Scenario failure: Structured websocket failed
```
