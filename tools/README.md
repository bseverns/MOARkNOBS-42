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

## Bring Your Own Hacks

Got a script that makes debugging less dull? Park it here with a README and keep dependencies light. No binaries, no mystery jars—just plain text mischief that others can remix.
