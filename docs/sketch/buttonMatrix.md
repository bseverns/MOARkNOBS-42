The button slab leans on **two CD74HC4067s** to jam a 7×6 switch matrix into a lone ADC channel.
One mux picks the row, the other picks the column, and the Teensy 4.0 sniffs the junction through
`buttonMuxAnalogPin`—42 switches, one pin, zero shame.

Feast your eyes on the button matrix! A matching screenshot lives in [PNG_btnBRD_2025-07-22](PNG_btnBRD_2025-07-22).

```mermaid
flowchart LR
  subgraph SlotMatrix["Slot Matrix (42 Buttons)"]
    U1[Teensy 4.0] -->|A0–A5 select| CD1[CD74HC4067 #1]
    CD1 --> Buttons42[42_Buttons]
    CD1 -->|ColRead| Read22[Read_pin_22]
    CD1 -->|RowRead| Read23[Read_pin_23]
  end

  subgraph CtrlMatrix["Control Matrix & Pots"]
    U1 -->|A8–A11 select| CD2[CD74HC4067 #2]
    CD2 --> Ctrl6[6_Switches]
    CD2 --> Pot1[Pot1_Analog]
    CD2 --> Pot2[Pot2_Analog]
    CD2 --> Pot3[Pot3_Analog]
  end
```

### Scan dance

- **Kick a row live.** Drive `PIN_ROW_DRV` high and set `MUXR1..4` to the row index.
- **Sweep the columns.** For each column, wiggle `MUXC1..4` and let the internal pull‑up duke it out.
- **Sniff the analog line.** Sample `buttonMuxAnalogPin` (A4); low voltage means the switch at that row/column is getting mashed.
- **Drop the row and move on.** Pull `PIN_ROW_DRV` low, bump the row counter, and keep cruising.
- **Repeat fast.** Hit all 7×6 combos before the user blinks and they'll think you're psychic.

