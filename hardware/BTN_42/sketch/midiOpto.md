flowchart LR
  subgraph MIDIIn["MIDI IN Opto + ESD"]
    DINIn[DIN_In] --> ESD[ESD_Array]
    ESD --> Opto[Opto_6N138]
    Opto -->|OUT| U1[Teensy 4.0]
    U1 --> GND[GND_Bus]
  end