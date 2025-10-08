# BiquadFilter

Part of the firmware `include` jungle. Peek at the [include README](../README.md) for map and compass, and the [main firmware README](../../README.md) for the battlefield layout.

Tiny DSP sledgehammer for shaping envelope vibes.

> Need a refresher on coefficients and poles? The [MIDI + DSP 101 Primer](../../../docs/Primers/MIDI-DSP101.md#biquad-filters) breaks down the RBJ math we lean on.

## Where it fits

BiquadFilter smooths raw signals for EnvelopeFollower or any module that needs a quick EQ before talking to the rest.

```
[Signal] --> BiquadFilter --> EnvelopeFollower
```

Zoom out via the [main firmware README](../../README.md).

## Key Methods

- `configure(type, freq, sampleRate, q)` – set up the coefficients and blast any leftover history.
- `process(sample)` – run one float through the math grinder.

## Reset Behavior

When you call `configure`, the filter torches its memory (`x1`, `x2`, `y1`, `y2`).
Perfect when you want a clean start, but don't expect continuity across reconfigs.

## Typical Use

```cpp
#include "BiquadFilter.h"

BiquadFilter filt;
filt.configure(BiquadFilter::LOWPASS, 1000, 44100);
float out = filt.process(in);
```

Read the source at [BiquadFilter.h](../BiquadFilter.h).

## Filter Types

| Type | What slips through | Typical use |
| ---- | ------------------ | ----------- |
| `LOWPASS` | Frequencies below the cutoff | Calm the high-end buzz before modulation |
| `HIGHPASS` | Frequencies above the cutoff | Dump DC and sluggish swells |
| `BANDPASS` | A window around the cutoff | Focus on a slice and trash the rest |

These modes feed the [EnvelopeFollower](../EnvelopeFollower/README.md) and any other module that needs quick tone surgery.
