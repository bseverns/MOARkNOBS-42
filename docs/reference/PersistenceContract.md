# Generation-Backed Persistence Contract

> **Doc class:** Contract doc. This page defines the current schema-8 durable configuration layout and commit guarantees.

MOARkNOBS-42 uses a transactional LittleFS-backed virtual storage region. The current firmware exposes 49,152 bytes, and the manifest reports both that capacity and the bytes required by the compiled scene layout. Hosts must treat `persistence.status != "ready"` as a write-safety failure.

## Commit Model

Durable configuration uses two permanent data blobs and two metadata records. A transaction writes the inactive blob, verifies its checksum, then activates the new generation through redundant metadata. A failed or incomplete write never makes that generation authoritative, and a poisoned transaction cannot be committed later.

At boot, firmware selects the newest valid metadata record whose referenced blob passes validation. If neither generation is valid, firmware falls back to defaults and reports the compatibility health fields described in the [Manifest Contract](ManifestContract.md).

The transaction image uses a single reserved DMA-memory buffer; concurrent
transactions are rejected rather than allocating from the general heap. The
inactive blob is written in bounded chunks and must have the expected size
before metadata is activated. A malformed inactive blob is non-authoritative
and is recreated from the intact active generation during startup repair.

Compatibility writes may update the active virtual EEPROM region outside a
bulk transaction. Metadata selection therefore requires a valid metadata
record and correctly sized blob; the checksum remains mandatory when a newly
staged inactive generation is verified before activation.

Runtime-only restoration is explicitly write-free. A failed transaction may
restore its prior live slots, ARG/envelope settings, filter tail, LED settings,
and mappings, but that restoration must not write either durable generation or
advance `storage_generation`.

## Capacity Contract

The compiled layout includes configuration, profiles, macros, and scene slots. Firmware has a compile-time assertion that the complete scene layout fits the storage backend. Runtime capability reporting is conservative:

- `persistence.capacity` reports available bytes.
- `persistence.layout_required` reports bytes required by the compiled layout.
- `persistence.status` is `ready` only when transactional storage is present and large enough.
- `persistence.detail` is a stable machine-readable readiness or failure code.
- `capabilities.scene_capacity` reports the number of complete scene slots that fit.
- `capabilities.scenes` is true only when the full compiled scene count fits.

`persistence.detail` is `ready` after initialization or a successful commit.
Failure values identify the failed phase (for example `mount_failed`,
`eeprom_migration_failed`, `commit_blob_write_failed`,
`commit_meta_write_failed`, or `commit_verify_failed`). It is diagnostic data:
hosts must continue to use `persistence.status` as the write-safety gate.

## Apply Receipt

After durable Apply, firmware returns `applied_checksum` and `storage_generation`. The checksum is a device-owned digest of the applied configuration and the generation identifies the committed storage epoch. The App and Bridge must verify authoritative readback before reporting success when the receipt is absent, malformed, or differs from the transmitted candidate.

The current digest includes slots, profiles, per-slot ARG configuration, both fixed LFO lanes per slot, operating and LED modes, filters, envelope baselines, active profile, and USB MIDI state. LFO flags and signed amounts are hashed as canonical semantic fields; derived extension CRCs and object padding are excluded. It is a device-owned receipt, not yet a portable serialization checksum: host implementations must not reproduce it by hashing compiler object representations.

Schema migration is cumulative. In particular, a direct schema-6 to schema-8 boot converts the embedded historical slot layouts in profiles, the macro, and every scene before creating empty schema-8 modulation-extension blocks. Existing ARG, macro, and scene values are preserved; newly introduced fixed LFO lanes default to disabled.

The storage-region map and schema-6/schema-7 tail-relocation arithmetic are shared with the hardware-free `native_persistence` test lane. That lane also executes profile-modulation ARG/LFO sanitization, compact packing, and CRC coverage over semantic slot bytes. These executable calculations guard current layout drift, but they do not replace the separate requirement for a frozen byte image captured from genuine schema-6 firmware.

Legacy migration reports an explicit result for capacity, read, write, and verification failures. On the production backend it runs against the inactive transactional generation and commits only after relocation, conversion, clearing, and verified semantic writes all succeed. `CONFIG_VERSION` is promoted last; a failure aborts the inactive generation and leaves the historical committed image available for a later retry rather than treating a partial conversion as current.

Profile loads compose the ordinary profile snapshot and its schema-8 modulation extension in memory before persistence. Each resulting slot is saved once, preventing an intermediate MIDI/EF-only slot image and avoiding the former double-write pass across all 42 slots.

The historical schema-4 emulated EEPROM offsets are documented separately in [Legacy EEPROM Layout](EEPROMLayout.md) and are not the current persistence contract.
