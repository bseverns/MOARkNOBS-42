# Testing

Testing is layered from fast firmware checks to full hardware + bridge exercises.
Canonical source: `docs/validation/TESTING.md`

## Canonical command set

Run from repo root unless noted.

1. Full orchestrated suite:
   ```bash
   ./test.sh
   ```
2. Unity firmware suite (strict command from repo contract):
   ```bash
   pio test -d firmware -e teensy40_unity -vvv
   ```
3. Firmware build sanity:
   ```bash
   pio run -d firmware -e teensy40_main
   ```
4. Bridge tests:
   ```bash
   npm --prefix bridge test
   ```
5. App tests:
   ```bash
   npm --prefix App test
   ```

## Unity test contract (important)

- PlatformIO project root is `firmware/`.
- Unity transport is custom only (`firmware/test/unity_output.cpp` using `Serial1`).
- Do not use PlatformIO's default serial Unity transport.
- Do not add `-DSerial=...` transport substitutions.
- Do not include `unittest_transport.h` from PlatformIO defaults.
- USB mode for tests is `USB_MIDI_SERIAL`.

## Manual firmware test environments

- `teensy40_full_system`
- `teensy40_unified_test`
- `teensy40_biquad_test`
- `teensy40_eeprom_persistence`
- `teensy40_slot_verify`

Build and upload with:

```bash
pio run -d firmware -e <env> -t upload
```

## Full-stack hardware runner

```bash
node firmware/system_test/mn42_fullstack_runner.js --serial /dev/ttyACM0 --report logs/system-test.json
```

## Reference docs

- `docs/validation/TESTING.md`
- `firmware/test/README.md`
- `firmware/system_test/README.md`
