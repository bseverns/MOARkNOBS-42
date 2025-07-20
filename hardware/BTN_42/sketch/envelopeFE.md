flowchart LR
%% Shared VREF generator
subgraph VREF_GEN[Shared VREF Generator]
  Rv1[R_VREF1 100k to 3V3] --> VREF
  Rv2[R_VREF2 100k to GND] --> VREF
  VREF --> Cvref[C_VREF 4.7uF + 100nF]
end

%% One Envelope Follower Channel (duplicate for E1..E6)
subgraph CH1[Envelope Follower Channel E1]
  JIN1[Input Jack J4] --> CIN1[C_IN1 100nF]
  CIN1 --> RIN1[R_IN1 100k]
  RIN1 --> INV1[Inverting Node]
  VREF --> OP1_PLUS[OpAmp +]
  INV1 --> OP1_MINUS[OpAmp -]
  OP1_OUT[OpAmp Out] --> D1[D1 1N4148]
  D1 --> RECT1[RECT Node]
  RECT1 --> RA1[R_A 4.7k]
  RA1 --> ENV1[ENV Node]
  ENV1 --> RR1[R_R 20k to GND]
  ENV1 --> CENV1[C_ENV 1uF to GND]
  ENV1 --> ROUT1[R_OUT 1k]
  ROUT1 --> E1[ADC Net E1]
  RECT1 --> RF1[R_F1 100k]
  RF1 --> INV1
end

%% Optional clamp diodes (DNI)
subgraph CLAMPS_Optional[Optional Clamps DNI]
  E1 --> DCLP[D_CLAMP+ to 3V3]
  E1 --> DCLM[D_CLAMP- to GND]
end

%% Bias connections
VREF --> OP1_PLUS
