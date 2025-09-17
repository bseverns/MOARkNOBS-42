# Oblique RTL + DAW impulse workflow

This is the measurement dance for round-trip latency (RTL) when you shove the
controller through an external DAW loop and back again. Treat it as both lab
notebook and how-to guide so future you (or the review committee) can reproduce
the numbers without swearing.

## Gear + setup

- **Interface:** whatever USB audio box you trust for loopback work. Document
  its driver, firmware, and sample rate (we stick to 48 kHz here).
- **DAW:** route a full-scale impulse out and straight back in. Ableton, Logic,
  Reaper—doesn’t matter as long as you can print the response to disk.
- **Buffers:** run two passes:
  - Baseline: 256-sample buffers (`N_baseline` runs, target ≥ 5).
  - Tuned: 64-sample buffers (`N_tuned` runs, target ≥ 5).
- **Cable sanity:** short, known-good cables. No thru boxes, no stomp pedals in
  the middle.

## Capture flow

1. Arm the DAW to record the loopback channel.
2. Trigger a single-sample impulse from the controller source (MIDI note or
   audio burst—whatever this board spits back cleanly).
3. Bounce the take to a mono WAV. Keep naming consistent, e.g.,
   `rtl_baseline_run01.wav`.
4. Repeat until you have the agreed baseline and tuned run counts. Store them in
   `docs/bench/latency/raw/` (or another tracked directory) so the script can
   glob them later.

## Crunch the numbers

Fire the analyzer from the repo root once the WAVs are staged:

```bash
python tools/rtl_latency_report.py \
  --baseline docs/bench/latency/raw/rtl_baseline_run*.wav \
  --tuned docs/bench/latency/raw/rtl_tuned_run*.wav \
  --buffers baseline=256 tuned=64 daw=512 \
  --json docs/bench/latency/rtl_latest.json
```

What you get back:

- A table showing each capture’s latency in samples and milliseconds, plus how
  many audio buffers that equates to for every config you listed.
- A summary block calling out median/mean/p95 so you can cite stability in your
  write-up.
- Optional JSON (see `--json`) ready to drop into notebooks or the
  professorship dossier.

## Interpreting results

- **Latency (ms):** round-trip time from impulse to return. < 15 ms keeps things
  playable; < 10 ms feels tight.
- **Buffers:** ratio of the latency samples to your buffer size. If the tuned
  pass still spends ~3 buffers, your USB driver is the bottleneck.
- **Amplitude pairs:** the script records send vs. return peak magnitudes. If
  the return collapses (≪ send), check your gain staging or the interface’s
  loopback trim.

## Fold into testing

- Drop the command above into your personal `test-notes.md` and call it out when
  you run the full hardware gauntlet (`./test.sh`).
- Commit the generated JSON alongside the WAVs if you want CI or lab mates to
  diff improvements.
- When publishing results, cite the buffer sizes, DAW version, and the script
  revision (git SHA) so reviewers know the exact conditions.

Stay loud, stay reproducible.
