# Latency Bench Method

- **Goal:** knob→MIDI/OSC end-to-end latency distribution
- **Rig:** board rev, host OS, USB direct, baud 115200
- **Procedure:** move K01 slow/fast for 60 s; log with SerialToCsv (L)
- **Metric:** delta_ms = t_tx_us - t_scan_us printed by firmware
- **Acceptance:** median 3–5 ms; p95 ≤ 10 ms
- **Notes:** close DAWs; no other MIDI devices

## EF sample-to-MIDI enqueue capture

For the firmware portion of an envelope-follower route, flash
`teensy40_ef_latency_bench` and run
`tools/ef_latency_logger/EfLatencyCsv.pde` in Processing 4. It captures
`ef_latency` records into a timestamped CSV.

`device_latency_us` measures the last completed digital EF sample through
translation and MIDI enqueue. It is not a claim about analog-front-end settling
or physical host MIDI arrival; measure those with an external trigger and a
scope or logic analyzer.

To summarize a captured run, open
`tools/ef_step_response_analyzer/EfStepResponseAnalyzer.pde` in Processing 4.
It infers monotonic MIDI-output rises and releases by EF/slot and exports
observed 10–90% duration, event cadence, and device latency.
