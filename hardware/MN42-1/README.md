# MN42-1 Board Files

> First-run button board with zero chill. Learn it, hack it, make it scream.

## Revision Snapshot

**MN42-1** is the inaugural hardware spin for MOARkNOBS-42. Forty‑two switches, a storm of WS2812s, and a Teensy 4.0 pulling the strings. If you're poking at traces or reworking the layout, this is your launchpad.

## Key Files

- **[BOM_MOAR_MOAR_Board_2025-08-02.xlsx](BOM_MOAR_MOAR_Board_2025-08-02.xlsx)** – every resistor, diode, and shiny trinket spelled out.
- **[Gerber_MOAR_Board_2025-08-02.zip](Gerber_MOAR_Board_2025-08-02.zip)** – drop this on your fab house and watch the copper fly.
- **[shell/](shell/)** – enclosure models. `3DShell_btnBRD` holds STEP files for CAD nerds; `stl/` is ready for your printer.

## Big-Picture Context

Want the whole saga? Jump back to the [hardware overview](../README.md) and see how this misfit board fits into the bigger beast.

## Test Pad Hookups (WIP)

Feeling brave enough to poke the live board with a scope?  This spin needs test pads for the gritty bits:

- `TP_VIN` and `TP_VREF` keep power honest.
- `TP_ROW*` / `TP_COL*` let you spy on the matrix scans.
- `TP_MIDIRX` and `TP_MIDITX` watch the 5‑pin traffic.
- `TP_EF*` taps into the EF triggers.

Each pad is a 1.6 mm SMD landing—big enough for spring clips, small enough to stay out of the way.  After dropping them in your EDA tool of choice, regenerate the Gerbers and BOM so fabrication doesn’t miss the party.

> Heads‑up: the pads aren’t in this repo yet. You’ll have to slam them into the schematic/PCB by hand until someone commits the definitive update.
