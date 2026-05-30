# Validation Report: Attach-Live Config + Bridge Session

Date: 2026-05-30
Commit: 5db84bd
Host: Republican-Hivemind.local
Node: v20.20.2
Board revision: unknown/not captured
Board rework state: unknown/not captured
Serial port: /dev/cu.usbmodem192460701
Firmware env: teensy40_main

## Scope

This report records the board-backed evidence captured on 2026-05-30 for the current hardware-test / beta-candidate stack. It does not claim packaged installer readiness, public release readiness, or validated high-power rail operation.

## Commands run

```bash
pio run -d firmware -e teensy40_main -t upload
node firmware/system_test/mn42_boot_contract_runner.js --serial /dev/cu.usbmodem192460701 --attach-live --report logs/boot-contract-attach-live-20260530-121954.json
node --experimental-websocket firmware/system_test/mn42_bridge_session_runner.js --serial /dev/cu.usbmodem192460701 --http-port 8791 --report logs/bridge-session-20260530-121954.json
```

## Result

PASS with hardware and host caveats.

## Proven

- `teensy40_main` flashed and re-enumerated successfully on the attached board.
- Attach-live boot contract passed:
  - live `HELLO` succeeded before configurator handoff
  - `ENTER_CONFIG_MODE` ACKed and rebooted into configurator mode
  - configurator `HELLO -> GET_MANIFEST -> GET_SCHEMA -> GET_CONFIG` completed
  - `SET_ALL` apply ACKed and cleanup restored the original `idle_floor`
- Bridge structured session passed against the same board:
  - HTTP console started at `http://127.0.0.1:8791`
  - session became ready from device schema
  - `/ws/events` emitted `device.ready`
  - structured stage/apply returned checksum ACK
  - cleanup restored the original live config

## Observed manifest truth

From `logs/boot-contract-attach-live-20260530-121954.json`:

- `power_profile`: `POWER_CHOKED_V1`
- `led_brightness_cap`: `26`
- `rail_topology_verified`: `false`
- `git_sha`: `5db84bd`

## Observed warnings and limits

- The attach-live boot artifact recorded `{"warning":"display_init_failed"}` during standalone boot.
- The bridge-session runner required `node --experimental-websocket` on this host because `v20.20.2` does not expose global `WebSocket` by default; repo support remains `Node >=24 <25`.
- This report does not cover soak, panic baseline, EF stability, EXT-clock starvation, thermal measurements, or validated split-rail / reworked board operation.

## Artifacts

- `logs/boot-contract-attach-live-20260530-121954.json`
- `logs/bridge-session-20260530-121954.json`
- [Bridge structured-session receipt](../../bench/bridge/2026-05-30-structured-bridge-session-summary.md)
