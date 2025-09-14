# SerialToCsv Logger

A scrappy Processing sketch that funnels serial chatter into CSV files. Useful
for latency and noise benches without dragging in heavyweight logging stacks.

## Quick Start
1. Fire up Processing 4.
2. Load `SerialToCsv.pde`.
3. Hit Run. Pick your port number.
4. Smash `L` for latency or `N` for noise logging.
5. Twist knobs or let the board sit. Hit `Q` to bail.

## Where the Bits Go
- Latency lines \u2192 `docs/bench/latency/latency.csv`
- Noise samples \u2192 `docs/bench/noise/adc_idle.csv`

Append mode keeps old runs, so nuke files if you need a clean slate.

Punk tip: this logger doesn’t care about protocols, it just writes lines.
