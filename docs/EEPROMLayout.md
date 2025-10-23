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
| 0x0E7 | — | Backup config block | Mirrors 0x000–0x0B4 (ends at 0x19B) |
| 0x19C | `MIDISlot[42]` | Slot payload arena | 6 bytes per slot (0x19C–0x297) |
| 0x298 | — | Profile 1 block | 412‑byte slice for alt configs (id 1) |
| 0x434 | — | Profile 2 block | Another 412‑byte slice (id 2) |

The slot arena scoots out of the way of the config+backup duet so we never
stomp the calibration data again. Think of it as a velvet rope at `0x19C`:
only the 42 MIDISlots get in, everybody else queues up afterwards.

In code, that velvet rope shows up as `EEPROM_SLOT_BASE`, which now equals the
full mirrored config span (`EEPROM_CONFIG_MIRROR_SIZE = 0x19C`). The slots chew
through `EEPROM_SLOT_REGION_SIZE` (252 bytes for 42×6) before
`EEPROM_PROFILE_START(1)` kicks in at `0x298`. Profiles march forward in tidy
`EEPROM_PROFILE_BLOCK_SIZE` (412 byte) chunks beyond that point.

For the gory details, the code comments in [`firmware/include/ConfigManager.h`](../firmware/include/ConfigManager.h) spill every byte. This table just keeps the map close at hand.
