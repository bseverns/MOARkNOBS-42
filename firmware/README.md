# MOARkNOBS-42 Firmware Hardware-Test Guide

> **Doc class:** Contract doc. This is the firmware build/test boundary for the current hardware-test package; release and hardware-readiness claims still require the validation docs.

This `firmware/` directory is the real PlatformIO project for the current MOARkNOBS-42 hardware-test package.
Run PlatformIO here, not from the repo root.

For document tie-break rules, see [Documentation Truth Map](../docs/reference/DocumentationTruthMap.md).

## Baseline Build

```bash
pio run -e teensy40_main
```

The repo-root `platformio.ini` is only a guard that rejects accidental root-level builds.

## Firmware Environments

- `teensy40_main` for Rev A / unverified-rail boards; this is the conservative default build
- `teensy40_main_reworked` only for boards whose split-rail rework has been validated and documented
- `teensy40_full_system` for bridge-connected full-stack bench validation
- `teensy40_unified_test` for manual bench verification of buttons, pots, OLED, LEDs, and envelope inputs
- `teensy40_biquad_test` for filter verification
- `teensy40_eeprom_persistence` for staged persistence and backup-restore validation
- `teensy40_slot_verify` for slot-storage integrity checks
- `teensy40_led_demo` for LED pattern and load-surface checks
- `teensy40_button_ef_demo` for simple button plus envelope USB MIDI validation
- `teensy40_display_alive` for an OLED-only "is the panel alive?" check
- `teensy40_display_led_hw` for OLED and LED hardware bring-up
- `teensy40_power_burnin` for LED load and power-rail soak testing
- `teensy40_unity` for the custom Unity smoke suite

The full lane summary lives in [../docs/hardware-test/TestMatrix.md](../docs/hardware-test/TestMatrix.md).

`GET_MANIFEST` reports the active board power profile, LED cap, and rail-verification bit from the selected env, so do not flash `teensy40_main_reworked` unless the physical board actually matches that claim.

## Envelope Filter Timing

Envelope filter configuration keeps the legacy `frequency` field for wire and
preset compatibility, but the value is a shaping-control scale rather than a
physical cutoff in hertz. The runtime samples one of six followers every 2 ms,
giving each follower an effective rate of about 83.3 Hz. It translates the
legacy control ratio onto that real cadence before configuring the biquad.
Per-slot EF voices perform the same translation using the configured mid-tier
envelope interval. The `native_biquad` step-response test locks this timing
model without claiming analog-input-to-host latency.

## First Reading Path

If you are learning this firmware by reading code, start with
[src/firmware_main.cpp](src/firmware_main.cpp), then follow the ordered header
walkthrough in [../docs/firmware/FirmwareMainReadingPath.md](../docs/firmware/FirmwareMainReadingPath.md).
That path is the repo's intended "top-down" introduction to the machine.

## Core Commands

Firmware build:

```bash
pio run -e teensy40_main
```

Unity smoke suite:

```bash
pio test -e teensy40_unity -vvv
```

Unified bench sketch:

```bash
pio run -e teensy40_unified_test -t upload
```

LED surface stress sketch:

```bash
pio run -e teensy40_led_demo -t upload
```

Full bridge/system lane:

```bash
pio run -e teensy40_full_system -t upload
```

Production boot contract proof:

```bash
node system_test/mn42_boot_contract_runner.js --serial /dev/cu.usbmodemXXXX --flash --report ../logs/boot-contract.json
```

Attach-live hardware proof when the board is already running firmware and you only need the real configurator/apply lane:

```bash
node system_test/mn42_boot_contract_runner.js --serial /dev/cu.usbmodemXXXX --attach-live --report ../logs/boot-contract-attach-live.json
```

Request one-shot configurator boot from a running firmware image:

```text
ENTER_CONFIG_MODE
```

`ENTER_CONFIG_MODE` stores a one-shot boot marker and reboots. The next boot enters
USB configurator mode and consumes the marker; normal power-up defaults to the
standalone runtime so USB MIDI remains available without opening the configurator
or bridge first.

Live runtime controls exposed over the configurator/native protocol:

```text
GET_NOTE_DYNAMICS
SET_NOTE_DYNAMICS,<velocityShift>,<changeProbability>
GET_JITTER
SET_JITTER,<depth>,<smoothness>
GET_CLOCK
SET_CLOCK,<followExternal>,<clockOutEnabled>,<tappedBpm>
GET_USB_MIDI
SET_USB_MIDI,<0|1>
```

`SET_NOTE_DYNAMICS` clamps to `-64..63` and `0..100`. `SET_JITTER` clamps both
values to `0.0..1.0`. `SET_CLOCK` clamps the internal tempo to `20..300 BPM`.
The firmware advertises these lanes in the manifest so the browser configurator
can fail closed on older builds.

`GET_MANIFEST` also reports a small persistence-health snapshot for host tools:
`brownout_count`, `eeprom_primary_valid`, `eeprom_backup_valid`, and
`eeprom_last_load` (`primary`, `backup`, or `defaults`).

## Important Test Contract

- PlatformIO project root is `firmware/`.
- The Unity lane uses the custom Serial1 transport from `test/unity_output.cpp`.
- Do not re-enable PlatformIO's autogenerated Unity transport.
- Tests assume `board_build.usbtype = usb_midi_serial`.
- Hardware firmware builds override USB strings to enumerate as `MN42 MIDI` (manufacturer `MN42`).
- Do not pull display or SD stacks into the Unity lane unless explicitly needed.

## Local References

- [include/](include/README.md)
- [src/](src/README.md)
- [test/](test/README.md)
- [system_test/](system_test/README.md)
