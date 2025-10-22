# EEPROM Layout

Schema version: `0x0002`

| Offset (hex) | Type / Size | Usage | Notes |
| ------------ | ----------- | ----- | ----- |
| 0x000 | `uint8[42]` | Pot channel map | MIDI channel per pot |
| 0x02A | `uint8[42]` | Pot CC map | CC number per pot |
| 0x054 | `uint8[42]` | Envelope assignments | Pot→envelope routing |
| 0x07E | `uint8[42]` | Envelope types | Follower mode per pot |
| 0x0A8 | `uint8` | LED brightness | 0–255 |
| 0x0A9 | `uint8[3]` | LED colour (RGB) | Order: R,G,B |
| 0x0AC | `uint8` | ARG mode | Routing scheme |
| 0x0AD | `uint8` | ARG method | Blend math |
| 0x0AE | `uint8` | ARG env A | First envelope index |
| 0x0AF | `uint8` | ARG env B | Second envelope index |
| 0x0B0 | `uint8` | ARG enable | Non‑zero enables ARG |
| 0x0B1 | `uint16` | Config version | `CONFIG_VERSION` |
| 0x0B3 | `uint16` | CRC | Integrity check |
| 0x0B5 | `uint16` | Primary magic | `0xABCD` when sane |
| 0x0B7 | `uint16` | Backup magic | `0xDCBA` mirror tag |
| 0x0B9 | `float[6]` | Envelope baselines | Learned silence |
| 0x0D1 | `uint8[22]` | Buffer | `EEPROM_CONFIG_SCRATCH_SIZE` scratch padding |
| 0x0E7 | — | Backup config block | Mirrors 0x000–0x0E6 |
| 0x19C | `MIDISlot[42]` | Slot payload arena | 6 bytes per slot (0x19C–0x297) |
| 0x298 | — | Profile 1 block | 412‑byte slice |
| 0x434 | — | Profile 2 block | 412‑byte slice |
| 0x5D0 | `float` | Filter freq | Last tuned EF frequency |
| 0x5D4 | `float` | Filter Q | Last tuned EF resonance |
| 0x5D8 | `uint16` | Brown-out counter | Power-sag survivor count |

For the gory details, [`firmware/include/Globals.h`](../firmware/include/Globals.h) now owns the canonical constants and [`ConfigManager`](../firmware/include/ConfigManager.h) hangs a `static_assert` to make sure profile math never trespasses the filter/brown-out stash. This table just keeps the map close at hand.

## How the math works out

`ConfigManager` now codifies the arithmetic so you do not have to keep a napkin handy:

- `EEPROM_CONFIG_BYTES` still evaluates to `0x0B5` (181 decimal), which is the exact footprint of the data written by `writeEEPROM()` – the pot channel/CC maps plus versioning and CRC tags.
- `EEPROM_BACKUP_START` lands at `0x0E7` (231 decimal) after we tack on the six baseline floats and the 22-byte scratch buffer. That is where the mirrored block begins.
- `EEPROM_CONFIG_MIRROR_SIZE` (a new helper) resolves to `0x19C`, matching the combined primary+backup footprint. We peg `EEPROM_SLOT_BASE` to that value so the 42 `MIDISlot`s live in their own gated corral at `0x19C–0x297` instead of trampling the pot maps.
- `EEPROM_PROFILE_START(1)` now jumps to `0x298` (slot arena + mirror size) and each subsequent call marches forward by `EEPROM_PROFILE_BLOCK_SIZE` (`0x19C`). With `EEPROM_PROFILE_COUNT = 3`, the last byte claimed by a profile is `EEPROM_PROFILE_ARENA_END = 0x5D0`, and the filter/Q/brown-out trio are chained immediately after.

Knowing those numbers makes it dead simple to sanity-check new fields: bump the sizes, watch `EEPROM_CONFIG_MIRROR_SIZE` and the profile arena grow, and the filter scratchpad will automatically leap forward so nothing gets stomped.

If you prefer the poetic version: the mirrored config chews through `0x19C` bytes, the 42-slot mosh pit takes another `0xFC`, and only then do we roll out the velvet rope for alternative profiles. That rope sits at `EEPROM_SLOT_REGION_END`, and everything north of it is fair game for future profile experiments.
