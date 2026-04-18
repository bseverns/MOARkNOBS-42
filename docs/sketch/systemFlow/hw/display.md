The display block pumps I²C pixels to a tiny SSD1306 OLED and breaks out an aux header for debugging.
Teensy pins 18/19 sling SDA/SCL while a spare GPIO drives the status LED.
The OLED likes 5 V for power but I²C stays at 3 V3—mix them up and you’re in for a long night.

**References**

- [Schematic](../../Interface&Cntrl.png)
- [SSD1306 datasheet](https://cdn-learn.adafruit.com/assets/assets/000/036/495/original/SSD1306.pdf)
- Snapshot [Interface&Cntrl.png](../../Interface&Cntrl.png)

Here’s the display & aux header layout. Peek [Interface&Cntrl.png](../../Interface&Cntrl.png) for the original schematic snapshot.

```mermaid
flowchart LR
  subgraph Sheet8["Sheet 8 – Display & Aux"]
    U1[Teensy 4.0] -->|SDA| OLED[SSD1306 OLED]
    U1 -->|SCL| OLED
    OLED -->|VCC 5V| V5[5V Rail]
    OLED -->|GND| GND[Ground]
    U1 -->|GPIO ?| LEDact[Status LED]
    LEDact -->|Resistor 1k| U1
    U1 -->|Pins| EXP[Exp Header<br/>5V/3V3/SDA/SCL/GND]
    EXP -->|SWDIO/SWCLK/RESET| SWD[SWD Pads]
  end
```
