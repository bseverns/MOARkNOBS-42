# Generation-Backed Persistence Contract

> **Doc class:** Contract doc. This page defines the current schema-6 durable configuration layout and commit guarantees.

MOARkNOBS-42 uses a transactional LittleFS-backed virtual storage region. The current firmware exposes 49,152 bytes, and the manifest reports both that capacity and the bytes required by the compiled scene layout. Hosts must treat `persistence.status != "ready"` as a write-safety failure.

## Commit Model

Durable configuration uses two permanent data blobs and two metadata records. A transaction writes the inactive blob, verifies its checksum, then activates the new generation through redundant metadata. A failed or incomplete write never makes that generation authoritative, and a poisoned transaction cannot be committed later.

At boot, firmware selects the newest valid metadata record whose referenced blob passes validation. If neither generation is valid, firmware falls back to defaults and reports the compatibility health fields described in the [Manifest Contract](ManifestContract.md).

## Capacity Contract

The compiled layout includes configuration, profiles, macros, and scene slots. Firmware has a compile-time assertion that the complete scene layout fits the storage backend. Runtime capability reporting is conservative:

- `persistence.capacity` reports available bytes.
- `persistence.layout_required` reports bytes required by the compiled layout.
- `persistence.status` is `ready` only when transactional storage is present and large enough.
- `capabilities.scene_capacity` reports the number of complete scene slots that fit.
- `capabilities.scenes` is true only when the full compiled scene count fits.

## Apply Receipt

After durable Apply, firmware returns `applied_checksum` and `storage_generation`. The checksum is a device-owned digest of the applied configuration and the generation identifies the committed storage epoch. The App and Bridge must verify authoritative readback before reporting success when the receipt is absent, malformed, or differs from the transmitted candidate.

The current digest includes slots, profiles, ARG configuration, operating and LED modes, filters, envelope baselines, active profile, and USB MIDI state. It is a device-owned receipt, not yet a portable serialization checksum: host implementations must not reproduce it by hashing compiler object representations.

The historical schema-4 emulated EEPROM offsets are documented separately in [Legacy EEPROM Layout](EEPROMLayout.md) and are not the current persistence contract.
