import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';
import MiniAjv from '../lib/mini-ajv.js';
import { normalizeConfig } from '../runtime/config_normalize.js';
import {
  ARG_METHOD_PRESENTATION,
  EF_FILTER_PRESENTATION,
  SLOT_TUNING_RECIPES,
  applySlotTuningRecipe,
  describeDeviceConfigPatch,
  formatArgMethodLabel,
  formatEfFilterLabel
} from '../lib/tuning_catalog.js';

const schema = JSON.parse(
  fs.readFileSync(path.resolve(process.cwd(), 'config_schema.json'), 'utf8')
);
const validator = new MiniAjv({ allErrors: true }).compile(schema);
const manifest = { slot_count: 42, envelope_count: 6 };

function recipeConfig() {
  const config = normalizeConfig({}, manifest);
  config.slots[0].label = 'browser-only note';
  config.slots[0].ef.baseline = 0.37;
  config.slots[0].ef.gain = 1.4;
  config.slots[0].ef.index = 2;
  config.slots[0].efIndex = 2;
  config.slots[0].arg.sourceA = 2;
  config.slots[0].arg.sourceB = 4;
  return config;
}

test('presentation enum coverage stays locked to schema truth', () => {
  const slotProperties = schema.properties.slots.items.properties;
  expect(Object.keys(EF_FILTER_PRESENTATION)).toEqual(
    slotProperties.ef.properties.filter_name.enum
  );
  expect(Object.keys(ARG_METHOD_PRESENTATION)).toEqual(
    slotProperties.arg.properties.method_name.enum
  );

  for (const entry of [
    ...Object.values(EF_FILTER_PRESENTATION),
    ...Object.values(ARG_METHOD_PRESENTATION)
  ]) {
    expect(entry).not.toHaveProperty('type');
    expect(entry).not.toHaveProperty('enum');
    expect(entry).not.toHaveProperty('minimum');
    expect(entry).not.toHaveProperty('maximum');
    expect(entry).not.toHaveProperty('default');
  }
});

test('musical labels lead while exact firmware tokens remain discoverable', () => {
  expect(formatEfFilterLabel('LINEAR')).toBe('Literal · LINEAR');
  expect(formatEfFilterLabel('LOWPASS')).toBe('Smooth · LOWPASS');
  expect(formatEfFilterLabel('EXPONENTIAL')).toBe('Punchy · EXPONENTIAL');
  expect(formatArgMethodLabel('PLUS')).toBe('Add Together · PLUS');
  expect(formatArgMethodLabel('AVG')).toBe('Average Together · AVG');
  expect(formatArgMethodLabel('MAXX')).toBe('Strongest Wins · MAXX');
  expect(formatArgMethodLabel('XABS')).toBe('Difference · XABS');
  expect(formatArgMethodLabel('MULT')).toBe('Interaction · MULT');
});

test('every tuning recipe is deterministic, schema-valid, and limited to one slot', () => {
  for (const recipe of SLOT_TUNING_RECIPES) {
    const source = recipeConfig();
    source.slots[0].ef = {
      ...source.slots[0].ef,
      filter_name: 'OPPOSITE_LINEAR',
      filter_index: 1,
      mode: 3
    };
    source.slots[0].arg = {
      ...source.slots[0].arg,
      enabled: false,
      method: 13,
      method_name: 'XORR'
    };
    source.slots[0].lfo = [
      { enabled: false, mode: 0, amount: 0 },
      { enabled: false, mode: 0, amount: 0 }
    ];
    const untouchedSlot = structuredClone(source.slots[1]);
    const originalSelected = structuredClone(source.slots[0]);

    const first = applySlotTuningRecipe(source.slots[0], recipe.id, { laneIndex: 1 });
    const second = applySlotTuningRecipe(source.slots[0], recipe.id, { laneIndex: 1 });

    expect(first).toEqual(second);
    expect(source.slots[0]).toEqual(originalSelected);
    expect(source.slots[1]).toEqual(untouchedSlot);
    expect(first.changedPaths.length).toBeGreaterThan(0);

    source.slots[0] = first.slot;
    const normalized = normalizeConfig(source, manifest);
    expect(validator(normalized), `${recipe.id}: ${JSON.stringify(validator.errors)}`).toBe(true);
    expect(normalized.slots[1]).toEqual(normalizeConfig(recipeConfig(), manifest).slots[1]);
    expect(normalized.slots[0].ef.baseline).toBe(0.37);
    expect(normalized.slots[0].ef.gain).toBe(1.4);
    expect(normalized.slots[0].efIndex).toBe(2);

    if (recipe.target === 'arg') {
      expect(normalized.slots[0].arg.sourceA).toBe(2);
      expect(normalized.slots[0].arg.sourceB).toBe(4);
    }
  }
});

test('neutral recipe restores response defaults without erasing calibration or routing', () => {
  const source = recipeConfig();
  source.slots[0].ef = {
    ...source.slots[0].ef,
    filter_name: 'RANDOM',
    filter_index: 3,
    frequency: 4000,
    q: 3,
    smoothing: 0.05,
    mode: 2,
    gateThreshold: 70
  };

  const result = applySlotTuningRecipe(source.slots[0], 'ef-neutral');

  expect(result.slot.ef).toMatchObject({
    filter_name: 'LINEAR',
    filter_index: 0,
    frequency: 1000,
    q: 0.707,
    oversample: 4,
    smoothing: 0.2,
    mode: 0,
    attackMs: 5,
    releaseMs: 20,
    baseline: 0.37,
    gain: 1.4,
    index: 2
  });
  expect(result.slot.ef.destination_mode).toBe(source.slots[0].ef.destination_mode);
});

test('device patch notices describe reported truth without claiming an unproven origin', () => {
  const previous = {
    slots: Array.from({ length: 7 }, (_, index) => ({
      index,
      ef: { oversample: 4 },
      arg: { enabled: false },
      lfo: [{ enabled: false, mode: 4, amount: 100 }]
    }))
  };
  const live = structuredClone(previous);
  live.slots[6].ef.oversample = 8;
  expect(describeDeviceConfigPatch({
    slots: [{
      index: 6,
      ef: { oversample: 8 },
      arg: { enabled: false },
      lfo: [{ enabled: false, mode: 4, amount: 100 }]
    }]
  }, previous, live)).toBe('Slot 7 · EF oversampling → 8×');
  expect(describeDeviceConfigPatch({
    slots: [{ index: 2, arg: { enabled: true } }]
  })).toBe('Slot 3 · ARG → On');
  expect(describeDeviceConfigPatch({
    slots: [{ index: 4, lfo: [{ enabled: true, mode: 4, amount: 100 }] }]
  })).toBe('Slot 5 · LFO 1 → On · Centered · 100%');
});
