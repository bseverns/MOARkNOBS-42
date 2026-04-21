# Hardware-Test Package Manifest

## Included Lanes

### Firmware

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

### Host-Side Validation

- `npm --prefix bridge test`
- `npm --prefix App test`
- `python3 tools/check_markdown_links.py`
- `python3 tools/export_hardware_test_source.py`
- `node firmware/system_test/mn42_fullstack_runner.js`

### Included Top-Level Surfaces

- `firmware/`
- `hardware/`
- `App/`
- `bridge/`
- `docs/hardware-test/`
- selected `tools/` helpers
- `requirements.txt`
- `test.sh`
- `README.md`
- `HARDWARE_TEST_README.md`

## Excluded Or Removed Lanes

These lanes are intentionally not part of the hardware-test package surface:

- `teensy40_main_legacy`
- `teensy40_eeprom_persistence_legacy`
- `teensy40_led_demo`
- `teensy40_usb_midi_random_cc`
- `teensy40_usb_midi_random_note`
- `teensy40_usb_midi_random_mixed`

These repository workflows are also out of scope for the hardware-test package and are replaced by a single package-aligned workflow:

- docs publishing workflows
- wiki publishing workflows
- release packaging workflows
- broad release-oriented CI lanes

## Archive Exclusions

The hardware-test source archive excludes at least:

- `node_modules/`
- `.pio/`
- `.platformio/`
- `.DS_Store`
- `site/`
- `dist/`
- `logs/`
- Playwright reports and `App/test-results/`
- `tmp-slot-grid.png`
- Python `__pycache__/`
