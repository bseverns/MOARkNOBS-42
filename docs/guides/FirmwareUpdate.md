# Firmware Update: Keep the Beast Fresh

So you've got a MOARkNOBS-42 that already made it off the bench and into the wild. Here's how to feed it new brain cells without desoldering anything.

## Quick and Dirty

1. **Grab a release** – hit the [GitHub releases page](https://github.com/bseverns/MOARkNOBS-42/releases) and snag the latest `.hex` file.
2. **Jack in** – plug the controller into your machine over USB. Fire up the Teensy Loader GUI or the command-line `teensy_loader_cli`.
3. **Kick it into program mode** – if the loader just stares at you, tap the Teensy's button to nudge the bootloader awake.
4. **Flash it** – feed the hex to the loader or run:
   ```bash
   teensy_loader_cli -mmcu=teensy40 -w mn42-firmware.hex
   ```
5. **Wait for the rave** – the LEDs should chase, the loader will holler `reboot`, and the unit restarts itself.
6. **Smoke test** – power cycle and make sure the slot LEDs still dance and the buttons misbehave.

## Want the Bleeding Edge?

If you crave the latest untagged chaos:

1. Clone the repo and hop into `firmware/`.
2. Build and upload straight from source:
   ```bash
   pio run -d firmware -t upload -e teensy40_main
   ```
3. Put on headphones. Things might get loud.

Happy flashing, and don't blame us if you brick it—just reflash and pretend nothing happened.
