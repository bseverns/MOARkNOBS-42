# BTN\_42 Hardware (MOARkNOBS v2)

> Welcome to the heart of the MOARkNOBS controller: the BTN\_42 button board. This document combines a concise hardware overview with in-depth electrical theory, design rationale, and optional/DNI notes to guide you from schematic to debug.

## Directory Contents

* **BOM\_btnBRD\_btnBRD\_2025-04-17.xlsx** – complete bill of materials for v2.
* **PickAndPlace\_btnBRD\_2025-04-17.xlsx** – component placement for automated assembly.
* **Gerber\_btnBRD\_2025-04-17.zip** – fabrication files (2 oz copper, ENIG finish).
* **shell/** – mechanical STEP (in `3DShell_btnBRD/`) and STL (`stl/`) enclosure models.
* **sketch/** – block diagrams and EasyEDA schematic exports (`PNG_btnBRD_2025-07-20/`).

---

## Schematic Sheet Summary

| Sheet # | Title                                  | Contents                                                                                            |
| ------- | -------------------------------------- | --------------------------------------------------------------------------------------------------- |
| 1       | Title / Block Diagram                  | Topology overview; signal & power flow.                                                             |
| 2       | **Power & Protection**                 | DC jack, F1 logic PTC, TVS, bulk caps, F2 LED PTC, VLED cap, on-board regulator note.               |
| 3       | **Teensy Core & Headers**              | Teensy 4.0 pinout subset: GPIO, ADC, I²C, MIDI, LED data, mount holes, reset/boot.                  |
| 4       | **Key Matrix & MUX**                   | 42 switches + diodes; 2×CD74HC4067 row/col scanners; pull-ups and ghost‑prevention.                 |
| 5       | **Level-Shifter & LED / MIDI OUT**     | SN74AHCT125 buffer, RLED 33 Ω, optional per‑LED decouplers, MIDI OUT current loop resistors.        |
| 6       | **MIDI IN Optocoupler + ESD**          | 6N138 (or alternative), series resistors, 1N4148 reverse diode, 3.3 V pull-up, ESD TVS placeholder. |
| 7       | **Envelope Follower Analog Front‑End** | 6 channels: AC coupling, unity‑gain precision rectifier, attack/release RC, VREF mid‑rail bias.     |
| 8       | **Display, UI & Aux Headers**          | SSD1306 OLED (I²C), pull‑ups, debug header, reserved SPI\_FUT (DNI), SWD pads.                      |
| 9       | **Netlist & BOM Cross‑Reference**      | Consolidated nets, optional/DNI footprints checklist (`Optional_DNI_Reference.md`).                 |

---

## Detailed Sheet Insights

### 1. Title & Block Diagram

Provides a macro view of power domains, scan loops, and UI connections.
**Why it matters:** Early visualization of power and signal flow prevents layout blind spots.

### 2. Power & Protection

* **DC Jack → PTC F1 (0.5 A)** → VIN\_FUSED → bulk (220 µF + 10 µF) + local decoupling (100 nF).
* **TVS D44 (SA6.0A)** clamps surges ≥ 6 V to GND.
* **LED rail**: separate PTC F2 (2.5 A) + 100 µF cap isolates WS2812 current from logic.

> **Theory:** Resettable fuses and staged capacitance form a low‑pass filter, smoothing supply noise and shielding logic from LED transients.

### 3. Teensy Core & Headers

* Exposes only required pins: digital I/O (matrix/MUX), ADC (pots/envelopes), I²C (display), UART/MIDI, LED data.
* Mounting holes and boot/reset nets clearly labeled.

> **Theory:** Minimizing pin use reduces congestion. Grouping similar functions at board edges streamlines harness routing.

### 4. Key Matrix & MUX

* **7×6 diode matrix** of B3F switches. Row lines (MUXR1..4) and column lines (MUXC1..4) route into two CD74HC4067 multiplexers.
* Firmware cycles these lines with a `setMux()` helper, scanning all 42 buttons through a single analog sense pin.
* Single pull-up resistor on column sense; analog read on control MUX.

> **Theory:** Multiplexing slashes GPIO count. Diode isolation prevents ghosting; settle delays in firmware ensure stable reads.

### 5. Level‑Shifter & LED / MIDI OUT

* **SN74AHCT125** buffers LED data (800 kHz) and MIDI OUT loops.
* **RLED (33 Ω)** damps reflections; optional C\_LEDD footprints near LEDs (DNI).
* **220 Ω resistors** conform to MIDI spec.

> **Theory:** Controlled edge rates preserve signal integrity over flex or long ribbon cables; decoupling and series R form a matched‑impedance path.

### 6. MIDI IN Optocoupler + ESD

* **6N138** opto isolates incoming MIDI; 220 Ω series input resistor(s) plus 1N4148 reverse diode.
* Collector pull-up to 3.3 V; ESD TVS footprint (future) across DIN pins.

> **Theory:** Isolation breaks ground loops on stage. Pull-ups and diodes must support 31.25 kBd and ±7 kV ESD.

### 7. Envelope Follower Analog Front‑End

* **Option A**: single-supply precision rectifier using rail‑to‑rail op-amp (e.g., MCP6002), biased at **VREF ≈ 1.65 V**.
* **R\_IN = R\_F = 100 kΩ**, **R\_A = 4.7 kΩ** (5 ms attack), **R\_R = 20 kΩ** (20 ms release), **C\_ENV = 1 µF**.
* Six channels feed A0, A1, A2, A3, A6 and A7 so audio or CV can modulate any slot.

> **Theory:** Rail‑to‑rail amps handle 0–VCC signals; mid‑rail bias avoids negative swings; precision rectification yields accurate envelope curves.

### 8. Display, UI & Aux Headers

* **SSD1306 OLED** via I²C (GND, 3.3 V, SDA, SCL).
* **4.7 kΩ pull-ups** on SDA/SCL; series resistors (22 Ω) and alternate address jumper (DNI).
* **J\_I2C\_DEBUG** header for logic analyzer; **J\_SPI\_FUT** (DNI) reserves SPI bus for v3.

> **Theory:** I²C rise time is set by pull-up/Rbus; proper termination critical at 400 kHz+ speeds.

### 9. Netlist & BOM Cross‑Reference

* All nets and parts with **DNI / Opt / Future** flags documented in `Optional_DNI_Reference.md`.
* Ensures assembly clarity and prevents unintentional omissions.

---

## Original Design Notes & Rationale

> The initial MN42-1 revision prioritized simplicity: straight‑through switch matrices and TL072‑based envelope followers.  v2 refines each subsystem with focused component choices, advanced theory, and debug flexibility.

* **Early Sketches** captured high-level grouping: power, logic, analog, UI.  Refined into multiple EasyEDA sheets.
* **Dry BOM review** identified critical footprints and alternative parts (AHCT125 vs single-gate buffers, TL072 vs MCP6002, etc.).

---

## Optional / DNI Features

Refer to `sketch/Optional_DNI_Reference.md` for the full checklist of optional footprints, RC networks, and future header reservations.

---

## License

All hardware files are MIT‑licensed. See [LICENSE](../../LICENSE).

---

**Build boldly. Probe thoroughly. May your firmware catch every edge.**
