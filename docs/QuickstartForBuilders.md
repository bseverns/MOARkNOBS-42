# Quickstart for Builders

This is the shortest path from checkout to first bring-up.

## What this project is

MOARkNOBS-42 is a Teensy-based MIDI/OSC controller with onboard controls, LED feedback, profile storage, and browser-based configuration.

## Before you start

- A MOARkNOBS-42 board or in-progress build
- A Teensy 4.0
- A USB cable with data lines
- PlatformIO installed
- A bench power/setup workflow you trust

## 1. Check the current hardware status

Before ordering boards or parts, read [Hardware Current Build](HardwareCurrentBuild.md).

That page is the canonical source for what hardware files are currently verified in the repo and what is still missing or unverified.

## 2. Flash the firmware

From the repo root:

```bash
pio -d firmware run -t upload -e teensy40_main
```

Or from `firmware/`:

```bash
pio run -t upload -e teensy40_main
```

## 3. Do a minimum bring-up check

1. Power the board over USB.
2. Confirm the Teensy enumerates.
3. Open the browser configurator or a serial terminal.
4. Send `HELLO` and confirm the device answers with `{"hello":"mn42"}`.
5. If the main firmware is up, confirm basic LED/button behavior before closing the enclosure.

## 4. Run the safe firmware test gate

From the repo root:

```bash
pio -d firmware test -e teensy40_unity -vvv
```

This is the main automated firmware check. It does not replace bench validation on a real board.

## 5. Run hardware-facing checks when the board is assembled

Use the full-system test environment when you want bring-up help tied to real hardware:

```bash
pio -d firmware run -e teensy40_full_system
```

## 6. If something fails

- Start with [Troubleshooting](Troubleshooting.md).
- Use [Builder's Handbook](BuildersHandbook.md) for wiring and bring-up context.
- Use [Hardware Substitutions](HardwareSubstitutions.md) before swapping parts on instinct.
- Use [Validation Flow](ValidationFlow.md) to decide whether the board is still in bring-up, ready for demo work, or blocked on bench fixes.

## 7. Before calling the board demo-ready

- Run [Demo Test Punch List](DemoTestPunchList.md).
- Record what passed, what was blocked, and what still needs bench validation.

## Builder note

This repo currently documents hardware more clearly than it packages it. If you do not see a verified Gerber zip or versioned BOM in [Hardware Current Build](HardwareCurrentBuild.md), do not infer one from older filenames elsewhere in the docs.
