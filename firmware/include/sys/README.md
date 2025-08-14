# sys/report

> Tiny helper that spits out a JSON blob describing the firmware build.

Call `sys::report()` to grab the string, or `sys::printReport()` to blast it to any `Print` stream. The payload includes:

- `fw_version` – whatever `FW_VERSION` was stamped with at build time
- `git_sha` – short commit hash so you know exactly which beast you're running
- `board` – the board name baked in at compile time (e.g. `teensy40`)
- `mcu` – the chip slinging the bits
- `f_cpu_hz` – clock speed in raw Hertz
- `build_time` – when the binary was forged
- `compiler` – the toolchain version that did the forging
- `arduino` – Arduino core version number
- `pio_env` – PlatformIO env tag if it snuck in

This lives under `sys/` so we can grow more low-level diagnostics without polluting the main include root.
