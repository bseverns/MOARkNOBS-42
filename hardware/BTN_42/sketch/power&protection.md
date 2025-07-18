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
  class F1,F2 fill:#ffeeba,stroke:#b89400;
  class DTVS1 fill:#f5c6cb,stroke:#a71d2a;
  class Cbulk1,Cbulk2,CLED1 fill:#d4edda,stroke:#155724;