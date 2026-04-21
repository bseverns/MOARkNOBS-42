# MOARkNOBS-42 Hardware-Test Readme

## What This Package Is

This package is a self-contained hardware-test bundle for the current MOARkNOBS-42 prototype.
It is intended to validate:

- prototype firmware bring-up
- OLED function at address `0x3C`
- button matrix and control buttons
- potentiometer scanning
- WS2812 LED behavior
- USB MIDI and board-side MIDI routing
- envelope follower behavior and baseline checks
- WebSerial telemetry and configurator behavior
- OSC/MIDI bridge connectivity

## What This Package Is Not

- not a public release package
- not a fabrication-ready manufacturing bundle
- not a claim that the current Gerber archive or NC-drill outputs are verified
- not a signed desktop-installer package

Current package limitations are tracked in [docs/hardware-test/KnownIssues.md](docs/hardware-test/KnownIssues.md).

## Package Layout

- `firmware/` contains the real PlatformIO project and all kept firmware test lanes.
- `hardware/` contains the current schematic PDF, PCB/reference drawing PDF, BOM export, and parts notes.
- `App/` is included because WebSerial telemetry validation is part of this package.
- `bridge/` is included because bridge validation is part of this package.
- `docs/hardware-test/` contains the hardware-test operator docs.
- `tools/` contains the bench capture helpers, link checker, and source-export script.

## First Command

From a clean checkout, verify the documented firmware build:

```bash
pio -d firmware run -e teensy40_main
```

That is the package baseline. If this build fails, stop there and fix the toolchain before bench work.

## Setup

Install the Python-side requirements:

```bash
python3 -m pip install -r requirements.txt
```

Install the optional host-side validation dependencies when you need them:

```bash
npm --prefix bridge ci
npm --prefix App ci --no-audit --no-fund
```

## Supported Firmware Environments

- `teensy40_main`
- `teensy40_full_system`
- `teensy40_unified_test`
- `teensy40_biquad_test`
- `teensy40_eeprom_persistence`
- `teensy40_slot_verify`
- `teensy40_button_ef_demo`
- `teensy40_display_led_hw`
- `teensy40_power_burnin`
- `teensy40_unity`

See [docs/hardware-test/TestMatrix.md](docs/hardware-test/TestMatrix.md) for when to use each one.

## First-Boot Checklist

Run the first-boot procedure in [docs/hardware-test/Bringup.md](docs/hardware-test/Bringup.md).

The minimum bring-up sequence is:

1. Inspect power rails and soldering before first power.
2. Build and upload `teensy40_main`.
3. Confirm Teensy USB detection and serial output.
4. Run `teensy40_display_led_hw` to confirm the OLED path and LED path.
5. Run `teensy40_unified_test` for manual control, display, and EF checks.
6. Validate WebSerial with `App/`.
7. Validate bridge connectivity with `bridge/` plus `teensy40_full_system`.

## WebSerial Validation

Serve the configurator locally:

```bash
python3 -m http.server --directory App 8000
```

Then open `http://localhost:8000/`, connect to the board, and confirm:

- `HELLO` succeeds
- manifest/config load succeeds
- live telemetry updates while controls move

The App-specific reference is [App/README.md](App/README.md).

## Bridge Validation

Start the bridge:

```bash
npm --prefix bridge start
```

Then open `http://127.0.0.1:8787/` and confirm the bridge reaches the device after the
firmware sends its handshake. The bridge-specific reference is [bridge/README.md](bridge/README.md).

For the end-to-end lane, flash `teensy40_full_system` and run:

```bash
node firmware/system_test/mn42_fullstack_runner.js --serial /dev/ttyACM0 --report logs/system-test.json
```

Replace `/dev/ttyACM0` with the actual device path on your host.

## Source Package Export

To create a hardware-test source archive that excludes `node_modules`, `.pio`, `.platformio`,
`site`, `dist`, `logs`, `.DS_Store`, Playwright output, and temp files:

```bash
python3 tools/export_hardware_test_source.py --root . --label hardware-test --output dist/mn42_hardware_test.zip
```

The included and excluded surfaces for that archive are listed in
[docs/hardware-test/PackageManifest.md](docs/hardware-test/PackageManifest.md).
