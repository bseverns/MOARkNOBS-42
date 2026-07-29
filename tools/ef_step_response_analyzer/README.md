# EF Step Response Analyzer

Open `EfStepResponseAnalyzer.pde` in Processing 4 and press `O` to select a
CSV from `tools/ef_latency_logger/EfLatencyCsv.pde`. The sketch groups samples
by EF and slot, infers monotonic rises/releases, and exports one row per
meaningful response run with:

- observed MIDI-output 10–90% duration;
- event cadence;
- mean device sample-to-MIDI-enqueue latency; and
- the output range and sample count used for each result.

The analyzer ignores movements under 16 MIDI values and splits runs after a
100 ms sample gap or a meaningful direction reversal. Those thresholds are in
the sketch constants so a bench operator can make them stricter for a noisy
signal.

The result describes the observed **translated MIDI output response**. It does
not establish analog EF circuit rise/release time from an external stimulus or
physical host MIDI-arrival latency. Use a shared trigger and scope/logic
analyzer when those measurements are required.
