# Latency Bench Method

- **Goal:** knob→MIDI/OSC end-to-end latency distribution
- **Rig:** board rev, host OS, USB direct, baud 115200
- **Procedure:** move K01 slow/fast for 60 s; log with SerialToCsv (L)
- **Metric:** delta_ms = t_tx_us - t_scan_us printed by firmware
- **Acceptance:** median 3–5 ms; p95 ≤ 10 ms
- **Notes:** close DAWs; no other MIDI devices
