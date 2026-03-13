# Filter Feel Guide

This guide is about how the envelope follower shapes feel in practice, not just what the enum names mean.

The main question is simple:

What kind of motion do you want under your hands?

## Start here if you are new

If you do not yet know what you want, start in this order:

1. `LINEAR`
2. `LOWPASS`
3. `EXPONENTIAL`
4. only then try `BANDPASS`, `HIGHPASS`, or `RANDOM`

That progression teaches the system without overwhelming the player.

## The shapes by feel

### LINEAR

**Feels like:** the most literal mapping.

Use when:

- you are learning the rig
- you want one-to-one behavior
- you are comparing other settings against a neutral baseline

Avoid when:

- the input is too noisy and raw motion feels messy

### OPPOSITE_LINEAR

**Feels like:** the same literal mapping, but upside down.

Use when:

- loud should reduce a value rather than increase it
- you want simple inversion without added character

This is best understood after `LINEAR`, not before it.

### EXPONENTIAL

**Feels like:** more dramatic attacks and less emphasis on the tail.

Use when:

- you want punch
- you want the rig to reward transient-heavy playing
- you want a preset to feel immediately animated

This is a great "show it off" setting.

### LOWPASS

**Feels like:** smoother, calmer, less twitchy motion.

Use when:

- you want follower cleanup
- your input feels too fizzy or unstable
- you want reactive motion that still reads as intentional

This is often the best first practical filter after `LINEAR`.

### HIGHPASS

**Feels like:** reacts more to change than to steady level.

Use when:

- you care more about attacks than sustain
- slow swells should matter less
- you want the rig to ignore drift or rumble

This is a sharper, more selective feel than `LOWPASS`.

### BANDPASS

**Feels like:** more focused and characterful, less general-purpose.

Use when:

- you want the reactive motion to feel selective
- you want a more stylized response
- you want obvious contrast against smoother presets

This is a good second-stage exploration setting once the player already understands `LINEAR` and `LOWPASS`.

### RANDOM

**Feels like:** alive, unstable, generative, a little unruly on purpose.

Use when:

- you want patch-lab energy
- the point is character, not predictable control
- you are comfortable with the rig feeling slightly less literal

This is not the first stop for a beginner, but it is a good "the instrument can also be weird" demonstration.

## Good beginner pairings

### Best first lesson

- filter: `LINEAR`
- no ARG
- one envelope follower

### Best first practical rig

- filter: `LOWPASS`
- no ARG or `PLUS`
- LED brightness as the visible target

### Best first expressive demo

- filter: `EXPONENTIAL`
- `MAXX` ARG
- a preset like `DEMO_A`

### Best first experimental lab

- filter: `BANDPASS` or `RANDOM`
- `XABS` or `MULT`
- an exploratory preset like `AE Modular - Probability Sketch`

## Which shipped presets demonstrate which feel

| Preset | Main feel lesson |
| --- | --- |
| `DEMO_A - Reactive Stack` | `LOWPASS` plus obvious composite motion |
| `DEMO_B - Clock Contrast` | `BANDPASS` plus stronger contrast |
| `AE Modular - Probability Sketch` | `LOWPASS` in a patch-lab context |

See: [Preset Library](PresetLibrary.md)

## If the filter choice feels wrong

When in doubt:

- too jittery -> try `LOWPASS`
- too boring -> try `EXPONENTIAL`
- too confusing -> go back to `LINEAR`
- too chaotic -> leave `RANDOM` and `BANDPASS` for later

## Read next

- [ARG Guide](ARGGuide.md) for the blend layer that often sits on top of these shapes
- [Reactive Control Guide](ReactiveControlGuide.md) for the wider signal path
- [Preset Library](PresetLibrary.md) for example presets that make these differences visible
