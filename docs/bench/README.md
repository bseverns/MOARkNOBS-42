# Bench Receipts

> **Doc class:** Evidence index. This page points to observed receipts, methods, and templates. It does not replace the release gates in [Release Criteria](../release/ReleaseCriteria.md).

Bench receipts record what was actually observed on a rig, host, browser, or release lane. They are intentionally narrower than product claims: a receipt should say what hardware, firmware, App, Bridge, host, and date were involved.

## Receipt Index

| Area        | Receipt                                                                                     | What It Proves                                                               |
| ----------- | ------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| App         | [App over Bridge session summary](app/2026-05-31-app-over-bridge-session-summary.md)        | Browser App can operate through the structured Bridge session path.          |
| Bridge      | [2026-08-26 structured session failure](bridge/2026-08-26-structured-bridge-session-failure.md) | Current runner is blocked by Bridge control-token authentication drift.   |
| Bridge      | [Structured Bridge session summary](bridge/2026-05-30-structured-bridge-session-summary.md) | Bridge session behavior was exercised against the expected runtime contract. |
| Host recipe | [macOS IAC + REAPER basic observed](bridge-host-recipes/macos-iac-reaper-basic-observed.md) | A specific host routing recipe was observed, not universally certified.      |
| Firmware    | [Boot contract summary](firmware/0526_boot-contract-summary.md)                             | Firmware boot identity and contract behavior were checked.                   |
| Firmware    | [2026-08-26 boot contract](firmware/2026-08-26_boot-contract-summary.md)                     | Current production firmware handoff, apply, readback, and cleanup passed.     |
| Firmware    | [2026-08-26 live controls](firmware/2026-08-26_live-controls-summary.md)                     | Current live-control lanes round-tripped and restored without config drift.  |
| Firmware    | [Bridge session summary](firmware/0526_bridge-session-summary.md)                           | Firmware behavior was exercised through the Bridge session path.             |
| Firmware    | [Persistence abuse](firmware/PersistenceAbuse.md)                                           | EEPROM/profile persistence was stressed beyond normal happy-path use.        |
| Latency     | [Latency method](latency/method.md)                                                         | How latency measurements should be captured.                                 |
| Latency     | [Oblique RTL](latency/oblique_rtl.md)                                                       | A specific round-trip latency observation.                                   |
| Noise       | [Noise method](noise/method.md)                                                             | How ADC/noise captures should be recorded.                                   |

## Templates

Use these when adding new receipts:

- [App over Bridge session template](app/TEMPLATE_app-over-bridge-session-summary.md)
- [Direct Web Serial template](app/TEMPLATE_direct-webserial-summary.md)
- [Bridge session template](bridge/TEMPLATE_bridge-session-summary.md)
- [Packaged console HIL template](bridge/TEMPLATE_packaged-console-hil-summary.md)
- [Host recipe template](bridge-host-recipes/TEMPLATE_host-recipe-receipt.md)

## Methods And Context

- [App bench folder](app/README.md)
- [Bridge bench folder](bridge/README.md)
- [Bridge host recipes](bridge-host-recipes/README.md)
- [Environment conditions](environment/conditions.json)
- [Firmware build stamp](firmware/build.txt)

## Receipt Status Labels

- **Observed:** run on a named setup with enough detail to repeat.
- **Template:** a receipt shape waiting for a run.
- **Method:** how to measure, not a claim that a measurement has passed.
- **Release evidence:** referenced by a release lane or release criteria page.

## Adding A Receipt

1. Start from the closest template.
2. Record date, operator, hardware revision, firmware version or commit, App/Bridge version, host OS, browser, and transport.
3. Say what passed, what failed, and what was not attempted.
4. Keep caveats visible. Do not convert one observed host recipe into a universal compatibility claim.
5. Link the receipt from this index only after it has enough setup detail to be useful.
