# Learner Paths

> Three tiny tours through the beast. Pick one signal, chase it until it squeals, then stop before you accidentally write a framework.

Each path has a safe read, a tiny poke, and one check. No hardware? Use the app simulator path first.

## 1. Button press path: meat finger to MIDI bark

**Open these:**

1. [`docs/sketch/systemFlow/hw/buttonMatrix.md`](../sketch/systemFlow/hw/buttonMatrix.md) — what the rows, columns, muxes, and diodes are doing.
2. [`firmware/include/ButtonManager/README.md`](../../firmware/include/ButtonManager/README.md) — the human-facing button map.
3. [`firmware/src/ButtonManager.cpp`](../../firmware/src/ButtonManager.cpp) — debounce, long press, double press, combo goblins.
4. [`firmware/src/MIDIHandler.cpp`](../../firmware/src/MIDIHandler.cpp) — where the press finally becomes a MIDI message.

**Tiny poke:** change one label or comment in the button map, not behavior. Learners get oriented without risking the scan loop.

**Check:**

```bash
pio -d firmware test -e teensy40_unity -vvv
```

Expected: Unity runs over the custom `Serial1` transport. If PlatformIO tries to invent its own transport, back away slowly and read [`AGENTS.md`](../../AGENTS.md).

## 2. Envelope path: voltage wiggle to slot modulation

**Open these:**

1. [`docs/sketch/systemFlow/hw/envelopeFE.md`](../sketch/systemFlow/hw/envelopeFE.md) — rectifier/front-end reality before the ADC fantasy.
2. [`firmware/include/EnvelopeFollower/README.md`](../../firmware/include/EnvelopeFollower/README.md) — filter and ARG cheat sheets.
3. [`firmware/src/EnvelopeFollower.cpp`](../../firmware/src/EnvelopeFollower.cpp) — smoothing, filters, and calibration.
4. [`firmware/src/ARGMixer.cpp`](../../firmware/src/ARGMixer.cpp) — where followers start fighting creatively.
5. [`firmware/src/ConfigManager.cpp`](../../firmware/src/ConfigManager.cpp) — why the chosen settings survive power cycles.

**Tiny poke:** tweak one documented default in a branch, then undo it after the tour. The point is to watch where the value travels.

**Check:**

```bash
pio -d firmware run -e teensy40_main
```

Expected: firmware compiles. Hardware readings still need a real bench, because analog lies until measured.

## 3. Editor path: browser click to checksum receipt

**Open these:**

1. [`App/config_schema.json`](../../App/config_schema.json) — what the browser is allowed to edit.
2. [`App/views/form_renderer.js`](../../App/views/form_renderer.js) — schema becomes boring inputs.
3. [`App/views/benzknobz.js`](../../App/views/benzknobz.js) — buttons, profile toolbar, and stage props.
4. [`App/runtime.js`](../../App/runtime.js) — transport selection, staged state, rollback, checksum.
5. [`docs/guides/WebSerial.md`](../guides/WebSerial.md) — the wire words the board expects.

**Tiny poke:** run the simulator, edit one numeric field, hit Apply, then watch the dirty badge disappear.

**Check:**

```bash
npm --prefix App test
```

Expected: Playwright drives the simulator and the staged/apply/rollback paths stay green.

## Exit ramp

When a learner gets lost, make them answer four questions: What physical thing changed? Which firmware manager saw it? Which payload crossed the wire? Which UI state changed? If they can answer those, they own the loop.
