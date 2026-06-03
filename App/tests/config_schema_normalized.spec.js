import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';
import MiniAjv from '../lib/mini-ajv.js';
import { normalizeConfig } from '../runtime/config_normalize.js';

function createDeviceConfig() {
  return {
    slots: Array.from({ length: 42 }, (_, index) => ({
      index,
      type_name: 'CC',
      channel: (index % 16) + 1,
      data1: index % 128,
      active: true,
      ef_index: index % 6,
      ef: {
        index: index % 6,
        filter_index: 0,
        filter_name: 'LINEAR',
        frequency: 1000,
        q: 0.707,
        oversample: 4,
        smoothing: 0.2,
        baseline: 0,
        gain: 1,
        mode: 0,
        autoBaseline: true,
        autoGain: true,
        attackMs: 5,
        releaseMs: 20,
        rmsWindowMs: 50,
        baselineTauMs: 2000,
        gainTauMs: 3000,
        gateThreshold: 16,
        gateHysteresis: 4,
        activityThreshold: 4,
        gainTarget: 102,
        destination_mode: 0,
        destination_mode_name: 'add_clamp'
      },
      arg: {
        enabled: false,
        method: 0,
        method_name: 'PLUS',
        sourceA: 0,
        sourceB: 1
      }
    })),
    efSlots: Array.from({ length: 6 }, (_, index) => ({ slots: [index] })),
    filter: { type: 'LINEAR', freq: 1000, q: 1, idle_floor: 24 },
    arg: { method: 'PLUS', a: 0, b: 1, enable: true },
    led: { brightness: 64, color: '#112233', mode: 'STATIC' }
  };
}

test('normalized device config with EF destination mode validates against App schema', () => {
  const schemaPath = path.resolve(process.cwd(), 'config_schema.json');
  const schema = JSON.parse(fs.readFileSync(schemaPath, 'utf8'));
  const ajv = new MiniAjv({ allErrors: true });
  const validator = ajv.compile(schema);
  const normalized = normalizeConfig(createDeviceConfig(), {
    slot_count: 42,
    envelope_count: 6
  });

  expect(normalized.slots[0].ef.destination_mode).toBe('add_clamp');
  expect(validator(normalized), JSON.stringify(validator.errors ?? [], null, 2)).toBe(true);
});
