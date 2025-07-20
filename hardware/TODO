# MOARkNOBS v2 – Optional / DNI Features Reference

**Purpose:** Central checklist of every *optional*, *Do Not Install (DNI)*, or *future‑proof* footprint and design choice we discussed. Use this before layout freeze, during BOM prep, and for bring‑up decisions.

---

## Legend

| Tag        | Meaning                                                                 |
| ---------- | ----------------------------------------------------------------------- |
| **DNI**    | Footprint placed, *not populated* by default (risk mitigation / tuning) |
| **Opt**    | Populate only if condition applies (need / measurement driven)          |
| **Future** | Reserved for later revision (v3+)                                       |
| **Alt**    | Alternative part / topology decision point                              |

---

## 1. Power & Protection

| Item                                          | Ref / Net              | Status            | Notes                                                                                                        |
| --------------------------------------------- | ---------------------- | ----------------- | ------------------------------------------------------------------------------------------------------------ |
| Logic PTC fuse                                | F1 (0.5 A hold)        | Installed         | Mandatory protection                                                                                         |
| LED rail PTC fuse                             | F2 / U6 (2–2.5 A hold) | Installed         | Mandatory for LED chain                                                                                      |
| TVS diode                                     | D44 SA6.0A             | Installed         | Protects VIN\_FUSED (≥6 V clamp)                                                                             |
| Reverse / backfeed isolation (USB vs DC jack) | Schottky or note       | **Opt**           | Add diode or explicit silkscreen warning if not implemented                                                  |
| Per‑LED 0.1 µF decoupling caps                | C\_perLED\_x           | **DNI** / **Opt** | Provide footprints every LED or at least start/mid/end; populate if signal integrity / color glitches appear |
| Additional bulk cap on VLED (extra 220 µF)    | C\_LED\_EXTRA          | **Opt**           | Populate if brownouts when many LEDs white                                                                   |

---

## 2. I²C / Display Subsystem

| Item                      | Ref                             | Status     | Notes                                                                |
| ------------------------- | ------------------------------- | ---------- | -------------------------------------------------------------------- |
| External I²C pull-ups     | R\_I2C1, R\_I2C2 (4.7 kΩ)       | **Opt**    | Populate if OLED module pull-ups removed or weak; pull to 3.3 V only |
| Series edge damping       | R\_SDA, R\_SCL (22–33 Ω)        | **DNI**    | Populate if ringing / overshoot on scope                             |
| Alternate address jumper  | SJ\_ADDR                        | **Future** | If second display or address conflict later                          |
| OLED VCC 5 V support path | Net note                        | **Opt**    | Default to 3.3 V; power at 5 V only after verifying pull-up levels   |
| Test header (debug)       | J\_I2C\_DEBUG (GND,3V3,SDA,SCL) | Installed  | For logic analyzer / expansion                                       |

---

## 3. LED Data Path

| Item                                   | Ref                  | Status    | Notes                                                                                                   |
| -------------------------------------- | -------------------- | --------- | ------------------------------------------------------------------------------------------------------- |
| Series resistor                        | R20 (33–100 Ω)       | Installed | Damps edges (keep)                                                                                      |
| Shunt cap (data line)                  | C\_LEDD (100–220 pF) | **DNI**   | Only if EMI or overshoot persists after good routing                                                    |
| Buffer/level shifter (AHCT125 channel) | U8.x channel         | **Opt**   | Use if long external LED cable / signal degradation; otherwise may tie OE high and leave channel unused |
| Unused buffer channels tie‑off         | U8 unused inputs/OE  | Required  | Inputs → GND, OE → VCC (disable)                                                                        |

---

## 4. Matrix & Control MUX

| Item                             | Ref                                   | Status     | Notes                                                              |
| -------------------------------- | ------------------------------------- | ---------- | ------------------------------------------------------------------ |
| External column pull-up          | R\_COLPU (47 kΩ)                      | **DNI**    | Populate if internal pull-up proves unstable or slow               |
| Column RC filter                 | R\_COL (330–1k) + C\_COL (100–220 pF) | **DNI**    | Add if switch bounce causes false reads; RC adds propagation delay |
| CTRL ADC RC filter               | R\_CTRL (100 Ω) + C\_CTRL (10–47 nF)  | **DNI**    | Populate if analog jitter on pots/buttons excessive                |
| Spare MUX B channels             | CH6, CH7, CH14–15                     | **Future** | Label SPARE\_x; test pads added                                    |
| Pull-down/up on MUX enable (/EN) | R\_MUX\_EN (100 kΩ)                   | **DNI**    | Only if enable pin floats (depends on symbol)                      |

---

## 5. Potentiometers (via MUX Option)

| Item                        | Ref                       | Status  | Notes                                   |
| --------------------------- | ------------------------- | ------- | --------------------------------------- |
| Series isolation at ADC pin | R\_ADC\_ISO (100 Ω)       | **DNI** | Populate if multiplexed settling noise  |
| Shared smoothing cap        | C\_ADC\_SMOOTH (10–47 nF) | **DNI** | Only if required; increases settle time |

---

## 6. Envelope Followers (EF) – Option A Architecture

| Item                                   | Ref                                                     | Status    | Notes                                                                                             |
| -------------------------------------- | ------------------------------------------------------- | --------- | ------------------------------------------------------------------------------------------------- |
| Op-amp choice                          | MCP6002 vs TLV/TLV9xxx                                  | **Alt**   | Choose rail-to-rail dual (3 pcs for 6 ch). TL072 **deprecated** in low-voltage single-supply role |
| Mid-rail reference divider             | R\_VREF1, R\_VREF2 (100 kΩ) + C\_VREF (4.7 µF + 100 nF) | Installed | Shared node VREF ≈ 1.65 V                                                                         |
| Input AC coupling                      | C\_INn (100 nF)                                         | Installed | Increase for more LF response (e.g., 1 µF)                                                        |
| Gain resistors                         | R\_INn, R\_Fn (100 kΩ)                                  | Installed | Adjust ratio for pre-scaling if needed                                                            |
| Attack resistor                        | R\_A\_n (4.7 kΩ)                                        | Installed | Adjust for attack time (τ = R\_A × C\_ENV)                                                        |
| Release resistor                       | R\_R\_n (20 kΩ)                                         | Installed | τ\_release = R\_R × C\_ENV                                                                        |
| Envelope cap                           | C\_ENVn (1.0 µF)                                        | Installed | Increase to reduce ripple (scale resistors)                                                       |
| Output series to ADC                   | R\_OUTn (1 kΩ)                                          | Installed | Source isolation & clamp current limit                                                            |
| Clamp diodes                           | D\_CLAMPn+ / D\_CLAMPn– (1N4148)                        | **DNI**   | Populate if over/under-shoot risk or external CV overvoltage                                      |
| Alternate ground-ref follower topology | Entire block                                            | **Alt**   | Not selected; would remove VREF offset processing                                                 |

---

## 7. MIDI

| Item                              | Ref                                      | Status     | Notes                                                     |
| --------------------------------- | ---------------------------------------- | ---------- | --------------------------------------------------------- |
| MIDI IN extra pull-up adjustments | (N/A)                                    | —          | Only standard pull-up to 3.3 V required                   |
| Optocoupler output RC filter      | R\_MIDI\_OUT (series) + C\_MIDI (to GND) | **DNI**    | Add only if edge ringing / EMI on very long cables        |
| MIDI Activity LED                 | LED\_ACT + R\_LED\_ACT                   | **Future** | Optional status indicator driver off TX or opto collector |

---

## 8. I/O & Expansion / Debug

| Item                   | Ref                                             | Status           | Notes                                     |
| ---------------------- | ----------------------------------------------- | ---------------- | ----------------------------------------- |
| SPI future header      | J\_SPI\_FUT (1×6 or 2×5)                        | **Future / DNI** | Reserve pins 10–13 + CS; not populated v2 |
| UART debug header      | J\_UART\_DBG (GND, TX, RX)                      | **Future**       | Only if spare serial planned              |
| Power probe header     | J\_PWR\_PROBE (VIN\_RAW, VIN\_FUSED, VLED, 3V3) | **Opt**          | Convenience for voltage drop measurements |
| Test pads (signals)    | TP\_\* (see list)                               | Installed        | Bring-up instrumentation                  |
| Optional VREF test pad | TP\_VREF                                        | **Opt**          | Useful for baseline calibration           |

### Test Pad Inventory (Planned)

TP\_VINRAW, TP\_VINFUSE, TP\_VLED, TP\_3V3 (x2), TP\_GND (per cluster), TP\_ROWDRV, TP\_ROWx (e.g., ROW3), TP\_COL, TP\_CTRLADC, TP\_LEDD, TP\_MIDITX, TP\_MIDIRX, TP\_E1, TP\_E4, TP\_SDA, TP\_SCL, TP\_SPARE6, TP\_SPARE7, (Optional) TP\_VREF.

---

## 9. Protection & Clamping (General)

| Item                                       | Ref                     | Status     | Notes                                            |
| ------------------------------------------ | ----------------------- | ---------- | ------------------------------------------------ |
| ADC line clamp (shared)                    | D\_ADC\_UP / D\_ADC\_DN | **DNI**    | If multiple external unknown CV sources expected |
| Series resistors on sensitive analog lines | R\_SER\_ENV (100 Ω)     | **DNI**    | Add if digital feedthrough observed              |
| ESD TVS for MIDI connectors                | D\_TVS\_MIDI            | **Future** | If field reliability issues arise                |

---

## 10. Mechanical / Display

| Item                        | Ref             | Status     | Notes                                   |
| --------------------------- | --------------- | ---------- | --------------------------------------- |
| OLED Mounting holes plating | Hx              | **Opt**    | Plate & tie to GND for shield else NPTH |
| Keepout under OLED          | Mech layer note | Installed  | Prevent tall components underneath      |
| Address pad/jumper          | SJ\_ADDR        | **Future** | For second display / address conflict   |

---

## 11. Firmware-Dependent Options (Documented in Hardware)

| Item                                      | Hardware Ref     | Status  | Notes                                           |
| ----------------------------------------- | ---------------- | ------- | ----------------------------------------------- |
| Internal pull-ups only for column sense   | COL\_SENSE pin 7 | Current | External R\_COLPU available if needed           |
| Baseline subtraction for EF               | VREF node        | Current | Firmware must sample TP\_VREF at boot           |
| Analog vs digital read multiplex strategy | MUX B channels   | Current | Single analog read path; thresholds for buttons |

---

## 12. Alternate Component Choices Summary

| Function         | Default             | Alt (Why)                                  | Populate Criteria              |
| ---------------- | ------------------- | ------------------------------------------ | ------------------------------ |
| EF op-amp        | MCP6002             | TLV9062 (higher BW), OPA2314 (lower noise) | Need higher slew / lower noise |
| LED data buffer  | Direct (AHCT125 ch) | 74HCT1G125 (single gate)                   | Board space reduction later    |
| I²C pull-ups     | 4.7 kΩ              | 2.2 kΩ (long bus), 10 kΩ (low power)       | Adjust for rise time / power   |
| Attack resistor  | 4.7 kΩ              | 2.2–10 kΩ                                  | Adjust envelope snappiness     |
| Release resistor | 20 kΩ               | 10–100 kΩ                                  | Adjust tail length             |
| Envelope cap     | 1.0 µF              | 0.47–4.7 µF                                | Ripple vs response speed       |

---

## 13. Pre-Layout Optional Footprint Checklist

* [ ] Place all TP\_\* pads (see inventory) near edge.
* [ ] Add R\_SDA / R\_SCL (DNI) in series path.
* [ ] Add R\_COL / C\_COL (DNI) stub adjacent to COL\_SENSE trace.
* [ ] Add R\_CTRL / C\_CTRL (DNI) close to ADC pin 23.
* [ ] Add clamp diode pads for each EF output (DNI).
* [ ] Reserve SPI\_FUT header (DNI) or document omission.
* [ ] Confirm unused AHCT125 inputs tied to GND; OE to VCC or appropriate.
* [ ] Confirm spare MUX channels labeled SPARE\_x and given pads.
* [ ] Add VREF test pad (optional but recommended).

---

## 14. Bring-Up Population Guidance

| Stage                 | Populate                                                         | Leave DNI                                                                       | Measurement Goal                           |
| --------------------- | ---------------------------------------------------------------- | ------------------------------------------------------------------------------- | ------------------------------------------ |
| Initial Power-Up      | Mandatory PTCs, TVS, bulk caps, R20, EF core parts               | All RC filters, clamps, SPI\_FUT, per-LED caps (except start/mid/end if chosen) | Verify rails, matrix baseline, EF baseline |
| Signal Integrity Pass | Add per-LED caps (if flicker), I²C series resistors (if ringing) | Clamps (unless overvoltage seen)                                                | Clean digital edges                        |
| Noise Optimization    | Add R\_CTRL/C\_CTRL, R\_COL/C\_COL as needed                     | Remain off if no jitter                                                         | Reduce analog jitter                       |
| Field Hardening       | Add EF clamps, MIDI ESD TVS                                      | Keep SPI\_FUT if not using                                                      | Robustness under abuse                     |

---

## 15. Revision Log Placeholder

| Rev       | Date   | Changes to Optional/DNI Strategy          |
| --------- | ------ | ----------------------------------------- |
| v2.0 Beta | (Fill) | Initial optional footprint set            |
| v2.1      | (Fill) | e.g., Populated R\_COLPU after noise test |

---

## 16. Quick Reference (Top 10 Fast Checks)

1. VREF node present & stable (divider + cap).
2. AHCT125 unused channels properly tied off.
3. SPI\_FUT header either placed & DNI or documented omission.
4. Column sense external pull-up footprint present (DNI).
5. CTRL ADC optional RC filter footprints present (DNI).
6. Per-LED decoupling footprints at least start/mid/end.
7. Envelope clamp diode footprints (DNI) present.
8. I²C pull-ups defined strategy (on-board vs external).
9. Test pad inventory complete & labeled.
10. USB vs VIN backfeed policy documented.

---

**End of Document**
