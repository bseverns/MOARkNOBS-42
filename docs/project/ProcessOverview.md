# Process Overview

## Hardware Prep
Before the microcontroller sings, the chassis and wiring need love. Lay out the board, solder the muxes, and double-check every rail so nothing smokes on first power. Full build gospel lives in the [Builder's Handbook](../getting-started/BuildersHandbook.md).

## Firmware Flash
Once the hardware is solid, feed the Teensy the latest brain dump. PlatformIO handles the compile and upload, and the firmware README maps all the build flags for custom targets. See the [firmware README](https://github.com/bseverns/benzknober/blob/main/firmware/README.md) for the gory details.

## Bridge Setup
To chat with browsers or hurl OSC, the Node bridge slings serial data into the right sockets. Install dependencies, fire it up, and you've got WebSerial and OSC both talking to the board. The [bridge README](https://github.com/bseverns/benzknober/blob/main/bridge/README.md) walks through the whole circus.

## Test Run
Don't trust a build until it survives a gauntlet. Run the Unity tests and poke every control until the rig squeals the right bytes. The [testing ritual](../validation/TESTING.md) documents the full suite from polite to destructive.

## Release Steps
When the build is battle-tested, tag the code, pack the gerbers, and write up what changed so others can reproduce it. The [release guide](../release/ReleaseGuide.md) spells out the full release grind.

## Hands-on lab: tweak a slot, ship it, watch it scream

The fastest way to learn the pipeline is to change something tiny and shove it through every stage. This lab keeps it spicy but beginner-proof—perfect for mentoring or self-study.

1. **Edit a default in `firmware/src/`**
   - Pop open [firmware/src/ConfigManager.cpp](https://github.com/bseverns/benzknober/blob/main/firmware/src/ConfigManager.cpp) and scroll to the slot defaults called out in the [Annotated Source Field Guide](https://github.com/bseverns/benzknober/blob/main/README.md#annotated-source-field-guide). Those `SlotARGConfig` and `SlotEnvelopePayload` defaults are where the checksum/rollback story begins.
   - Nudge one of the default values (e.g., flip a `SlotARGConfig` method or tweak the `SlotEnvelopePayload` filter type) so you can spot the change downstream. Keep the edit small so you can track it end-to-end.

2. **Compile + flash with the Quick Start incantation**
   - From repo root, build and upload with: `pio run -t upload -d firmware -e teensy40_main`.
   - Teensy on USB, PlatformIO on PATH—same ritual as the main [Quick Start](https://github.com/bseverns/benzknober/blob/main/README.md#quick-start). If the board is busy, kill any other serial monitors first.

3. **Push the tweak over WebSerial (sim then hardware)**
   - Launch the WebSerial configurator (see [docs/WebSerial.md](../guides/WebSerial.md)) and start in the simulator mode to confirm your edited default shows up. The simulator runs the same JSON schema but never touches hardware—great for sanity checks.
   - Flip to the real device, hit `HELLO` → `GET_MANIFEST`, and let the app stream. Your changed slot should land with a fresh checksum; if the firmware rejects it, you’ll see rollback chatter in the console. That’s the ConfigManager guardrails doing their job.

4. **Spy on the change stream over OSC**
   - Fire up the bridge CLI: `node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000`.
   - In another shell, watch the outbound updates: `oscdump 9000` (or `oscdump 9000 /mn42/slots` if you want to filter). Twist the slot in WebSerial and you’ll see the OSC patches echo live. If checksum validation rolls you back, the OSC stream shows the revert too.

What you just proved: default tweak → firmware flash → WebSerial push with checksum/rollback → OSC telemetry. That’s the whole loop, no mysticism required.
