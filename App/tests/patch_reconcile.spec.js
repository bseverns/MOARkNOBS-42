import { test, expect } from '@playwright/test';
import { createPatchReconciler } from '../runtime/patch_reconcile.js';
import { clone, shallowEqual } from '../runtime/runtime_utils.js';

test('patch reconciler carries live nested slot telemetry into dirty cloned staged config', () => {
  let liveConfig = {
    slots: [
      {
        index: 0,
        data1: 10,
        ef: { index: 0, filter_name: 'LINEAR', frequency: 100, q: 0.7 }
      }
    ],
    filter: { freq: 800 }
  };
  let stagedConfig = clone(liveConfig);
  stagedConfig.filter.freq = 900;

  const applyConfigPatch = createPatchReconciler({
    getLiveConfig: () => liveConfig,
    getStagedConfig: () => stagedConfig,
    isDirty: () => true,
    setLiveConfig: (next) => {
      liveConfig = next;
    },
    setStagedConfig: (next) => {
      stagedConfig = next;
    },
    clone,
    normalizeConfig: (config) => config,
    shallowEqual,
    getManifest: () => ({})
  });

  applyConfigPatch({
    slots: [
      {
        index: 0,
        ef: { frequency: 200 }
      }
    ]
  });

  expect(liveConfig.slots[0].ef.frequency).toBe(200);
  expect(stagedConfig.slots[0].ef.frequency).toBe(200);
  expect(stagedConfig.filter.freq).toBe(900);
});

test('a clean firmware slot patch updates device and editor truth without creating a draft', () => {
  let liveConfig = {
    slots: [{ index: 0, data1: 74, ef: { oversample: 4 }, arg: { enabled: false } }]
  };
  let stagedConfig = clone(liveConfig);
  let dirty = false;
  const devicePatches = [];

  const applyConfigPatch = createPatchReconciler({
    getLiveConfig: () => liveConfig,
    getStagedConfig: () => stagedConfig,
    isDirty: () => dirty,
    reconcileDevicePatch: (nextLive, nextStaged) => {
      liveConfig = nextLive;
      stagedConfig = nextStaged;
      dirty = JSON.stringify(nextLive) !== JSON.stringify(nextStaged);
    },
    clone,
    normalizeConfig: (config) => config,
    shallowEqual,
    getManifest: () => ({}),
    onDevicePatch: (payload) => devicePatches.push(payload)
  });

  applyConfigPatch({
    slots: [{ index: 0, ef: { oversample: 8 }, arg: { enabled: true } }]
  });

  expect(liveConfig.slots[0]).toEqual({
    index: 0,
    data1: 74,
    ef: { oversample: 8 },
    arg: { enabled: true }
  });
  expect(stagedConfig).toEqual(liveConfig);
  expect(dirty).toBe(false);
  expect(devicePatches).toHaveLength(1);
  expect(devicePatches[0].patch.slots[0].ef.oversample).toBe(8);
});

test('a firmware slot patch preserves unrelated local intent and reports leaf conflicts', () => {
  let liveConfig = {
    slots: [{
      index: 0,
      data1: 10,
      ef: { oversample: 4, smoothing: 0.2 },
      arg: { enabled: false, method: 0 },
      lfo: [
        { enabled: false, mode: 4, amount: 0 },
        { enabled: false, mode: 4, amount: 0 }
      ]
    }]
  };
  let stagedConfig = clone(liveConfig);
  stagedConfig.slots[0].data1 = 99;
  const conflicts = [];

  const applyConfigPatch = createPatchReconciler({
    getLiveConfig: () => liveConfig,
    getStagedConfig: () => stagedConfig,
    isDirty: () => true,
    reconcileDevicePatch: (nextLive, nextStaged) => {
      liveConfig = nextLive;
      stagedConfig = nextStaged;
    },
    clone,
    normalizeConfig: (config) => config,
    shallowEqual,
    getManifest: () => ({}),
    onConflict: (entries) => conflicts.push(...entries)
  });

  applyConfigPatch({
    slots: [{
      index: 0,
      ef: { oversample: 8 },
      arg: { enabled: true },
      lfo: [
        { enabled: true, mode: 4, amount: 100 },
        { enabled: false, mode: 4, amount: 0 }
      ]
    }]
  });

  expect(stagedConfig.slots[0].data1).toBe(99);
  expect(stagedConfig.slots[0].ef).toEqual({ oversample: 8, smoothing: 0.2 });
  expect(stagedConfig.slots[0].arg).toEqual({ enabled: true, method: 0 });
  expect(stagedConfig.slots[0].lfo[0]).toEqual({
    lfo: 0,
    enabled: true,
    mode: 4,
    amount: 100
  });
  expect(conflicts).toEqual([]);
});
