# Combo Guide

The raw button/combination table is useful, but it is not the easiest way to learn the controller.

This page groups the combos by intention so a new player can memorize the ones that matter first.

![Control-button map highlighting recovery, profile, arp, reactive-control, and mapping combo clusters.](../assets/workflows/combo-map-overview.png)

## Start here if you are new

Memorize these first:

1. profile cycling
2. arp toggles
3. panic-safe reset
4. one or two MIDI-type shortcuts

That gives you recovery, timing control, and basic mapping power without having to carry the whole matrix in your head.

The full button table still lives in [firmware/include/ButtonManager/README.md](https://github.com/bseverns/benzknober/blob/main/firmware/include/ButtonManager/README.md#button-map).

## Recovery combos

These are the ones worth learning even if you never become a combo wizard.

| Combo | What it does | Why it matters |
| --- | --- | --- |
| `Ctrl0 + Ctrl1 + Ctrl2` | panic-safe baseline reset | fastest "get me back to something sane" move |
| long-press `Ctrl1` | reload profile from EEPROM | recover a stored baseline |
| long-press `Ctrl3` | reset EEPROM | destructive recovery path; not a casual action |

Recommended to memorize first:

- panic-safe reset
- profile reload

## Profile and memory combos

These help you move between saved states.

| Combo | What it does |
| --- | --- |
| `Ctrl1 + Ctrl2` | cycle profiles A-D |
| long-press `Ctrl4` | save config |
| long-press `Ctrl1` | reload current profile |

These are the combos that matter most once you start using the rig in rehearsal or live work.

## Arp combos

These control timing and note behavior.

| Combo | What it does |
| --- | --- |
| `Ctrl2 + Ctrl4` | toggle arpeggiator |
| long-press `Ctrl2 + Ctrl4` | enter arp edit while held |
| `Ctrl2 + Ctrl3` | bump arp base note |
| long-press `Ctrl2 + Ctrl3` | cycle swing presets |

Best ones to learn first:

- `Ctrl2 + Ctrl4`
- long-press `Ctrl2 + Ctrl3`

Those two alone let you feel the arp become part of the instrument.

## Mapping and MIDI-type combos

These are about changing what a slot *is*.

| Combo | What it does |
| --- | --- |
| `Ctrl4 + Ctrl5` | set slot to MIDI Note mode |
| `Ctrl3 + Ctrl5` | set slot to Program Change |
| `Ctrl0 + Ctrl5` | set slot to Pitch Bend |
| `Ctrl1 + Ctrl4` | set slot to Aftertouch |
| `Ctrl2 + Ctrl5` | set slot to NRPN |
| `Ctrl1 + Ctrl3` | set slot to RPN |
| `Ctrl0 + Ctrl3` | set slot to SysEx |

This is powerful, but it is not the right first thing to memorize. Learn it once the concepts are clear.

## Reactive-control combos

These are where the deck starts acting more like a reactive instrument.

| Combo | What it does |
| --- | --- |
| `Ctrl0 + Ctrl1` | cycle ARG method |
| `Ctrl0 + Ctrl2` | cycle ARG envelope pair |
| `Ctrl0 + Ctrl4` | enable EF and randomize settings |

These are great once you already understand EF and ARG, but they are exactly the ones that can feel like "black magic" without context.

Read these first:

- [ARG Guide](ARGGuide.md)
- [Filter Feel Guide](FilterFeelGuide.md)
- [Reactive Control Guide](ReactiveControlGuide.md)

## System and mode combos

These change broader operating behavior.

| Combo | What it does |
| --- | --- |
| `Ctrl3 + Ctrl4 + Ctrl5` | toggle USB MIDI output |
| `Ctrl3 + Ctrl4` | cycle LED display modes |
| `Ctrl1 + Ctrl5` | toggle MIDI clock output |

These are useful, but most players do not need them on day one.

## Dangerous or destructive actions

These deserve extra respect:

- long-press `Ctrl3` for EEPROM reset
- anything that rewrites slot type while you are not sure what the active slot is

If in doubt, reload a profile instead of escalating immediately.

## A good memorization order

If you only want a sensible first set, learn these in order:

1. `Ctrl1 + Ctrl2` for profile cycling
2. `Ctrl2 + Ctrl4` for arp toggle
3. `Ctrl0 + Ctrl1 + Ctrl2` for panic-safe reset
4. long-press `Ctrl1` for profile reload
5. one MIDI type shortcut you actually use often

That gives you a practical live vocabulary without forcing you to memorize the whole matrix at once.

## Read next

- [Profile Workflow](ProfileWorkflow.md) for how profile actions fit the browser/device workflow
- [Failure-First Guide](../validation/FailureFirst.md) for what to do when a combo puts the rig somewhere unexpected
- [Builder's Handbook](../getting-started/BuildersHandbook.md) for the broader hands-on control story
