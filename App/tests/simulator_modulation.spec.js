import { test, expect } from '@playwright/test';

test('simulator LFO telemetry helpers are deterministic and follow declared shapes', async ({ page }) => {
  await page.goto('/runtime/simulator_transport.js');
  const shapes = await page.evaluate(async () => {
    const { simulateLfoValue } = await import('/runtime/simulator_transport.js');
    const sample = (shape, config = {}) => Array.from({ length: 32 }, (_, index) =>
      Number(simulateLfoValue({ shape, frequency_hz: 1, depth: 1, bipolar: false, ...config }, index, {
        frameMs: 32,
        seed: 17,
        bpm: 120
      }).toFixed(4))
    );
    return {
      sine: sample(0),
      triangle: sample(1),
      saw: sample(2),
      square: sample(3),
      sampleHoldOneHz: sample(4),
      sampleHoldA: sample(4, { frequency_hz: 8 }),
      sampleHoldB: sample(4, { frequency_hz: 8 }),
      randomSlewA: sample(5, { frequency_hz: 8 }),
      randomSlewB: sample(5, { frequency_hz: 8 }),
      syncWhole: sample(0, { sync: true, sync_ratio: 0 }),
      syncFast: sample(0, { sync: true, sync_ratio: 7 })
    };
  });

  expect(new Set([
    JSON.stringify(shapes.sine),
    JSON.stringify(shapes.triangle),
    JSON.stringify(shapes.saw),
    JSON.stringify(shapes.square)
  ]).size).toBe(4);
  expect(shapes.square.every((value) => value === 0 || value === 1)).toBe(true);
  expect(shapes.saw[0]).toBeLessThan(shapes.saw.at(-1));
  expect(new Set(shapes.sampleHoldOneHz).size).toBe(1);
  expect(shapes.sampleHoldA).toEqual(shapes.sampleHoldB);
  expect(shapes.randomSlewA).toEqual(shapes.randomSlewB);
  expect(new Set(shapes.sampleHoldA).size).toBeGreaterThan(1);
  expect(Math.max(...shapes.sampleHoldA)).toBeGreaterThan(0.5);
  expect(new Set(shapes.randomSlewA).size).toBeGreaterThan(4);
  expect(shapes.syncWhole).not.toEqual(shapes.syncFast);
});

test('simulator EF recipes produce distinct repeatable rehearsal telemetry', async ({ page }) => {
  await page.goto('/runtime/simulator_transport.js');
  const result = await page.evaluate(async () => {
    const { createSimulator } = await import('/runtime/simulator_transport.js');
    const { applySlotTuningRecipe } = await import('/lib/tuning_catalog.js');
    const deps = {
      createManifest: () => ({
        slot_count: 42,
        pot_count: 42,
        envelope_count: 6,
        lfo_count: 2,
        schema_version: 8
      }),
      argMethodNames: ['PLUS', 'AVG', 'MAXX', 'XABS', 'MULT'],
      efFilterNames: [
        'LINEAR',
        'OPPOSITE_LINEAR',
        'EXPONENTIAL',
        'RANDOM',
        'LOWPASS',
        'HIGHPASS',
        'BANDPASS'
      ],
      cloneValue: structuredClone,
      setNested: () => {},
      telemetryFrameMs: 0
    };

    async function collect(recipeId, slotIndex = 0) {
      const simulator = createSimulator(deps);
      await simulator.open();
      await simulator.writeLine(JSON.stringify({ id: 1, rpc: 'get_config' }));
      const configResponse = JSON.parse(await simulator.nextLine());
      const config = structuredClone(configResponse.result.config);
      config.slots[slotIndex] = applySlotTuningRecipe(config.slots[slotIndex], recipeId).slot;
      await simulator.writeLine(JSON.stringify({ id: 2, rpc: 'set_config', config }));
      await simulator.nextLine();
      const values = [];
      const active = [];
      const outputs = [];
      for (let index = 0; index < 80; index += 1) {
        const frame = JSON.parse(await simulator.nextLine());
        values.push(frame.envelopes[0]);
        active.push(frame.efStatus[0]);
        outputs.push(frame.slotOutputs[slotIndex]);
      }
      await simulator.close();
      return { values, active, outputs };
    }

    return {
      neutral: await collect('ef-neutral'),
      smooth: await collect('ef-smooth'),
      punchy: await collect('ef-punchy'),
      gate: await collect('ef-gate'),
      experimentalA: await collect('ef-experimental'),
      experimentalB: await collect('ef-experimental'),
      slotThirteenNeutral: await collect('ef-neutral', 12),
      slotThirteenGate: await collect('ef-gate', 12)
    };
  });

  // Scope telemetry is the synthetic physical EF source: it is deliberately
  // independent of any slot-specific illustrative EF recipe.
  expect(result.smooth.values).toEqual(result.neutral.values);
  expect(result.punchy.values).toEqual(result.neutral.values);
  expect(result.gate.values).toEqual(result.neutral.values);
  expect(result.gate.active).toEqual(result.neutral.active);
  expect(result.experimentalA.values).toEqual(result.experimentalB.values);
  expect(result.experimentalA.values).toEqual(result.neutral.values);
  expect(result.slotThirteenNeutral.values).toEqual(result.slotThirteenGate.values);
  expect(result.slotThirteenNeutral.outputs).not.toEqual(result.slotThirteenGate.outputs);
  expect(new Set([
    JSON.stringify(result.neutral.outputs),
    JSON.stringify(result.smooth.outputs),
    JSON.stringify(result.punchy.outputs),
    JSON.stringify(result.gate.outputs),
    JSON.stringify(result.experimentalA.outputs)
  ]).size).toBeGreaterThan(1);
});
