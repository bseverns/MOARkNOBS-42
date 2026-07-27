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
  const rpcPayloads = [];
  const runtime = createLiveControlsRuntime({
    emit: (event, payload) => events.push({ event, payload }),
    sendRpc: sendRpc ?? (async (payload) => {
      rpcPayloads.push(payload);
      return { ok: true };
    }),
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

  return { events, lines, rpcPayloads, runtime, staged };
}

test('live controls runtime queues macro commands through the shared RPC lane', async () => {
  const { events, lines, rpcPayloads, runtime } = createHarness();

  const pending = runtime.sendMacroCommand('SAVE_MACRO_SLOT');

  expect(lines).toEqual([]);
  expect(rpcPayloads).toEqual([{ rpc: 'macro_command', command: 'SAVE_MACRO_SLOT' }]);
  expect(runtime.handleMacroLine({ macro_saved: true, macro_available: true })).toBe(true);

  await expect(pending).resolves.toEqual({ ok: true });
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

test('live controls runtime delegates macro serialization to the shared RPC lane', async () => {
  const { rpcPayloads, runtime } = createHarness();

  const pending = runtime.sendMacroCommand('SAVE_MACRO_SLOT');

  await runtime.sendMacroCommand('RECALL_MACRO_SLOT');
  expect(rpcPayloads).toEqual([
    { rpc: 'macro_command', command: 'SAVE_MACRO_SLOT' },
    { rpc: 'macro_command', command: 'RECALL_MACRO_SLOT' }
  ]);
  runtime.handleMacroLine({ macro_saved: true });
  await pending;
});

test('live controls runtime queues scene requests through the shared RPC lane', async () => {
  const { events, lines, rpcPayloads, runtime } = createHarness();

  const pending = runtime.requestScenes();

  expect(lines).toEqual([]);
  expect(rpcPayloads).toEqual([{ rpc: 'scene_command', payload: { cmd: 'GET_SCENES' } }]);
  expect(
    runtime.handleSceneLine({
      scenes: [{ slot: '2', name: 'Intro', available: 1 }, { slot: 'bad' }]
    })
  ).toBe(true);

  await expect(pending).resolves.toEqual({ ok: true });
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
