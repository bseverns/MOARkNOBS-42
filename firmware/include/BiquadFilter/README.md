# BiquadFilter

Part of the firmware `include` jungle. Peek at the [include README](../README.md) for map and compass, and the [main firmware README](../../README.md) for the battlefield layout.

Tiny DSP sledgehammer for shaping envelope vibes.

## Where it fits

BiquadFilter smooths raw signals for EnvelopeFollower or any module that needs a quick EQ before talking to the rest.

```
[Signal] --> BiquadFilter --> EnvelopeFollower
```

Zoom out via the [main firmware README](../../README.md).

## Key Methods

- `configure(type, freq, sampleRate, q)` – set up the coefficients.
- `process(sample)` – run one float through the math grinder.

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
