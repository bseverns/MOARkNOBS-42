# Bench Toys & Code Gremlins

> Grab-bag of small scripts that sniff, log, or otherwise harass the hardware so you can learn by poking.

## Serial Logger

Curious about latency or noise? `serial_logger/SerialToCsv.pde` is a Processing sketch that funnels whatever the board spits into CSV files you can graph or diff later. Quick run:

```text
1. Launch Processing 4.
2. Load `SerialToCsv.pde`.
3. Hit Run and choose the board's serial port.
4. Press `L` to log latency, `N` for noise. `Q` bails.
```

Outputs land in `docs/bench/latency/latency.csv` and `docs/bench/noise/adc_idle.csv`. More details live in the [serial_logger README](serial_logger/README.md).

## RTL Latency Report

When you bounce the rig through an external DAW loop and capture an impulse
response, feed those WAVs to `rtl_latency_report.py`. It auto-detects the
send/return spikes, measures the gap in samples, and spits out both a table and
JSON so you can compare baseline (256 buffer) against tuned (64 buffer) sessions
without hand math. Think of it as a drummer with a calculator: loud, precise,
and painfully honest.

## Bring Your Own Hacks

Got a script that makes debugging less dull? Park it here with a README and keep dependencies light. No binaries, no mystery jars—just plain text mischief that others can remix.
