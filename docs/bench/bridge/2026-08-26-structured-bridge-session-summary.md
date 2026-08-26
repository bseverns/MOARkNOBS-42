# Bridge Bench Summary: Structured Session

Date: 2026-08-26
Commit: d58c9d2a
Firmware git_sha: d58c9d2a
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

PASS

## Proven

- The source Bridge console started on loopback and generated a control-token URL.
- The HIL runner captured the ephemeral token, authenticated `/ws/events` and protected API requests, and redacted the token from console output.
- The Bridge opened the attached Teensy serial port and completed device session hydration from device manifest, schema, and config.
- `/ws/events` emitted a valid `device.ready` event.
- The structured stage endpoint accepted `filter.idle_floor` 24 -> 25.
- Apply returned a matching ACK checksum and promoted the staged config.
- Cleanup restored `filter.idle_floor` to the original value of 24 and confirmed it through live session readback.

## Transport Observation

- Earlier attempts showed that this firmware advertised chunked config reads but did not complete that stream reliably on this bench, saturating the serial lane and hiding a later Apply ACK.
- The Bridge now defaults to the bounded single-line `GET_CONFIG` response. Chunked config reads remain opt-in inside the device-session implementation and retain a tested fallback to `GET_CONFIG`.
- With the bounded response, the complete HIL scenario finished in approximately 2.6 seconds.

## Caveats

- This was a source-mode Bridge run, not a packaged console or signed installer proof.
- Board revision and rework state were not captured.
- The manifest reported `POWER_CHOKED_V1`, LED brightness cap 26, and `rail_topology_verified=false`; this receipt does not validate higher-power LED states.
- The manifest reported `display_status=not_attempted` in configurator mode; this receipt does not establish display health.

## PASS output

The ephemeral control token was redacted from the console URL.

```text
[bridge-session:server] bridge console: http://127.0.0.1:8791/?token=[redacted]
[bridge-session:server] serial up on /dev/cu.usbmodem192460701 @115200
[bridge-session] wrote report to logs/bridge-session-2026-08-26.json
[bridge-session] PASS – Bridge server starts and exposes the console address: http://127.0.0.1:8791
[bridge-session] PASS – Structured bridge session becomes ready on hardware: schemaSource=device fw=0.0.0
[bridge-session] PASS – Structured websocket emits device.ready: device.ready observed on /ws/events
[bridge-session] PASS – Structured stage endpoint accepts a live device config: idle_floor 24 -> 25
[bridge-session] PASS – Structured apply returns ACK and promotes staged config: checksum=1b1903c65c3850dac63dee5cf0952d5ba41786d7ef69c4b8e0f5a2121c364f08
[bridge-session] PASS – Cleanup apply restores the original live config: idle_floor restored to 24
```
