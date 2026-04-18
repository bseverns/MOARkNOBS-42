# MOARkNOBS v2 – Optional / DNI Features Reference

**Purpose:** Central checklist of every _optional_, _Do Not Install (DNI)_, or _future‑proof_ footprint and design choice we discussed. Use this before layout freeze, during BOM prep, and for bring‑up decisions.

---

## Legend

| Tag        | Meaning                                                                 |
| ---------- | ----------------------------------------------------------------------- |
| **DNI**    | Footprint placed, _not populated_ by default (risk mitigation / tuning) |
| **Opt**    | Populate only if condition applies (need / measurement driven)          |
| **Future** | Reserved for later revision (v3+)                                       |
| **Alt**    | Alternative part / topology decision point                              |

---

## 1. Power & Protection

| Item                                          | Ref / Net        | Status            | Notes                                                                                                        |
| --------------------------------------------- | ---------------- | ----------------- | ------------------------------------------------------------------------------------------------------------ |
| Reverse / backfeed isolation (USB vs DC jack) | Schottky or note | **Opt**           | Add diode or explicit silkscreen warning if not implemented                                                  |
| Per‑LED 0.1 µF decoupling caps                | C_perLED_x       | **DNI** / **Opt** | Provide footprints every LED or at least start/mid/end; populate if signal integrity / color glitches appear |
| Additional bulk cap on VLED (extra 220 µF)    | C_LED_EXTRA      | **Opt**           | Populate if brownouts when many LEDs white                                                                   |

---

## 2. I²C / Display Subsystem

| Item                      | Ref                     | Status     | Notes                                                                |
| ------------------------- | ----------------------- | ---------- | -------------------------------------------------------------------- |
| External I²C pull-ups     | R_I2C1, R_I2C2 (4.7 kΩ) | **Opt**    | Populate if OLED module pull-ups removed or weak; pull to 3.3 V only |
| Series edge damping       | R_SDA, R_SCL (22–33 Ω)  | **DNI**    | Populate if ringing / overshoot on scope                             |
| Alternate address jumper  | SJ_ADDR                 | **Future** | If second display or address conflict later                          |
| OLED VCC 5 V support path | Net note                | **Opt**    | Default to 3.3 V; power at 5 V only after verifying pull-up levels   |

---

## 3. LED Data Path

| Item                                   | Ref                 | Status  | Notes                                                                                                   |
| -------------------------------------- | ------------------- | ------- | ------------------------------------------------------------------------------------------------------- |
| Shunt cap (data line)                  | C_LEDD (100–220 pF) | **DNI** | Only if EMI or overshoot persists after good routing                                                    |
| Buffer/level shifter (AHCT125 channel) | U8.x channel        | **Opt** | Use if long external LED cable / signal degradation; otherwise may tie OE high and leave channel unused |

---

## 4. Matrix & Control MUX

| Item                             | Ref                                 | Status     | Notes                                                              |
| -------------------------------- | ----------------------------------- | ---------- | ------------------------------------------------------------------ |
| External column pull-up          | R_COLPU (47 kΩ)                     | **DNI**    | Populate if internal pull-up proves unstable or slow               |
| Column RC filter                 | R_COL (330–1k) + C_COL (100–220 pF) | **DNI**    | Add if switch bounce causes false reads; RC adds propagation delay |
| CTRL ADC RC filter               | R_CTRL (100 Ω) + C_CTRL (10–47 nF)  | **DNI**    | Populate if analog jitter on pots/buttons excessive                |
| Spare MUX B channels             | CH6, CH7, CH14–15                   | **Future** | Label SPARE_x; test pads added                                     |
| Pull-down/up on MUX enable (/EN) | R_MUX_EN (100 kΩ)                   | **DNI**    | Only if enable pin floats (depends on symbol)                      |

---

## 5. Potentiometers (via MUX Option)

| Item                        | Ref                     | Status  | Notes                                   |
| --------------------------- | ----------------------- | ------- | --------------------------------------- |
| Series isolation at ADC pin | R_ADC_ISO (100 Ω)       | **DNI** | Populate if multiplexed settling noise  |
| Shared smoothing cap        | C_ADC_SMOOTH (10–47 nF) | **DNI** | Only if required; increases settle time |

---

## 6. Envelope Followers (EF) – Option A Architecture

| Item                                   | Ref                            | Status  | Notes                                                                                             |
| -------------------------------------- | ------------------------------ | ------- | ------------------------------------------------------------------------------------------------- |
| Op-amp choice                          | MCP6002 vs TLV9062             | **Alt** | Choose rail-to-rail dual (3 pcs for 6 ch). TL072 **deprecated** in low-voltage single-supply role |
| Clamp diodes                           | D_CLAMPn+ / D_CLAMPn– (1N4148) | **DNI** | Populate if over/under-shoot risk or external CV overvoltage                                      |
| Alternate ground-ref follower topology | Entire block                   | **Alt** | Not selected; would remove VREF offset processing                                                 |

---

## 7. MIDI

| Item                              | Ref                                   | Status     | Notes                                                     |
| --------------------------------- | ------------------------------------- | ---------- | --------------------------------------------------------- |
| MIDI IN extra pull-up adjustments | (N/A)                                 | —          | Only standard pull-up to 3.3 V required                   |
| Optocoupler output RC filter      | R_MIDI_OUT (series) + C_MIDI (to GND) | **DNI**    | Add only if edge ringing / EMI on very long cables        |
| MIDI Activity LED                 | LED_ACT + R_LED_ACT                   | **Future** | Optional status indicator driver off TX or opto collector |

---

## 8. I/O & Expansion / Debug

| Item                   | Ref                                         | Status           | Notes                                     |
| ---------------------- | ------------------------------------------- | ---------------- | ----------------------------------------- |
| SPI future header      | J_SPI_FUT (1×6 or 2×5)                      | **Future / DNI** | Reserve pins 10–13 + CS; not populated v2 |
| UART debug header      | J_UART_DBG (GND, TX, RX)                    | **Future**       | Only if spare serial planned              |
| Power probe header     | J_PWR_PROBE (VIN_RAW, VIN_FUSED, VLED, 3V3) | **Opt**          | Convenience for voltage drop measurements |
| Optional VREF test pad | TP_VREF                                     | **Opt**          | Useful for baseline calibration           |

### Test Pad Inventory (Planned)

TP_VINRAW, TP_VINFUSE, TP_VLED, TP_3V3 (x2), TP_GND (per cluster), TP_ROWDRV, TP_ROWx (e.g., ROW3), TP_COL, TP_CTRLADC, TP_LEDD, TP_MIDITX, TP_MIDIRX, TP_E1, TP_E4, TP_SDA, TP_SCL, TP_SPARE6, TP_SPARE7, (Optional) TP_VREF.

---

## 9. Protection & Clamping (General)

| Item                                       | Ref                 | Status     | Notes                                            |
| ------------------------------------------ | ------------------- | ---------- | ------------------------------------------------ |
| ADC line clamp (shared)                    | D_ADC_UP / D_ADC_DN | **DNI**    | If multiple external unknown CV sources expected |
| Series resistors on sensitive analog lines | R_SER_ENV (100 Ω)   | **DNI**    | Add if digital feedthrough observed              |
| ESD TVS for MIDI connectors                | D_TVS_MIDI          | **Future** | If field reliability issues arise                |

---

## 10. Mechanical / Display

| Item                        | Ref     | Status     | Notes                                   |
| --------------------------- | ------- | ---------- | --------------------------------------- |
| OLED Mounting holes plating | Hx      | **Opt**    | Plate & tie to GND for shield else NPTH |
| Address pad/jumper          | SJ_ADDR | **Future** | For second display / address conflict   |

---

## 11. Alternate Component Choices Summary

| Function        | Default             | Alt (Why)                                  | Populate Criteria              |
| --------------- | ------------------- | ------------------------------------------ | ------------------------------ |
| EF op-amp       | MCP6002             | TLV9062 (higher BW), OPA2314 (lower noise) | Need higher slew / lower noise |
| LED data buffer | Direct (AHCT125 ch) | 74HCT1G125 (single gate)                   | Board space reduction later    |
| I²C pull-ups    | 4.7 kΩ              | 2.2 kΩ (long bus), 10 kΩ (low power)       | Adjust for rise time / power   |

---

## 12. Pre-Layout Optional Footprint Checklist

- [ ] Place all TP\_\* pads (see inventory) near edge.
- [ ] Add R_SDA / R_SCL (DNI) in series path.
- [ ] Add R_COL / C_COL (DNI) stub adjacent to COL_SENSE trace.
- [ ] Add R_CTRL / C_CTRL (DNI) close to ADC pin 23.
- [ ] Add clamp diode pads for each EF output (DNI).
- [ ] Reserve SPI_FUT header (DNI) or document omission.
- [ ] Confirm unused AHCT125 inputs tied to GND; OE to VCC or appropriate.
- [ ] Confirm spare MUX channels labeled SPARE_x and given pads.
- [ ] Add VREF test pad (optional but recommended).

---

## 13. Bring-Up Population Guidance

| Stage                 | Populate                                                         | Leave DNI                                                                      | Measurement Goal                           |
| --------------------- | ---------------------------------------------------------------- | ------------------------------------------------------------------------------ | ------------------------------------------ |
| Initial Power-Up      | Mandatory PTCs, TVS, bulk caps, R20, EF core parts               | All RC filters, clamps, SPI_FUT, per-LED caps (except start/mid/end if chosen) | Verify rails, matrix baseline, EF baseline |
| Signal Integrity Pass | Add per-LED caps (if flicker), I²C series resistors (if ringing) | Clamps (unless overvoltage seen)                                               | Clean digital edges                        |
| Noise Optimization    | Add R_CTRL/C_CTRL, R_COL/C_COL as needed                         | Remain off if no jitter                                                        | Reduce analog jitter                       |
| Field Hardening       | Add EF clamps, MIDI ESD TVS                                      | Keep SPI_FUT if not using                                                      | Robustness under abuse                     |

---

## 14. Revision Log Placeholder

| Rev       | Date   | Changes to Optional/DNI Strategy         |
| --------- | ------ | ---------------------------------------- |
| v2.0 Beta | (Fill) | Initial optional footprint set           |
| v2.1      | (Fill) | e.g., Populated R_COLPU after noise test |

---

## 15. Quick Reference (Top 10 Fast Checks)

1. VREF node present & stable (divider + cap).
2. AHCT125 unused channels properly tied off.
3. SPI_FUT header either placed & DNI or documented omission.
4. Column sense external pull-up footprint present (DNI).
5. CTRL ADC optional RC filter footprints present (DNI).
6. Per-LED decoupling footprints at least start/mid/end.
7. Envelope clamp diode footprints (DNI) present.
8. I²C pull-ups defined strategy (on-board vs external).
9. Test pad inventory complete & labeled.
10. USB vs VIN backfeed policy documented.

---

**End of Document**
