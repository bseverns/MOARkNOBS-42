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

## Round-trip latency analyzer

When you bounce an impulse through a DAW to clock round-trip latency, feed the resulting WAVs to `rtl_analyzer.py`. The script is standard-library Python, so you can run it on the same host that drives PlatformIO.

```bash
# Baseline vs. tuned buffers from the Oblique RTL loopback
python tools/rtl_analyzer.py \
  --group baseline=~/loops/baseline_run1.wav,~/loops/baseline_run2.wav \
  --group tuned=~/loops/tuned_run.wav \
  --buffer baseline=256 --buffer tuned=64 \
  --max-ms 8.5 \
  --export-json docs/bench/latency/rtl_report.json \
  --export-markdown docs/bench/latency/rtl_report.md
```

What you get:

- Console table that spells out per-file round-trip latency alongside statistical summaries per group.
- Optional JSON + Markdown exports you can drop into lab notes or shove into CI artifacts.
- A `--max-ms` guardrail that trips your test run if the tuned rig suddenly regresses.

Tweak `--threshold` or `--min-gap-ms` if your impulse has a slow rise or heavy reflections. The analyzer grabs the two hottest peaks, so keep your captures clean and tight.

## Bring Your Own Hacks

Got a script that makes debugging less dull? Park it here with a README and keep dependencies light. No binaries, no mystery jars—just plain text mischief that others can remix.
