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
