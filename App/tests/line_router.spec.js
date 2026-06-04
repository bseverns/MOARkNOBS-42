import { test, expect } from '@playwright/test';

import { createRuntimeLineHandler } from '../runtime/line_router.js';

function createHarness(nativeRequest = { kind: 'profile_set' }) {
  const events = [];
  const responses = [];
  const rpcKernel = {
    getActivePendingRpc: () => ({
      id: 7,
      protocolMode: 'native',
      nativeRequest
    }),
    handleRpcResponse: (response) => responses.push(response)
  };
  const handleLine = createRuntimeLineHandler({
    emit: (event, payload) => events.push({ event, payload }),
    notifyStatus: (payload) => events.push({ event: 'status', payload }),
    rpcKernel,
    handleSceneLine: () => false,
    handleMacroLine: () => false,
    isManifestPayload: () => false,
    isConfigPayload: () => false,
    applyConfigPatch: () => {},
    extractSlotIndex: () => null,
    onTelemetry: () => {}
  });
  return { handleLine, events, responses };
}

test('native line router turns old firmware profile chunk text into an RPC error', () => {
  const { handleLine, events, responses } = createHarness();

  handleLine('Unknown command: SET_PROFILE_CHUNK');

  expect(events).toEqual([]);
  expect(responses).toEqual([
    {
      id: 7,
      error: {
        message:
          'Firmware does not support chunked profile saves. Flash the latest firmware, then reconnect.'
      }
    }
  ]);
});

test('native line router still logs unrelated plain text', () => {
  const { handleLine, events, responses } = createHarness(null);

  handleLine('Boot complete');

  expect(responses).toEqual([]);
  expect(events).toEqual([{ event: 'log', payload: 'Boot complete' }]);
});
