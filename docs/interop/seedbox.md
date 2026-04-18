# SeedBox ↔ MN42 Link Notes

> Notebook entry for the SeedBox handshake. Equal parts playbook and field log so future you (or the next punk in the studio) can wire the rigs together without re-reading the MIDI spec.

This page is a focused interop reference for one external rig path, not a broad host-compatibility claim for the main product surface.

For the general browser/bridge support boundary, see [Host Compatibility](../reference/HostCompatibility.md) and [Connectivity Guide](../getting-started/ConnectivityGuide.md).

## TL;DR

- MN42 now throws a Control Change **14** with value **0x01** on MIDI channel 1 as soon as the firmware boots (`SeedBoxLink::begin`).
- SeedBox answers with **0x11** on the same CC when its subsystems are caffeinated. We remember the ack and treat the session as live.
- Both sides pulse **0x7F** every few seconds. Skip two pulses (~8 s) and the firmware falls back to sending the boot hello again.
- Identity flex? The firmware coughs up `F0 7D 4D 4E 42 01 F7` via `sendIdentityPing()` so SeedBox can ignore look-alikes.

All the literal numbers live in [`firmware/include/interop/mn42_map.h`](https://github.com/bseverns/MOARkNOBS-42/blob/main/firmware/include/interop/mn42_map.h). Copy those constants instead of sprinkling magic values through sketches or scripts.

## MIDI CC Map Cheat Sheet

| CC  | Constant             | Meaning                           | Notes                    |
| --- | -------------------- | --------------------------------- | ------------------------ |
| 14  | `cc::kHandshake`     | Boot hello, ack, keep-alive       | Values in `handshake::*` |
| 15  | `cc::kMode`          | Bitfield of clock + debug toggles | Bits in `mode::*`        |
| 16  | `cc::kSeedMorph`     | Morph between saved seeds         | 0–127 ramp               |
| 17  | `cc::kTransportGate` | Momentary transport gate          | 0 = open, >0 = closed    |

Mode flag reference (same layout SeedBox ships with):

| Bitmask | Constant                     | Story                                                    |
| ------- | ---------------------------- | -------------------------------------------------------- |
| `0x01`  | `mode::kFollowExternalClock` | SeedBox is clock boss. MN42 follows.                     |
| `0x02`  | `mode::kExposeDebugMeters`   | SeedBox dumps raw meters for scope voyeurs.              |
| `0x04`  | `mode::kArpAccent`           | Let the accent lane punch above its weight.              |
| `0x08`  | `mode::kLatchTransport`      | Treat transport as a toggle instead of a momentary gate. |

## Firmware plumbing

`SeedBoxLink` lives under `firmware/src/interop/`. It wraps the MIDI send/receive routines so we can keep the handshake logic out of `firmware_main.cpp`.

### Boot

1. `SeedBoxLink::begin()` stores the shared `MIDIHandler`, fires the hello CC, and coughs the identity SysEx.
2. Low-priority scheduler (500 ms tick) calls `SeedBoxLink::update()`.
3. Without an ack we retry the hello every two seconds. After the ack lands we switch to keep-alive pulses every three seconds.

### Incoming data

- `MIDIHandler::handleMIDI()` routes CC 14 traffic into `SeedBoxLink::handleControlChange()`.
- A fresh ack flips `_hasAck` true and resets the peer-alive timer.
- Keep-alives just bump the timer. If the timer rolls beyond eight seconds we log the dropout and go back to the boot hello.
- SysEx packets tagged with the non-commercial ID (`0x7D`) and the `MNB` signature count as activity, which keeps the session alive when SeedBox wants to flex identity again.

### Helpers worth knowing

- `SeedBoxLink::peerAlive()` returns whether the last ack/keep-alive landed inside the timeout window. Use it if you want LEDs or UI banners reacting to the SeedBox presence.
- `SeedBoxLink::handleControlChange()` also answers SeedBox hellos with an ack. That covers reconnection cases when SeedBox is the one rebooting.

## Testing riffs

- **Unit test harness**: When you stub MIDI in tests, feed a CC 14/0x11 pair at the handler and assert `SeedBoxLink::peerAlive()` flips true. Follow with a simulated `now()` jump past eight seconds to confirm the timeout path fires a new hello.
- **Bench dry run**: Connect MN42 and SeedBox, sniff the MIDI stream (USB or DIN). You should see `0x0E 0x0E 0x01` (hello), `0x0E 0x0E 0x11` (ack), and keep-alive pulses every ~3 s once both sides are stable.
- **Identity check**: Watch for the `F0 7D 4D 4E 42 01 F7` SysEx burst on boot. If SeedBox filters on manufacturer ID + signature, this keeps random controllers from barging in.

## Future scribbles

- Sync the mode bit expectations with whatever SeedBox toggles when it exposes debug meters; right now we only document the table.
- Consider mirroring `peerAlive()` into the OLED status area so performers get a “SeedBox present” badge without opening a laptop.
- If we ever expose more CCs for interop, document the additions here and extend `mn42_map.h` so both repos stay in lockstep.
