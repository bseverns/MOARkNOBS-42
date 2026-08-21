export function createRpcSender({ rpcKernel, getConfigSession }) {
  return function sendRpc(payload, { timeoutMs, rollbackPolicy = 'none' } = {}) {
    if (!['none', 'staged-config'].includes(rollbackPolicy)) {
      throw new Error(`Unsupported RPC rollback policy: ${rollbackPolicy}`);
    }
    const request = rpcKernel.sendRpc(payload, { timeoutMs });
    if (rollbackPolicy === 'none') return request;

    // Staged-config rollback is deliberately opt-in. Profile, live-control, and read RPCs
    // must not discard unrelated configuration edits when they fail.
    return request.catch(async (err) => {
      try {
        await getConfigSession()?.rollback();
      } catch (rollbackErr) {
        console.debug('rollback failed', rollbackErr);
      }
      throw err;
    });
  };
}

