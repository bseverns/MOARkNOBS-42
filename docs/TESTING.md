# Testing the Firmware

Yeah, we've got tests. Two flavors, two moods:

- `firmware/test/` – quick Unity checks that run without touching a soldering iron.
- `firmware/system_test/` – gritty integration trials that expect a live board on the other end of the USB cable.

## Unity Tests

If you just changed some logic and want proof it still behaves, let Unity scream for you:

```bash
cd firmware
pio test -e teensy40_unity
```

That build only scoops up files in `firmware/test/`, so you're safe to run it on a laptop in a coffee shop.

## System Tests

When you need to poke real silicon, fire up the system tests. They live in `firmware/system_test/` and demand hardware to pass.

```bash
cd firmware
pio test -e teensy40_unity -d system_test
```

Bring a board, a cable, and zero fear. These tests waggle LEDs, trash EEPROM, and generally behave like they own the place.

