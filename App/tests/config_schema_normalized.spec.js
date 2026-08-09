import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';
import MiniAjv from '../lib/mini-ajv.js';
import { normalizeConfig } from '../runtime/config_normalize.js';
import { compactConfigForDevice } from '../runtime/config_session.js';
import { clone } from '../runtime/runtime_utils.js';

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
      },
      lfo: [
        { lfo: 0, enabled: index === 0, mode: 4, mode_name: 'centered', amount: 35 },
        { lfo: 1, enabled: false, mode: 3, mode_name: 'scale', amount: -12 }
      ]
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
  expect(normalized.slots[0].lfo).toEqual([
    { enabled: true, mode: 4, amount: 35 },
    { enabled: false, mode: 3, amount: -12 }
  ]);
  expect(validator(normalized), JSON.stringify(validator.errors ?? [], null, 2)).toBe(true);
});

test('firmware-shaped export stays schema-valid across device serialization round trip', () => {
  const schemaPath = path.resolve(process.cwd(), 'config_schema.json');
  const schema = JSON.parse(fs.readFileSync(schemaPath, 'utf8'));
  const ajv = new MiniAjv({ allErrors: true });
  const validator = ajv.compile(schema);
  const manifest = { slot_count: 42, envelope_count: 6 };
  const exported = normalizeConfig(createDeviceConfig(), manifest);

  expect(validator(exported), JSON.stringify(validator.errors ?? [], null, 2)).toBe(true);

  const devicePayload = compactConfigForDevice(exported, null, {
    clone,
    slotTypeNames: schema.properties.slots.items.properties.type.enum
  });
  const readback = normalizeConfig(devicePayload, manifest);

  expect(validator(readback), JSON.stringify(validator.errors ?? [], null, 2)).toBe(true);
  expect(readback).toEqual(exported);
});

test('normalization canonicalizes firmware floats, optional slot defaults, and power cap', () => {
  const config = createDeviceConfig();
  config.slots[0].ef.q = 0.707000017;
  config.slots[0].ef.smoothing = 0.200000003;
  delete config.slots[0].arpNote;
  delete config.slots[0].sysexTemplate;

  const normalized = normalizeConfig(config, {
    slot_count: 42,
    envelope_count: 6,
    led_brightness_cap: 26
  });

  expect(normalized.slots[0].ef.q).toBe(0.707);
  expect(normalized.slots[0].ef.smoothing).toBe(0.2);
  expect(normalized.slots[0].arpNote).toBe(0);
  expect(normalized.slots[0].sysexTemplate).toBe('');
  expect(normalized.led.brightness).toBe(26);
});

test('normalization accepts canonical firmware MIDI type labels', () => {
  const config = createDeviceConfig();
  config.slots[0].type_name = 'NOTE';
  config.slots[1].type_name = 'PITCH_BEND';
  config.slots[2].type_name = 'PROGRAM';

  const normalized = normalizeConfig(config, { slot_count: 42, envelope_count: 6 });

  expect(normalized.slots[0].type).toBe('Note');
  expect(normalized.slots[1].type).toBe('PitchBend');
  expect(normalized.slots[2].type).toBe('ProgramChange');
});
