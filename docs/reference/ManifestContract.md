# Manifest Contract

`GET_MANIFEST` is the first source of device truth for the App and Bridge. Fallback constants exist only so host tools can render before a device answers.

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

## Board Power Profiles

| Profile        | Compile define                                 | LED cap | Rail verified | Intended use                                                        |
| -------------- | ---------------------------------------------- | ------: | ------------- | ------------------------------------------------------------------- |
| Safe board     | `-DMN42_BOARD_POWER_PROFILE=POWER_CHOKED_V1`   |    `26` | `false`       | Current v1 board with conservative shared-rail power safety.        |
| Reworked board | `-DMN42_BOARD_POWER_PROFILE=SPLIT_RAIL_REWORK` |   `255` | `true`        | Split-rail rework after topology and thermal behavior are verified. |

The firmware clamps runtime LED brightness through `LEDManager` regardless of App or Bridge input. The manifest reports the active cap so host tools can show the operator whether the firmware is in a safe or reworked power profile.

## Host Fallbacks

The App fallback constants live in `App/manifest_contract.js`. Bridge fallback/documented constants live in `bridge/lib/manifest_contract.js`. `tools/check_contract_sync.py` compares those host fallbacks against firmware constants and the Globals-derived LED count.

Run it from the repo root:

```bash
python3 tools/check_contract_sync.py
```

Any mismatch is a release blocker unless the firmware and host migration path are updated together.
