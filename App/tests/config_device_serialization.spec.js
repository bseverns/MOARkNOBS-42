import { test, expect } from '@playwright/test';
import { compactConfigForDevice } from '../runtime/config_session.js';
import { clone } from '../runtime/runtime_utils.js';

const defaultEf = {
  index: 0,
  filter_index: 0,
  filter_name: 'LINEAR',
  frequency: 1000,
  q: 0.707,
  oversample: 4,
  smoothing: 0.2,
  baseline: 0,
  gain: 1,
  mode: 0,
  autoBaseline: false,
  autoGain: false,
  attackMs: 5,
  releaseMs: 20,
  rmsWindowMs: 50,
  baselineTauMs: 2000,
  gainTauMs: 3000,
  gateThreshold: 16,
  gateHysteresis: 4,
  activityThreshold: 4,
  gainTarget: 102,
  destination_mode: 'add_clamp'
};

function configWithEf(ef) {
  return {
    slots: [{ type: 'CC', channel: 1, data1: 1, active: true, ef_index: 0, ef }]
  };
}

function serialize(current, previous) {
  return compactConfigForDevice(current, previous, {
    clone,
    slotTypeNames: ['OFF', 'CC', 'NOTE']
  });
}

test('device serialization includes an advanced-only EF edit', () => {
  const previous = configWithEf(clone(defaultEf));
  const current = clone(previous);
  current.slots[0].ef.autoGain = true;

  expect(serialize(current, previous).slots[0].ef).toMatchObject({ autoGain: true });
});

test('device serialization includes resetting a non-default EF to defaults', () => {
  const previous = configWithEf({ ...clone(defaultEf), gain: 2, autoGain: true });
  const current = configWithEf(clone(defaultEf));

  expect(serialize(current, previous).slots[0].ef).toMatchObject({
    gain: 1,
    autoGain: false
  });
});
