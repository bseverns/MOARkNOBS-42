# Persistence Abuse

This page is the bench playbook for config/profile persistence abuse on the current hardware-test stack. It is intentionally conservative: safe default proofs run without destructive storage writes, destructive profile/storage checks require an explicit flag, and corruption cases stay in Unity or manual bench lanes until the repo has safe fault-injection hooks.

For document tie-break rules, see [Documentation Truth Map](../../reference/DocumentationTruthMap.md).

## Receipts

- JSON report wrapper: `firmware/system_test/mn42_persistence_abuse_runner.js`
- Non-destructive boot/apply/readback evidence: `firmware/system_test/mn42_boot_contract_runner.js`
- Destructive storage smoke evidence: `firmware/system_test/mn42_fullstack_runner.js --exercise-storage`
- Manual hardware sketch for deep EEPROM recovery checks: `pio run -d firmware -e teensy40_eeprom_persistence -t upload`

## Scenario matrix

| Scenario                                | Current lane                                          | Automation level           | Evidence today                                                                 | Notes                                                                                                                |
| --------------------------------------- | ----------------------------------------------------- | -------------------------- | ------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------- |
| Normal config apply + reboot + readback | `mn42_persistence_abuse_runner.js` default path       | Automated HIL              | `mn42_boot_contract_runner.js --attach-live`                                   | Safe default proof. Stages one small config change, waits for ACK, verifies readback, then restores baseline config. |
| Reset or disconnect before ACK          | Manual                                                | Manual only                | Operator notes + serial log                                                    | No safe host hook exists to cut power or force disconnect mid-`SET_ALL`.                                             |
| Reset or disconnect after ACK           | Manual                                                | Manual only                | Operator notes + serial log                                                    | Similar to above. Boot-contract proof covers readback after apply, but not an injected reset immediately after ACK.  |
| Corrupt primary / backup good           | Unity                                                 | Automated                  | `test_config_load_restores_from_backup_and_repairs_primary`                    | Uses the in-memory storage backend. Does not scribble corruption onto attached hardware.                             |
| Primary good / backup corrupt           | Unity                                                 | Automated                  | `test_config_load_prefers_primary_when_backup_copy_is_invalid`                 | Confirms the healthy primary wins when the backup copy is bad.                                                       |
| Both corrupt -> defaults with warning   | Unity                                                 | Automated                  | `test_config_load_resets_to_defaults_when_primary_and_backup_are_both_corrupt` | Confirms defaults load path and recovery event.                                                                      |
| Profile save/load/reset                 | `mn42_persistence_abuse_runner.js --exercise-storage` | Automated HIL, destructive | `mn42_fullstack_runner.js --exercise-storage`                                  | Overwrites sacrificial profile, macro, and scene slots.                                                              |
| Profile save interrupted                | Unity                                                 | Automated                  | `test_profile_save_interruption_leaves_latest_copy_in_backup`                  | Simulates interrupted primary writes while preserving the backup-first contract.                                     |
| Legacy config migration                 | Deferred                                              | Not currently automated    | None                                                                           | No dedicated persisted legacy-config fixture is scripted today.                                                      |

## Safe default command

Run the non-destructive receipt first:

```bash
node firmware/system_test/mn42_persistence_abuse_runner.js \
  --serial /dev/cu.usbmodemXXXX \
  --report logs/persistence-abuse-safe.json
```

What it proves:

- direct serial `HELLO` still works on the production firmware lane
- one staged config change applies with checksum/ACK
- readback matches the applied config
- cleanup restores the original config

Artifacts written:

- JSON report at the requested `--report` path
- Markdown receipt at `docs/bench/firmware/YYYY-MM-DD_persistence-safe-summary.md`

## Destructive storage command

Only use sacrificial storage targets:

```bash
node firmware/system_test/mn42_persistence_abuse_runner.js \
  --serial /dev/cu.usbmodemXXXX \
  --exercise-storage \
  --profile-slot 3 \
  --scene-slot 5 \
  --pot-index 0 \
  --report logs/persistence-abuse-storage.json
```

This intentionally overwrites:

- the selected profile slot
- the single macro snapshot
- the selected scene slot

## Manual fault-injection steps

Use these only when you need evidence the current automation does not honestly provide.

### Reset or disconnect before ACK

1. Flash `teensy40_main`.
2. Start the safe persistence runner or the boot-contract runner.
3. During the staged `SET_ALL` apply, manually press reset or unplug USB before the ACK appears.
4. Reattach the board, reconnect, and record whether the old config or the staged config survived.
5. Save the serial log and summarize the observation as a dated bench receipt.

### Reset or disconnect after ACK

1. Repeat the same lane.
2. Wait until the ACK/checksum is printed.
3. Immediately reset or unplug the board after the ACK.
4. Reconnect and confirm whether readback still matches the ACKed config.
5. Save the serial log and summarize the result.

### Primary/backup corruption on a real board

Do not script raw EEPROM corruption against the production firmware from the host until the repo has an explicit fault-injection hook. Use the dedicated persistence sketch instead:

```bash
pio run -d firmware -e teensy40_eeprom_persistence -t upload
```

That sketch is the current manual lane for verifying staged reboot steps such as primary corruption with backup recovery.
