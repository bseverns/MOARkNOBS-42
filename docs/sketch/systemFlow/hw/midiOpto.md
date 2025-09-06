Incoming MIDI gets quarantined here before touching the digital brain.
The DIN jack and its 1/8" TRS sidekick hit an ESD array then a 6N138 optocoupler, which spits out an isolated logic level for the Teensy.
Beware: 6N138s are slow—keep the pull‑up lean or you’ll drop notes.

**References**
- [Schematic](../../MIDI.png)
- [6N138 datasheet](https://www.vishay.com/docs/83726/6n138.pdf)
- Snapshot [MIDI.png](../../MIDI.png)

Optocoupler & ESD stage for incoming MIDI. See the screenshot in [MIDI.png](../../MIDI.png).

```mermaid
flowchart LR
  subgraph MIDIIn["MIDI IN Opto + ESD"]
    DINIn[DIN_In] --> ESD[ESD_Array]
    TRSIn[TRS_Type-A_In] --> ESD
    ESD --> Opto[Opto_6N138]
    Opto -->|OUT| U1[Teensy 4.0]
    U1 --> GND[GND_Bus]
  end
```

