This page maps how the Teensy 4.0 gets its juice and how its pins escape the board.
A 5 V feed hits the onboard regulator for 3 V3, while I²C and SWD signals fan out to headers for hacking.
Never back‑feed the 3 V3 rail through the expander unless you like smoke.

**References**
- [Schematic](../../Interface&Cntrl.png)
- [Teensy 4.0 pinout](https://www.pjrc.com/teensy/pinout.html)
- Snapshot [Interface&Cntrl.png](../../Interface&Cntrl.png)

Teensy power pins and expansion headers. The screenshot version lives in [Interface&Cntrl.png](../../Interface&Cntrl.png).

```mermaid
flowchart LR
  subgraph TeensyPower[" "]
    VIN_FUSED -->|5V→3V3 reg| VCC3V3[3V3 Rail]
    VCC3V3 --> Teensy[Teensy 4.0]
    GND --> Teensy
  end

  subgraph Headers[" "]
    Teensy --> SDA["I²C SDA<br/>(pin 18)"]
    Teensy --> SCL["I²C SCL<br/>(pin 19)"]
    Teensy --> SWD["SWDIO/SWCLK/RESET<br/>Pads"]
    Teensy --> EXP["Exp Header<br/>(5V,3V3,SDA,SCL,GND)"]
  end

  %% Ground bus  
  classDef groundBus fill:none,stroke:#000,stroke-width:3px;
  GND --- groundBusNode((GND Bus))
  groundBusNode:::groundBus
```
