# Firmware Bench Summary: Bridge Session

Date:
Commit:
Firmware git_sha: 8113ff6
Firmware version: 0.0.0
Schema version: 6
Board revision: Rev A prototype
Serial port: /dev/cu.usbmodem192460701
Host: Republican-Hivemind
Node:
Runner: firmware/system_test/mn42_bridge_session_runner.js
Report artifact: logs/bridge-session-test.json

## Result

PASS

## Proven

- Bridge console starts.
- Structured session becomes ready using device schema.
- /ws/events emits device.ready.
- /api/device/stage accepts live config mutation.
- /api/device/apply returns checksum ACK.
- Staged config is promoted.
- Cleanup restores original idle_floor.

## Caveats

- Board has known OLED/SDA instability.
- Board power topology under review; do not treat LED full-brightness behavior as validated.

[bridge-session:server] bridge console: http://127.0.0.1:8791/
[bridge-session:server] serial up on /dev/cu.usbmodem192460701 @115200
[bridge-session] wrote report to logs/bridge-session-test.json
[bridge-session] PASS – Bridge server starts and exposes the console address: http://127.0.0.1:8791
[bridge-session] PASS – Structured bridge session becomes ready on hardware: schemaSource=device fw=0.0.0
[bridge-session] PASS – Structured websocket emits device.ready: device.ready observed on /ws/events
[bridge-session] PASS – Structured stage endpoint accepts a live device config: idle_floor 24 -> 25
[bridge-session] PASS – Structured apply returns ACK and promotes staged config: checksum=159c3066af5379ccec0553fbe5362f37ea2bf1bcd48f671cd1f2f890c8f60353
[bridge-session] PASS – Cleanup apply restores the original live config: idle_floor restored to 24
