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
| 0x0D1 | `uint8[22]` | Buffer | Scratch padding |
| 0x0E7 | — | Backup config block | Mirrors 0x000–0x0E6 |
| 0x19C | — | Profile 1 block | 412‑byte slice |
| 0x338 | — | Profile 2 block | 412‑byte slice |

For the gory details, the code comments in [`firmware/include/ConfigManager.h`](../firmware/include/ConfigManager.h) spill every byte. This table just keeps the map close at hand.

## How the math works out

`ConfigManager` now codifies the arithmetic so you do not have to keep a napkin handy:

- `EEPROM_CONFIG_BYTES` evaluates to `0x0B5` (181 decimal), which is the exact footprint of the data written by `writeEEPROM()` – the pot channel/CC maps plus versioning and CRC tags.
- `EEPROM_BACKUP_START` lands at `0x0E7` (231 decimal) after we tack on the six baseline floats and the 22-byte scratch buffer. That is where the mirrored block begins.
- `EEPROM_PROFILE_BLOCK_SIZE` becomes the stride (`0x19C` / 412 bytes) so every profile slice comfortably holds both the primary payload and its backup twin.

Knowing those three numbers makes it dead simple to sanity-check new fields: update the size in code, recalc the totals, and the profile stride will follow automatically.
