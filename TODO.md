# Things I need to do in order for the project to reach demo stage - 7/26/2025

## Pads

- Drop TP_VINRAW, TP_VINFUSED, TP_3V3, TP_VREF, TP_ROWDRV, TP_COLSENSE, TP_SDA/SCL, TP_LED, TP_MIDIRX/TX, TP_E1–E6.

- - 1.6 mm round SMD, no paste, edge-accessible. Name TP_* in schematic.

## BOM sanity

- Lock footprints (0603 vs 0805, SOIC vs TSSOP).

- Mark DNI/OPT rows now so JLC doesn’t place them.

- Check LCSC stock/lead time for MCP6002, AHCT125, PTCs, TVS.

## Routing order

- Power rails + ground plane (star/tie if split).

- High‑speed: LED_DATA, I²C, MIDI. Short, clean, series R in place.

- Analog EF traces away from VLED currents; guard with ground.

- Row/Col buses as tidy bundles.

- Test pads last, near edges.


## LED Integration

- Add controls for the additional 10 ws2812 LEDs