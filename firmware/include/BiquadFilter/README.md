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
