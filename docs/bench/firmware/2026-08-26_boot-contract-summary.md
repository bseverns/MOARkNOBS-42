# Firmware Bench Summary: Boot Contract

Date: 2026-08-26
Commit: 407c271709d363ce31a48798f9ab3c114bdf82f8
Commit short: 407c2717
Firmware git_sha: 407c2717
Firmware version: 0.0.0
Schema version: 9
Power profile: POWER_CHOKED_V1
Serial port: /dev/cu.usbmodem192460701
Host: Mac.lan
Platform: darwin 27.0.0
Firmware env: teensy40_main
Runner: firmware/system_test/mn42_boot_contract_runner.js --attach-live
JSON report: logs/boot-contract-2026-08-26.json
Raw log: logs/boot-contract-2026-08-26.log

## Result

PASS

## Proven

- Already-running production firmware answered `HELLO` before the configurator handoff.
- `ENTER_CONFIG_MODE` acknowledged the request and rebooted into the USB configurator lane.
- `HELLO`, `GET_MANIFEST`, `GET_SCHEMA`, and `GET_CONFIG` completed against the attached board.
- The device reported 42 slots, schema 9, firmware git SHA `407c2717`, and the `POWER_CHOKED_V1` power profile.
- A one-step `filter.idle_floor` mutation applied with a matching checksum ACK and authoritative readback.
- Cleanup restored the original value and confirmed it with a second checksum ACK and readback.

## Safe Persistence Summary

- Baseline: `filter.idle_floor=24`
- Applied: `filter.idle_floor=25`
- Apply checksum: `cfc78ef5c5ab77ecdb87ffb17eb10fdf1ac275e936e1e494cd91da88de2c2bf5`
- Restored: `filter.idle_floor=24`
- Restore checksum: `dc47bb15a3b3c1ac264a2d078543a5e03789ae494c905cb68baa264c3365d438`
- Final cleanup readback: `filter.idle_floor=24`

## Observations

- The manifest reported LittleFS persistence `ready`, primary config valid, backup config valid, and the primary copy as the last load source.
- The reboot banner reported zero brownouts.
- The manifest reported `display_status=not_attempted` in configurator mode; this run does not establish display health.
- The manifest reported LED brightness cap 26 and `rail_topology_verified=false`; this receipt does not validate higher-power LED states.

## Caveats

- Attach-live mode proves handoff from an already-running firmware image. It does not prove a cold standalone boot marker.
- The board was not flashed by this run; firmware identity came from the attached device manifest.
- This was the safe default config mutation only. No profile, macro, scene, corruption-injection, or power-cut storage exercise was run.
