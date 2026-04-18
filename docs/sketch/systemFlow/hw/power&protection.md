This chunk wrangles incoming power, fuses, and transient smackdowns before the rest of the rig sees a volt.
VIN hits a resettable fuse, bulk caps, and a TVS clamp, then splits off to a beefy LED rail through its own PTC.
PTCs trip slow—short that LED strip and the core might brown out before the fuse wakes up.

**References**

- [Schematic](../../Power-Reg.png)
- [SA6.0A TVS datasheet](https://www.littelfuse.com/products/tvs-diodes/standard-tvs-diodes/sa.aspx)
- Snapshot [Power-Reg.png](../../Power-Reg.png)

Power and protection overview! A detailed PNG version lives in [Power-Reg.png](../../Power-Reg.png).

```mermaid
flowchart LR
  subgraph PowerEntry[" "]
    DC_JACK((DC Jack))
    DC_JACK -->|VIN_RAW| F1[PTC F1<br/>0.5 A hold]
    F1 --> VIN_FUSED[[VIN_FUSED]]
  end

  subgraph ProtectionAndDecoupling[" "]
    VIN_FUSED --> Cbulk1["Cbulk1 220 µF"]
    VIN_FUSED --> Cbulk2["Cbulk2 10 µF"]
    VIN_FUSED --> DTVS1[TVS DTVS1<br/>SA6.0A]
    DTVS1 --> GND((GND))
    Cbulk1 --> GND
    Cbulk2 --> GND
  end

  subgraph LEDRail[" "]
    VIN_FUSED -->|branch| F2[PTC F2<br/>2.5 A hold]
    F2 --> VLED[[VLED]]
    VLED --> CLED1["CLED1 100 µF"]
    CLED1 --> GND
  end

  %% Ground bus drawn once
  classDef groundBus fill:none,stroke:#000,stroke-width:3px;
  GND --- groundBusNode((GND Bus))

  groundBusNode:::groundBus

  %% Styling
  classDef fuse fill:#ffeeba,stroke:#b89400;
  classDef tvs fill:#f5c6cb,stroke:#a71d2a;
  classDef cap fill:#d4edda,stroke:#155724;
  class F1,F2 fuse;
  class DTVS1 tvs;
  class Cbulk1,Cbulk2,CLED1 cap;
```
