# Manifest Contract

> **Doc class:** Contract doc. This page defines host-visible firmware manifest fields and conservative fallback behavior.

`GET_MANIFEST` is the first source of device truth for the App and Bridge. Host fallbacks stay pinned to the conservative safe profile so tools can render before a device answers without overstating board power readiness.

## Required Fields

| Field                    | Current fallback  | Source                                                  |
| ------------------------ | ----------------- | ------------------------------------------------------- |
| `device_name`            | `MOARkNOBS-42`    | `interop/mn42_contract.json`                            |
| `schema_version`         | `8`               | `interop/mn42_contract.json`                            |
| `slot_count`             | `42`              | `interop/mn42_contract.json`                            |
| `pot_count`              | `42`              | `interop/mn42_contract.json`                            |
| `envelope_count`         | `6`               | `interop/mn42_contract.json`                            |
| `arg_method_count`       | `14`              | `ARGMethod::XORR + 1`                                   |
| `led_count`              | `52`              | `interop/mn42_contract.json`                            |
| `power_profile`          | `POWER_CHOKED_V1` | Canonical fallback; firmware reports active profile     |
| `led_brightness_cap`     | `26`              | Canonical fallback; firmware reports active profile     |
| `rail_topology_verified` | `false`           | Canonical fallback; firmware reports active profile     |

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
| `persistence.detail`          | Additive backend readiness or failure code; useful for diagnostics, never a substitute for `status`.     |
| `capabilities.scene_capacity` | Number of complete scene slots supported by the reported backend capacity.                               |
| `capabilities.arp_profile_assignments` | Profile arp payloads persist explicit slot assignments; profile recall arms them without starting note output. |
| `capabilities.verified_apply` | Firmware implements the complete verified Apply contract.                                                |
| `capabilities.apply_integrity_receipt` | A successful Apply ACK includes `applied_checksum` and `storage_generation`.                 |
| `capabilities.authoritative_readback` | Hosts can read device-owned configuration after Apply to verify semantic state.               |

The current commit and recovery guarantees are defined in the [Generation-Backed Persistence Contract](PersistenceContract.md). Hosts must not infer write readiness from the legacy `eeprom_*` fields.

Current hosts negotiate the three Apply capability fields above. For firmware
that predates those fields, `persistence.backend=littlefs` remains a temporary
compatibility signal for the same guarantees. An explicit capability value
always overrides that legacy inference.

Display health is intentionally observable because OLED bring-up can fail on the bench without invalidating the core configurator contract. Host tools should treat `display_ok=false` as a degraded-mode warning, not as proof that protocol lanes are unavailable.

## Board Power Profiles

| Profile        | PlatformIO env           | Compile define                                 | LED cap | Rail verified | Intended use                                                         |
| -------------- | ------------------------ | ---------------------------------------------- | ------: | ------------- | -------------------------------------------------------------------- |
| Safe board     | `teensy40_main`          | `-DMN42_BOARD_POWER_PROFILE=POWER_CHOKED_V1`   |    `26` | `false`       | Rev A or any board whose LED rail topology is still unverified.      |
| Reworked board | `teensy40_main_reworked` | `-DMN42_BOARD_POWER_PROFILE=SPLIT_RAIL_REWORK` |   `255` | `true`        | Only for boards whose split-rail rework and validation are complete. |

The firmware clamps runtime LED brightness through `LEDManager` regardless of App or Bridge input. The manifest reports the active cap so host tools can show the operator whether the firmware is in a safe or reworked power profile.

## Host Fallbacks

`interop/mn42_contract.json` is the machine-readable manifest authority and
`App/config_schema.json` is the canonical configuration schema. The generator
produces the App and Bridge fallback modules, the firmware manifest constants,
and the firmware `GET_SCHEMA` payload. The device projection intentionally
excludes the App-only `scenes` editor extension.

Regenerate after changing either canonical input:

```bash
python3 tools/generate_contract_artifacts.py --root .
```

Generated files carry a warning and must not be edited directly.
`tools/check_contract_sync.py` compares them byte-for-byte with generator
output; it no longer parses C++ expressions or reconstructs Arduino strings.

Run it from the repo root:

```bash
python3 tools/check_contract_sync.py
```

Any mismatch is a release blocker unless the firmware and host migration path are updated together.
