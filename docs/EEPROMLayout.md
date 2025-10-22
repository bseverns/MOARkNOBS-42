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
| 0x0C8 | `uint16` | Primary magic | `0xABCD` when sane |
| 0x0CA | `uint16` | Backup magic | `0xDCBA` mirror tag |
| 0x0CC | `float[6]` | Envelope baselines | Learned silence |
| 0x0E4 | `uint8[22]` | Buffer | Scratch padding |
| 0x0FA | — | Backup config block | Mirrors 0x000–0x0F9 (ends at 0x1F3) |
| 0x1F4 | `MIDISlot[42]` | Slot payload arena | 6 bytes per slot (0x1F4–0x2EF) |
| 0x2F0 | — | Profile 1 block | 256‑byte slice for alt configs (id 1) |
| 0x3F0 | — | Profile 2 block | Another 256‑byte slice (id 2) |

The slot arena scoots out of the way of the config+backup duet so we never
stomp the calibration data again. Think of it as a velvet rope at `0x1F4`:
only the 42 MIDISlots get in, everybody else queues up afterwards.

For the gory details, the code comments in [`firmware/include/ConfigManager.h`](../firmware/include/ConfigManager.h) spill every byte. This table just keeps the map close at hand.
