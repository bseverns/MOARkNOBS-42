# EF Latency CSV Logger

`EfLatencyCsv.pde` is a Processing 4 bench logger for the envelope-follower
pipeline. It records firmware timing from the last completed digital EF sample
to the attempt to enqueue its translated MIDI output, plus the host serial
receipt timestamp.

## Capture

1. Flash the bench build:

   ```bash
   pio run -d firmware -e teensy40_ef_latency_bench -t upload
   ```

2. Open `EfLatencyCsv.pde` in Processing 4, press the displayed port number,
   then press `R` to start a timestamped CSV under the sketch's `data/` folder.
3. Feed a repeatable signal into one active EF route. Press `Q` to close the
   serial port and CSV safely.

The CSV's `device_latency_us` is a device-clock interval: it includes the
high-tier-to-mid-tier handoff and EF translation/MIDI enqueue work. Serial
logging perturbs that path, so use it for relative firmware scheduling
measurements, not a production-performance claim.

It does **not** measure the analog envelope front-end settling time or physical
MIDI arrival at a host. Those require an external source trigger and a scope,
logic analyzer, or synchronized MIDI receiver measurement.
