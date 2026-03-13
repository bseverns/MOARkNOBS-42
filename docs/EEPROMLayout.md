# EEPROM Layout

Schema version: `0x0004`

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
| 0x1F4 | `MIDISlot[42]` | Slot payload arena | 36 bytes per slot (0x1F4–0x7DB) |
| 0x7DC | `float` | Legacy filter frequency mirror | Schema‑4+ global EF freq lives here so old dashboards can keep peeking |
| 0x7E0 | `float` | Legacy filter Q mirror | Schema‑4+ global EF Q without trampling slot 14 |
| 0x7E4 | `uint16` | Brownout counter | Gets bumped when VIN face‑plants; relocated out of the slot arena |
| 0x7E6 | — | Profile 1 block | 256‑byte slice for alt configs (id 1) |
| 0x8E6 | — | Profile 2 block | Another 256‑byte slice (id 2) |

The slot arena scoots out of the way of the config+backup duet so we never
stomp the calibration data again. Think of it as a velvet rope at `0x1F4`:
only the 42 MIDISlots get in, everybody else queues up afterwards. The two
floats parked at `0x7DC`/`0x7E0` are the re-homed “global filter” mirror that
legacy dashboards still sip on. They used to live at `0x3E8`/`0x3EC`, which was
fine until each slot grew to 36 bytes—suddenly every filter tweak was
chainsawing slot 13/14. Moving them to the tail keeps the compat shim alive
without carving up live presets.

Schema `0x0004` fattens each slot record to 36 bytes so the per-slot envelope
payload (filter type + frequency + Q) can live right beside the MIDI guts. On
first boot after the upgrade the firmware copies the legacy global filter
values into every slot and keeps emitting the old global fields so older UIs
don’t choke. Slots still carry the usual suspects—message type, channel, data
byte, EF index, active flag, arpeggiator note, and SysEx template metadata—but
now they also remember how their follower was tuned.

We also punted the brownout counter to `0x7E4`. It used to camp at `0x3F0`,
which meant every “hey the PSU dipped” write was stomping slot 14’s header.
The new spot keeps the telemetry alive without graffitiing the arena. If the
new address boots up erased (`0xFFFF`), the firmware zeros it and moves on; no
more surprise corruption when the power grid burps.

In code, that velvet rope shows up as `EEPROM_SLOT_BASE`, which equals the full
mirrored config span (`EEPROM_CONFIG_MIRROR_SIZE = 0x1F4`). The slots now chew
through `EEPROM_SLOT_REGION_SIZE` (1512 bytes for 42×36) before
`EEPROM_PROFILE_START(1)` kicks in at `0x7E6`. Profiles still march forward in
tight `EEPROM_PROFILE_BLOCK_SIZE` (256 byte) chunks beyond that point.

For the gory details, the code comments in [firmware/include/ConfigManager.h](https://github.com/bseverns/benzknober/blob/main/firmware/include/ConfigManager.h) spill every byte. This table just keeps the map close at hand.
