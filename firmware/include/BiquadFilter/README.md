# BiquadFilter

Part of the firmware `include` jungle. Peek at the [parent README](../README.md) for map and compass.

Tiny DSP sledgehammer for shaping envelope vibes.

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
