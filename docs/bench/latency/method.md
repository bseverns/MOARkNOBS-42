# Latency Bench Method

- **Goal:** knob→MIDI/OSC end-to-end latency distribution
- **Rig:** board rev, host OS, USB direct, baud 115200
- **Procedure:** move K01 slow/fast for 60 s; log with SerialToCsv (L)
- **Metric:** delta_ms = t_tx_us - t_scan_us printed by firmware
- **Acceptance:** median 3–5 ms; p95 ≤ 10 ms
- **Notes:** close DAWs; no other MIDI devices

## Loopback / DAW RTL add-on

When you need end-to-end round-trip latency for the hardware + host rig:

1. Wire the DAW send/return per the "Oblique RTL" method and print a sharp impulse at 48 kHz.
2. Record baseline buffer settings (e.g., 256 samples) and tuned settings (e.g., 64 samples) as separate WAV files.
3. Run the analyzer:

   ```bash
   python tools/rtl_analyzer.py \
     --group baseline=docs/bench/latency/baseline \
     --group tuned=docs/bench/latency/tuned \
     --buffer baseline=256 --buffer tuned=64 \
     --export-markdown docs/bench/latency/rtl_report.md \
     --export-json docs/bench/latency/rtl_report.json \
     --max-ms 10
   ```

   Drop WAVs or directories full of captures into each group. The script hunts the outbound/return spikes, prints a summary, and fails the run if round-trip latency slips past `--max-ms`.

4. Stash the Markdown/JSON artifacts next to the bench logs so reviewers can see raw numbers and stats.

This bolt-on keeps the firmware scan telemetry and DAW loopback numbers under the same testing umbrella.

