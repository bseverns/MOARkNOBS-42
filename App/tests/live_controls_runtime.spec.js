import { test, expect } from '@playwright/test';
import { createLiveControlsRuntime } from '../runtime/live_controls_runtime.js';
import { setNestedValue } from '../runtime/runtime_utils.js';

function createHarness({ sendRpc } = {}) {
  const events = [];
  const lines = [];
  const staged = {};
  const transport = {
    writeLine: async (line) => {
      lines.push(line);
    }
  };
  const runtime = createLiveControlsRuntime({
    emit: (event, payload) => events.push({ event, payload }),
    sendRpc: sendRpc ?? (async () => ({ ok: true })),
    getTransport: () => transport,
    configSession: {
      stage: (updater) => updater(staged)
    },
    setNestedValue,
    ensureConfigBootTransport: async () => {},
    disconnect: async () => {},
    getUseSimulator: () => false,
    macroResponseKeys: {
      SAVE_MACRO_SLOT: 'macro_saved',
      RECALL_MACRO_SLOT: 'macro_recalled'
    },
    macroCommandTimeoutMs: 500,
    sceneResponseKeys: {
      SAVE_SCENE: 'scene_saved',
      RECALL_SCENE: 'scene_recalled',
      GET_SCENES: 'scenes'
    },
    sceneCommandTimeoutMs: 500
  });

  return { events, lines, runtime, staged };
}

test('live controls runtime resolves macro commands from firmware receipt lines', async () => {
  const { events, lines, runtime } = createHarness();

  const pending = runtime.sendMacroCommand('SAVE_MACRO_SLOT');

  expect(lines).toEqual(['SAVE_MACRO_SLOT']);
  expect(runtime.handleMacroLine({ macro_saved: true, macro_available: true })).toBe(true);

  await expect(pending).resolves.toEqual({ macro_saved: true, macro_available: true });
  expect(events).toEqual([
    {
      event: 'macro',
      payload: {
        saved: true,
        recalled: false,
        available: true,
        raw: { macro_saved: true, macro_available: true }
      }
    }
  ]);
});

test('live controls runtime enforces single-flight macro commands', async () => {
  const { runtime } = createHarness();

  const pending = runtime.sendMacroCommand('SAVE_MACRO_SLOT');

  await expect(runtime.sendMacroCommand('RECALL_MACRO_SLOT')).rejects.toThrow(
    'Macro command already in progress'
  );
  runtime.handleMacroLine({ macro_saved: true });
  await pending;
});

test('live controls runtime normalizes scene list receipts', async () => {
  const { events, lines, runtime } = createHarness();

  const pending = runtime.requestScenes();

  expect(lines).toEqual([JSON.stringify({ cmd: 'GET_SCENES' })]);
  expect(
    runtime.handleSceneLine({
      scenes: [{ slot: '2', name: 'Intro', available: 1 }, { slot: 'bad' }]
    })
  ).toBe(true);

  await expect(pending).resolves.toEqual({
    scenes: [{ slot: '2', name: 'Intro', available: 1 }, { slot: 'bad' }]
  });
  expect(events).toEqual([
    {
      event: 'scene',
      payload: {
        type: 'list',
        scenes: [
          { slot: 2, name: 'Intro', available: true },
          { slot: 0, name: '', available: false }
        ]
      }
    }
  ]);
});

test('live controls runtime stages live patch paths before sending set_param RPC', async () => {
  const rpcPayloads = [];
  const { runtime, staged } = createHarness({
    sendRpc: async (payload) => {
      rpcPayloads.push(payload);
      return { applied: true };
    }
  });

  await expect(runtime.applyPatch('slots.0.value', 99)).resolves.toEqual({ applied: true });

  expect(staged).toEqual({ slots: [{ value: 99 }] });
  expect(rpcPayloads).toEqual([{ rpc: 'set_param', path: 'slots.0.value', value: 99 }]);
});
