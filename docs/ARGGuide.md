# ARG Guide

ARG is one of the most interesting parts of the rig and one of the easiest to overcomplicate in conversation.

This page explains ARG as a musical tool rather than as a list of formulas.

## What ARG is for

SEF gives you one envelope follower driving one response path.

ARG gives you a way to ask a more interesting question:

- what if two signals reinforce each other?
- what if one should oppose the other?
- what if the useful control value is the *difference* between them rather than either source alone?

That is why ARG matters. It turns the controller from "reactive" into "comparative."

## When to stay in SEF instead

Do **not** start with ARG just because it sounds more advanced.

Stay in SEF when:

- you are calibrating a new input
- you want obvious cause and effect
- you are teaching a newcomer
- you are troubleshooting noise or wrong routing

Move to ARG when you already trust the individual follower paths.

See: [Reactive Control Guide](ReactiveControlGuide.md)

## Start here if you are new

Use this safest-first setup:

- one known-good envelope source on A
- one obviously different source on B
- filter set to `LOWPASS` or `LINEAR`
- ARG method set to `PLUS`, `AVG`, `MAXX`, or `XABS`

Those four methods teach the concept cleanly.

## ARG matrix overview

The fastest way to understand ARG is to stop thinking about it as a list of names and start looking at what happens when the same two input signals are combined over time.

![Overview chart showing representative ARG methods as multiple colored time-series lines. In each card, a blue-gray A input and magenta B input run across time, while an orange C output line shows the resulting slot value for PLUS, AVG, MAXX, XABS, MULT, and XORR.](assets/signal-shapes/arg-matrix-overview.png)

This is not "audio scope truth." It is a quick map of the mixing math after the firmware clamps the result back into the normal `0..127` envelope range.

## The ARG families

The firmware supports fourteen ARG methods. The formulas live in the firmware docs; this page groups them by feel.

### Additive and cooperative

These methods make the two inputs feel like they are working together.

| Method | What it feels like | Use it when |
| --- | --- | --- |
| `PLUS` | both inputs push in the same direction | you want obvious combined energy |
| `AVG` | both inputs negotiate a middle path | you want smoother two-source behavior |
| `SQAR` | both inputs create a fuller combined magnitude | you want something bigger than either source alone |
| `MAXX` | whichever source is stronger wins | you want the dominant signal to take over |
| `MINN` | whichever source is weaker wins | you want the subtle signal to stay relevant |

Best beginner picks from this family:

1. `PLUS`
2. `AVG`
3. `MAXX`

### Subtractive and contrast-based

These methods make the interaction feel more like tension than cooperation.

| Method | What it feels like | Use it when |
| --- | --- | --- |
| `MIN` | A subtracts B | you want ducking or opposition |
| `PECK` | B subtracts A | same idea, opposite emphasis |
| `SHAV` | a softer subtraction | you want tension without wild swings |
| `XABS` | absolute difference between A and B | you want "how different are they?" to become the control signal |

Best beginner pick from this family:

1. `XABS`

`XABS` is the most teachable contrast method because the result is easy to hear and easy to explain.

### Ratio and comparative oddballs

These methods are less immediately intuitive.

| Method | What it feels like | Use it when |
| --- | --- | --- |
| `BABS` | A measured against absolute B | you want ratio-like behavior |
| `TABS` | a stronger `BABS` feel | you want that comparison more aggressively |
| `DIVI` | A divided by B-ish behavior | you want unstable comparative motion |

These are useful, but rarely the right first stop for a learner.

### Chaotic and glitch-forward

These are the "yes, the firmware can do that" methods.

| Method | What it feels like | Use it when |
| --- | --- | --- |
| `MULT` | both inputs multiply into each other | you want ring-mod-like intensity |
| `XORR` | bitwise glitch character | you want deliberate weirdness |

Treat these as experimental tools, not onboarding tools.

## Good first ARG progressions

### Progression 1: learn the idea

1. `PLUS`
2. `AVG`
3. `MAXX`

This teaches cooperation, averaging, and dominance.

### Progression 2: learn contrast

1. `PLUS`
2. `XABS`
3. `MIN`

This teaches how the same two inputs can stop behaving like a blend and start behaving like a comparison.

### Progression 3: move into experiments

1. `MAXX`
2. `MULT`
3. `XORR`

This is a good path once the user already trusts the setup and wants the rig to act stranger.

## When ARG feels wrong

If ARG suddenly feels confusing, noisy, or musically useless:

1. go back to SEF
2. verify both envelope sources alone
3. set the filter to `LOWPASS`
4. return to `PLUS` or `AVG`

That gets you back to something legible.

See: [Failure-First Guide](FailureFirst.md)

## Where ARG shows up in the shipped presets

- `DEMO_A - Reactive Stack` uses `MAXX` to exaggerate dominant motion.
- `DEMO_B - Clock Contrast` uses `XABS` to highlight contrast and difference.
- `AE Modular - Probability Sketch` uses `PLUS` to keep the modular feel lively without becoming unreadable.

See: [Preset Library](PresetLibrary.md)

## Read next

- [Filter Feel Guide](FilterFeelGuide.md) for the response shapes ARG is working on top of
- [Reactive Control Guide](ReactiveControlGuide.md) for the bigger signal path
- [Failure-First Guide](FailureFirst.md) for what to do when ARG gets confusing
