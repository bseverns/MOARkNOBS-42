import { test, expect } from '@playwright/test';

import { createRpcKernel } from '../runtime/rpc_kernel.js';
import { chunkString } from '../runtime/runtime_utils.js';

test('native set_profile uses chunked serial lines under firmware buffer limit', async () => {
  const writes = [];
  const transport = {
    writeLine: async (line) => {
      writes.push(String(line));
    }
  };
  const kernel = createRpcKernel({
    getTransport: () => transport,
    isJsonRpcTransport: () => false,
    chunkString,
    nativeSetAllChunkSize: 96,
    nativeSetAllLinePaceMs: 0,
    rpcTimeoutMs: 1000,
    rpcThrottleIntervalMs: 0
  });

  const profile = {
    lfos: [
      {
        index: 0,
        shape: 5,
        frequency_hz: 10,
        depth: 0.5,
        bipolar: true,
        sync: true,
        sync_ratio: 2
      },
      {
        index: 1,
        shape: 1,
        frequency_hz: 0.5,
        depth: 0.25,
        bipolar: false,
        sync: false,
        sync_ratio: 0
      }
    ],
    routes: [
      { lfo: 0, type: 0, target: 2, depth: 0.5, amount: 100, min: 0, max: 127 },
      { lfo: 1, type: 4, slot: 12, depth: 0.8, amount: -50, min: 10, max: 100 }
    ]
  };

  const result = kernel.sendRpc({ rpc: 'set_profile', slot: 0, profile });
  await expect.poll(() => writes.length).toBeGreaterThan(1);

  expect(writes.every((line) => line.length < 128)).toBe(true);
  expect(writes.every((line) => line.startsWith('SET_PROFILE_CHUNK,0,'))).toBe(true);
  expect(writes.some((line) => line.startsWith('SET_PROFILE,0,'))).toBe(false);

  const active = kernel.getActivePendingRpc();
  kernel.handleRpcResponse({
    id: active.id,
    result: { type: 'response', status: 'ok', command: 'SET_PROFILE' }
  });
  await expect(result).resolves.toMatchObject({ status: 'ok', command: 'SET_PROFILE' });
});
