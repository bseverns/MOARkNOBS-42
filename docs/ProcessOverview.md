# Process Overview

## Hardware Prep
Before the microcontroller sings, the chassis and wiring need love. Lay out the board, solder the muxes, and double-check every rail so nothing smokes on first power. Full build gospel lives in the [Builder's Handbook](BuildersHandbook.md).

## Firmware Flash
Once the hardware is solid, feed the Teensy the latest brain dump. PlatformIO handles the compile and upload, and the firmware README maps all the build flags for custom targets. See the [firmware README](../firmware/README.md) for the gory details.

## Bridge Setup
To chat with browsers or hurl OSC, the Node bridge slings serial data into the right sockets. Install dependencies, fire it up, and you've got WebSerial and OSC both talking to the board. The [bridge README](../bridge/README.md) walks through the whole circus.

## Test Run
Don't trust a build until it survives a gauntlet. Run the Unity tests and poke every control until the rig squeals the right bytes. The [testing ritual](TESTING.md) documents the full suite from polite to destructive.

## Release Steps
When the build is battle-tested, tag the code, pack the gerbers, and write up what changed so others can reproduce it. The [release guide](ReleaseGuide.md) spells out the full release grind.

