import { test, expect } from '@playwright/test';

test('simulator ARG math follows the firmware method table', async ({ page }) => {
  await page.goto('/runtime/simulator_transport.js');
  const values = await page.evaluate(async () => {
    const { simulateArgValue } = await import('/runtime/simulator_transport.js');
    return [0, 7, 10, 11].map((method) => simulateArgValue(80, 32, method));
  });
  expect(values).toEqual([112, 20, 48, 80]);
});

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

test('simulator shares active-profile LFO routing between telemetry and the Mod Matrix', async ({
  page
}) => {
  await page.goto('/runtime/simulator_transport.js');
  const result = await page.evaluate(async () => {
    const { createSimulator } = await import('/runtime/simulator_transport.js');
    const simulator = createSimulator({
      createManifest: () => ({ slot_count: 42, pot_count: 42, envelope_count: 6, lfo_count: 2 }),
      argMethodNames: ['PLUS'],
      efFilterNames: ['LINEAR'],
      cloneValue: structuredClone,
      setNested: () => {},
      telemetryFrameMs: 0
    });
    const rpc = async (id, rpc, payload = {}) => {
      await simulator.writeLine(JSON.stringify({ id, rpc, ...payload }));
      return JSON.parse(await simulator.nextLine()).result;
    };
    await simulator.open();
    const canonicalConfig = (await rpc(1, 'get_config')).config;
    canonicalConfig.slots[6] = {
      ...canonicalConfig.slots[6],
      type: 'CC',
      type_name: 'CC',
      active: true
    };
    await rpc(2, 'set_config', { config: canonicalConfig });
    const neutralProfile = await rpc(3, 'get_profile');
    const neutralMatrix = await rpc(4, 'get_mod_matrix');
    const route = { type: 4, lfo: 0, depth: 1, amount: 100, min: 20, max: 110, slot: 6 };
    await rpc(5, 'save_profile', { slot: 1 });
    await rpc(6, 'set_profile', { slot: 1, profile: { routes: [route] } });
    const activeMatrix = await rpc(7, 'get_mod_matrix');
    const frame = JSON.parse(await simulator.nextLine());
    const activeRoute = activeMatrix.routes.find((entry) => entry.id === 'lfo0_route0');
    const contribution = frame.slotContributions.find((entry) => entry.index === 6);
    const configResponse = await rpc(8, 'get_config');
    const config = configResponse.config;
    config.slots[6].lfo = [
      { enabled: true, mode: 4, amount: 25 },
      { enabled: false, mode: 0, amount: 0 }
    ];
    await rpc(9, 'set_config', { config });
    const shadowedMatrix = await rpc(10, 'get_mod_matrix');
    await simulator.close();
    return {
      neutralRoutes: neutralProfile.routes,
      neutralLfoRoutes: neutralMatrix.routes.filter((entry) => entry.source_type === 'lfo'),
      neutralConflicts: neutralMatrix.conflicts,
      activeRoute,
      contribution,
      shadowedRoute: shadowedMatrix.routes.find((entry) => entry.id === 'lfo0_route0'),
      fixedRoute: shadowedMatrix.routes.find((entry) => entry.id === 'lfo0_slot6')
    };
  });

  expect(result.neutralRoutes).toEqual([]);
  expect(result.neutralLfoRoutes).toEqual([]);
  expect(result.neutralConflicts).toEqual([]);
  expect(result.activeRoute).toMatchObject({ mode: 'legacy_replace', active: true, slot: 6 });
  expect(result.contribution?.activeMask & 0x02).toBeTruthy();
  expect(result.shadowedRoute).toMatchObject({ mode: 'legacy_shadowed', active: false });
  expect(result.fixedRoute).toMatchObject({ route_type: 'slot_lane', active: true });
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
      telemetryFrameMs: 0,
      fixture: 'demo'
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
      slotFifteenNeutral: await collect('ef-neutral', 14),
      slotFifteenGate: await collect('ef-gate', 14)
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
  expect(result.slotFifteenNeutral.values).toEqual(result.slotFifteenGate.values);
  expect(result.slotFifteenNeutral.outputs).not.toEqual(result.slotFifteenGate.outputs);
  expect(new Set([
    JSON.stringify(result.neutral.outputs),
    JSON.stringify(result.smooth.outputs),
    JSON.stringify(result.punchy.outputs),
    JSON.stringify(result.gate.outputs),
    JSON.stringify(result.experimentalA.outputs)
  ]).size).toBeGreaterThan(1);
});
