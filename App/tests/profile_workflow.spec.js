import { test, expect } from '@playwright/test';
import {
  createProfileWorkflow,
  resolveProfileCapabilities,
  supportsGuidedProfileFlow,
  unsupportedProfileActionCopy
} from '../views/panels/profile_workflow.js';

function createHarness({
  capabilities = { profileSave: true, profileLoad: true, profileReset: true },
  interactable = true,
  dirty = false,
  applyResult = { applied: true },
  dirtyAfterApply = false,
  rpcResponses = []
} = {}) {
  const calls = [];
  const statuses = [];
  let activeSlot = 1;
  const pendingResponses = [...rpcResponses];
  const runtime = {
    getState: () => ({ dirty }),
    apply: async () => {
      calls.push({ type: 'apply' });
      dirty = dirtyAfterApply;
      return applyResult;
    },
    sendRpc: async (payload, options) => {
      calls.push({ type: 'sendRpc', payload, options });
      if (!pendingResponses.length) return { profile: activeSlot };
      const response = pendingResponses.shift();
      if (response instanceof Error) throw response;
      return response;
    },
    hydrateAuthoritativeConfig: (config) =>
      calls.push({ type: 'hydrateAuthoritativeConfig', config })
  };
  const formRenderer = {
    clearPendingPatches: () => calls.push({ type: 'clearPendingPatches' })
  };
  const workflow = createProfileWorkflow({
    runtime,
    formRenderer,
    setStatus: (level, label, message) => statuses.push({ level, label, message }),
    clampSlot: (slot, count) => Math.min(count - 1, Math.max(0, Number(slot) || 0)),
    slotCount: 4,
    getCapabilities: () => capabilities,
    isInteractable: () => interactable,
    getActiveProfileSlot: () => activeSlot,
    setActiveProfileSlot: (slot) => {
      activeSlot = slot;
      calls.push({ type: 'setActiveProfileSlot', slot });
    },
    describeSlot: () => `Slot ${String.fromCharCode(65 + activeSlot)}`,
    refreshProfileControls: () => calls.push({ type: 'refreshProfileControls' }),
    refreshProfileUtilities: (options) => calls.push({ type: 'refreshProfileUtilities', options }),
    timeoutMs: 123
  });
  return { calls, statuses, workflow };
}

test('profile workflow derives capability gates from explicit manifest flags', () => {
  expect(
    resolveProfileCapabilities({
      capabilities: {
        profile_save: true,
        profile_load: false,
        profile_reset: true,
        macro_snapshot: true,
        scenes: false
      }
    })
  ).toEqual({
    profileSave: true,
    profileLoad: false,
    profileReset: true,
    macroSnapshot: true,
    arpLive: false,
    scenes: false
  });
  expect(supportsGuidedProfileFlow({ profileSave: true, profileLoad: false })).toBe(false);
  expect(unsupportedProfileActionCopy('load_profile')).toContain('cannot switch device profile slots');
});

test('profile workflow applies dirty staged edits before saving a profile', async () => {
  const { calls, statuses, workflow } = createHarness({
    dirty: true,
    rpcResponses: [{ profile_saved: true, profile: 2 }]
  });

  await expect(
    workflow.runProfileRpc('save_profile', {
      busyLabel: 'Saving profile...',
      successLabel: 'Profile saved',
      successCopy: 'Slot C archived'
    })
  ).resolves.toMatchObject({ ok: true });

  expect(calls.map((entry) => entry.type)).toEqual([
    'refreshProfileControls',
    'apply',
    'sendRpc',
    'setActiveProfileSlot',
    'refreshProfileControls',
    'refreshProfileUtilities'
  ]);
  expect(calls[2]).toMatchObject({
    payload: { rpc: 'save_profile', slot: 1 },
    options: { timeoutMs: 123 }
  });
  expect(statuses).toEqual([
    { level: 'warn', label: 'Applying staged edits…', message: 'Slot B • syncing before save' },
    { level: 'warn', label: 'Saving profile...', message: 'Slot B • busy' },
    { level: 'ok', label: 'Profile saved', message: 'Slot C archived' }
  ]);
});

test('profile workflow clears pending patches and refreshes config after loading', async () => {
  const config = { slots: [{ value: 64 }] };
  const { calls, workflow } = createHarness({
    rpcResponses: [{ profile_loaded: true, profile: 3 }, { config }]
  });

  await expect(
    workflow.runProfileRpc('load_profile', {
      busyLabel: 'Switching profile...',
      successLabel: 'Profile switched',
      expectConfig: true
    })
  ).resolves.toMatchObject({ ok: true });

  expect(calls.map((entry) => entry.type)).toEqual([
    'refreshProfileControls',
    'clearPendingPatches',
    'sendRpc',
    'setActiveProfileSlot',
    'sendRpc',
    'hydrateAuthoritativeConfig',
    'refreshProfileControls',
    'refreshProfileUtilities'
  ]);
  expect(calls[4]).toMatchObject({ payload: { rpc: 'get_config' }, options: { timeoutMs: 123 } });
  expect(calls[5]).toEqual({ type: 'hydrateAuthoritativeConfig', config });
});

for (const scenario of [
  {
    name: 'Apply reports verified device state without applying the draft',
    applyResult: { applied: false, verifiedDeviceState: true },
    dirtyAfterApply: true
  },
  {
    name: 'Apply succeeds while a newer draft remains staged',
    applyResult: { applied: true },
    dirtyAfterApply: true
  }
]) {
  test(`profile workflow does not save when ${scenario.name}`, async () => {
    const { calls, statuses, workflow } = createHarness({
      dirty: true,
      applyResult: scenario.applyResult,
      dirtyAfterApply: scenario.dirtyAfterApply
    });

    await expect(
      workflow.runProfileRpc('save_profile', {
        busyLabel: 'Saving profile...',
        successLabel: 'Profile saved'
      })
    ).resolves.toMatchObject({ ok: false, reason: 'apply_failed' });

    expect(calls.some((entry) => entry.type === 'sendRpc')).toBe(false);
    expect(statuses.at(-1)).toEqual({
      level: 'err',
      label: 'Apply failed',
      message: 'Profile save requires a successfully applied configuration with no newer staged edits.'
    });
  });
}

test('profile workflow saves after a clean Apply verified by readback', async () => {
  const { calls, workflow } = createHarness({
    dirty: true,
    applyResult: { applied: true, verifiedBy: 'readback' },
    dirtyAfterApply: false,
    rpcResponses: [{ profile_saved: true, profile: 1 }]
  });

  await expect(
    workflow.runProfileRpc('save_profile', {
      busyLabel: 'Saving profile...',
      successLabel: 'Profile saved'
    })
  ).resolves.toMatchObject({ ok: true });
  expect(calls.some((entry) => entry.type === 'sendRpc')).toBe(true);
});

test('profile workflow fails closed for unsupported and offline actions', async () => {
  const unsupported = createHarness({ capabilities: { profileSave: false }, dirty: true });
  await expect(
    unsupported.workflow.runProfileRpc('save_profile', {
      busyLabel: 'Saving profile...',
      successLabel: 'Profile saved'
    })
  ).resolves.toEqual({ ok: false, reason: 'unsupported' });
  expect(unsupported.calls).toEqual([]);
  expect(unsupported.statuses[0]).toMatchObject({
    level: 'warn',
    label: 'Profile action unavailable'
  });

  const offline = createHarness({ interactable: false });
  await expect(
    offline.workflow.runProfileRpc('load_profile', {
      busyLabel: 'Switching profile...',
      successLabel: 'Profile switched'
    })
  ).resolves.toEqual({ ok: false, reason: 'offline' });
  expect(offline.calls).toEqual([]);
  expect(offline.statuses[0]).toEqual({
    level: 'warn',
    label: 'Profile offline',
    message: 'Connect to the deck before using profiles.'
  });
});
