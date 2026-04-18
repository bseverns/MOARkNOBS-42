# Idle Noise Bench Method

- **Goal:** ADC idle noise at rest
- **Rig:** same as above
- **Procedure:** call benchNoiseRun(...) for 10–20 s; don’t touch controls
- **Metric:** RMS LSB per channel from raw_counts
- **Acceptance:** ≤ 2 LSB RMS
- **Notes:** isolate from vibration; stable PSU
