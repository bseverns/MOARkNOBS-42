flowchart LR
  subgraph Sheet7["Sheet 7 – Envelope FE Front‑End"]
    EnvJacks[6x Envelope Jacks] --> DiodeRect[Diode Rectifier]
    DiodeRect --> RCFilter[RC Filter 4k7+1uF]
    RCFilter --> ClampBAT[Clamp BAT54]
    ClampBAT --> ADCIn[ADC Input Pins]
    ADCIn --> U1[Teensy 4.0]
    RCFilter --> GND[GND Bus]
  end