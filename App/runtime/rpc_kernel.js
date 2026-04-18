export function createRpcKernel({
  getTransport,
  isJsonRpcTransport,
  chunkString,
  nativeSetAllChunkSize,
  rpcTimeoutMs,
  rpcThrottleIntervalMs,
  onFatalError
} = {}) {
  let rpcSeq = 0;
  const rpcQueue = [];
  let rpcBusy = false;
  let lastRpcTimestamp = 0;
  const pendingRpc = new Map();
  let activeRpcId = null;

  const now = () =>
    typeof performance !== 'undefined' && typeof performance.now === 'function'
      ? performance.now()
      : Date.now();

  function createCompletionLatch() {
    let released = false;
    let resolveLatch = () => {};
    const promise = new Promise((resolve) => {
      resolveLatch = resolve;
    });
    return {
      promise,
      release() {
        if (released) return;
        released = true;
        resolveLatch();
      }
    };
  }

  function buildNativeRpcRequest(message) {
    switch (message.rpc) {
      case 'hello':
        return { kind: 'hello', lines: ['HELLO'] };
      case 'get_manifest':
        return { kind: 'manifest', lines: ['GET_MANIFEST'] };
      case 'get_config':
        return { kind: 'config', lines: ['GET_CONFIG'] };
      case 'get_schema':
        return { kind: 'schema', lines: ['GET_SCHEMA'] };
      case 'save_profile':
        return { kind: 'profile_save', lines: [`SAVE_PROFILE,${Number(message.slot) || 0}`] };
      case 'load_profile':
        return { kind: 'profile_load', lines: [`LOAD_PROFILE,${Number(message.slot) || 0}`] };
      case 'reset_profile':
        return { kind: 'profile_reset', lines: [`RESET_PROFILE,${Number(message.slot) || 0}`] };
      case 'set_config': {
        const payload = JSON.stringify({
          seq: message.seq,
          checksum: message.checksum,
          config: message.config
        });
        return {
          kind: 'ack',
          lines: chunkString(payload, nativeSetAllChunkSize).map((chunk) => `SET_ALL ${chunk}`)
        };
      }
      default:
        return null;
    }
  }

  function getActivePendingRpc() {
    if (activeRpcId === null) return null;
    const entry = pendingRpc.get(activeRpcId);
    if (!entry) return null;
    return { id: activeRpcId, ...entry };
  }

  function handleRpcResponse(msg) {
    const pending = pendingRpc.get(msg.id);
    if (!pending) return;
    if (activeRpcId === msg.id) {
      activeRpcId = null;
    }
    if (pending.timer) {
      clearTimeout(pending.timer);
      pending.timer = null;
    }
    pendingRpc.delete(msg.id);
    if (msg.error) {
      const error = new Error(msg.error.message ?? 'RPC error');
      if (msg.error.code !== undefined) error.code = msg.error.code;
      pending.reject(error);
      pending.release?.();
      return;
    }
    const response = Object.prototype.hasOwnProperty.call(msg, 'result') ? msg.result : msg;
    pending.resolve(response);
    pending.release?.();
  }

  function flushPending(error) {
    activeRpcId = null;
    rpcQueue.length = 0;
    for (const [id, pending] of pendingRpc) {
      if (pending.timer) clearTimeout(pending.timer);
      pending.release?.();
      pending.reject(error);
      pendingRpc.delete(id);
    }
  }

  async function processRpcQueue() {
    if (rpcBusy || !rpcQueue.length) return;
    rpcBusy = true;
    try {
      while (rpcQueue.length) {
        const transport = getTransport?.();
        if (!transport) break;
        const message = rpcQueue[0];
        const entry = pendingRpc.get(message.id);
        if (!entry) {
          rpcQueue.shift();
          continue;
        }
        const elapsed = now() - lastRpcTimestamp;
        const wait = Math.max(0, rpcThrottleIntervalMs - elapsed);
        if (wait) {
          await new Promise((resolve) => setTimeout(resolve, wait));
        }
        try {
          activeRpcId = message.id;
          if (entry.protocolMode === 'native') {
            entry.nativeRequest = buildNativeRpcRequest(message);
            if (!entry.nativeRequest) {
              throw new Error(`Unsupported device RPC: ${message.rpc}`);
            }
            for (const line of entry.nativeRequest.lines) {
              await transport.writeLine(line);
            }
          } else {
            await transport.writeLine(JSON.stringify(message));
          }
        } catch (err) {
          if (activeRpcId === message.id) {
            activeRpcId = null;
          }
          if (entry.timer) {
            clearTimeout(entry.timer);
            entry.timer = null;
          }
          pendingRpc.delete(message.id);
          entry.release?.();
          entry.reject(err);
          flushPending(err);
          if (typeof onFatalError === 'function') onFatalError(err);
          break;
        }
        lastRpcTimestamp = now();
        entry.timer = setTimeout(() => {
          const pending = pendingRpc.get(message.id);
          if (!pending) return;
          if (activeRpcId === message.id) {
            activeRpcId = null;
          }
          if (pending.timer) {
            clearTimeout(pending.timer);
            pending.timer = null;
          }
          pending.release?.();
          pendingRpc.delete(message.id);
          pending.reject(new Error('RPC timeout'));
        }, entry.timeoutMs);
        await entry.completion;
        rpcQueue.shift();
      }
    } finally {
      rpcBusy = false;
    }
  }

  function sendRpc(payload, { timeoutMs } = {}) {
    if (!payload || typeof payload !== 'object' || !payload.rpc) {
      return Promise.reject(new Error('RPC payload must include rpc property'));
    }
    const transport = getTransport?.();
    if (!transport) return Promise.reject(new Error('Not connected'));
    if (!isJsonRpcTransport() && payload.rpc === 'set_param') {
      return Promise.resolve({ deferred: true, path: payload.path, value: payload.value });
    }
    const id = ++rpcSeq;
    const message = { ...payload, id };
    return new Promise((resolve, reject) => {
      const completion = createCompletionLatch();
      const entry = {
        resolve,
        reject,
        timeoutMs: Number.isFinite(Number(timeoutMs)) ? Number(timeoutMs) : rpcTimeoutMs,
        timer: null,
        completion: completion.promise,
        release: completion.release,
        protocolMode: isJsonRpcTransport() ? 'json-rpc' : 'native',
        nativeRequest: null
      };
      pendingRpc.set(id, entry);
      rpcQueue.push(message);
      processRpcQueue();
    });
  }

  return {
    sendRpc,
    flushPending,
    handleRpcResponse,
    getActivePendingRpc
  };
}
