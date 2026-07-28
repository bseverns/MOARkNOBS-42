# Manifest Contract

> **Doc class:** Contract doc. This page defines host-visible firmware manifest fields and conservative fallback behavior.

`GET_MANIFEST` is the first source of device truth for the App and Bridge. Host fallbacks stay pinned to the conservative safe profile so tools can render before a device answers without overstating board power readiness.

## Required Fields

| Field                    | Current fallback  | Source                                                  |
| ------------------------ | ----------------- | ------------------------------------------------------- |
| `device_name`            | `MOARkNOBS-42`    | `firmware/include/protocol/ManifestContract.h`          |
| `schema_version`         | `6`               | `firmware/include/Globals.h` `CONFIG_VERSION`           |
| `slot_count`             | `42`              | `firmware/include/MIDITypes.h` `NUM_SLOTS`              |
| `pot_count`              | `42`              | `firmware/include/Globals.h` `NUM_POTS`                 |
| `envelope_count`         | `6`               | `firmware/include/Globals.h` `NUM_ENVELOPES`            |
| `arg_method_count`       | `14`              | `ARGMethod::XORR + 1`                                   |
| `led_count`              | `52`              | `slotLedCount + efLedCount + control LED + potLedCount` |
| `power_profile`          | `POWER_CHOKED_V1` | `firmware/include/BoardPowerProfile.h`                  |
| `led_brightness_cap`     | `26`              | Active board power profile                              |
| `rail_topology_verified` | `false`           | Active board power profile                              |

## Operational Health Fields

These fields are host-visible diagnostics, not a fabrication or release-readiness claim.

| Field                   | Meaning                                                                                       |
| ----------------------- | --------------------------------------------------------------------------------------------- |
| `display_present`       | `true` when the most recent OLED init probe saw an I2C ACK at `0x3C`.                         |
| `display_ok`            | `true` when the OLED driver actually initialized and is ready to paint.                       |
| `display_init_failures` | Count of failed OLED init attempts since boot.                                                |
| `display_status`        | Stable result code: `not_attempted`, `ok`, `no_i2c_ack`, `driver_begin_failed`, or `timeout`. |
| `free_ram`              | Approximate free RAM snapshot for host diagnostics.                                           |
| `free_flash`            | Approximate remaining program flash for host diagnostics.                                     |
| `brownout_count`        | Brownout counter observed since boot.                                                         |
| `eeprom_primary_valid`  | Whether the primary EEPROM copy passed validation.                                            |
| `eeprom_backup_valid`   | Whether the backup EEPROM copy passed validation.                                             |
| `eeprom_last_load`      | Last EEPROM source used: `primary`, `backup`, `defaults`, or `unknown`.                       |

The `eeprom_*` names are legacy compatibility diagnostics. They describe whether the current configuration manager recovered a valid primary or backup copy; they do not mean the schema-4 EEPROM layout is the active persistence backend.

## Persistence and Capacity Fields

| Field                         | Meaning                                                                                                  |
| ----------------------------- | -------------------------------------------------------------------------------------------------------- |
| `persistence.backend`         | Active durable backend: `littlefs` when transactions are supported, otherwise `unavailable`.             |
| `persistence.capacity`        | Total bytes exposed by the active storage backend.                                                       |
| `persistence.layout_required` | Bytes required by the firmware's compiled configuration, profile, macro, and scene layout.               |
| `persistence.generation`      | Active committed storage generation.                                                                    |
| `persistence.status`          | `ready` when transactional storage exists and fits the required layout; otherwise `insufficient`.        |
| `capabilities.scene_capacity` | Number of complete scene slots supported by the reported backend capacity.                               |

The current commit and recovery guarantees are defined in the [Generation-Backed Persistence Contract](PersistenceContract.md). Hosts must not infer write readiness from the legacy `eeprom_*` fields.

Display health is intentionally observable because OLED bring-up can fail on the bench without invalidating the core configurator contract. Host tools should treat `display_ok=false` as a degraded-mode warning, not as proof that protocol lanes are unavailable.

## Board Power Profiles

| Profile        | PlatformIO env           | Compile define                                 | LED cap | Rail verified | Intended use                                                         |
| -------------- | ------------------------ | ---------------------------------------------- | ------: | ------------- | -------------------------------------------------------------------- |
| Safe board     | `teensy40_main`          | `-DMN42_BOARD_POWER_PROFILE=POWER_CHOKED_V1`   |    `26` | `false`       | Rev A or any board whose LED rail topology is still unverified.      |
| Reworked board | `teensy40_main_reworked` | `-DMN42_BOARD_POWER_PROFILE=SPLIT_RAIL_REWORK` |   `255` | `true`        | Only for boards whose split-rail rework and validation are complete. |

The firmware clamps runtime LED brightness through `LEDManager` regardless of App or Bridge input. The manifest reports the active cap so host tools can show the operator whether the firmware is in a safe or reworked power profile.

## Host Fallbacks

The App fallback constants live in `App/manifest_contract.js`. Bridge fallback/documented constants live in `bridge/lib/manifest_contract.js`. `tools/check_contract_sync.py` compares those host fallbacks against firmware constants, the Globals-derived LED count, and the shared firmware/App schema semantics that the configurator and bridge depend on.

Run it from the repo root:

```bash
python3 tools/check_contract_sync.py
```

Any mismatch is a release blocker unless the firmware and host migration path are updated together.
