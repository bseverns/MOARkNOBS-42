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
