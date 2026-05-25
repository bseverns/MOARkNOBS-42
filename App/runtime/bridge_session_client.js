function createBridgeError(payload, fallbackMessage) {
  const body = payload?.error ?? payload ?? {};
  const error = new Error(body.message ?? fallbackMessage ?? 'Bridge request failed');
  if (body.code !== undefined) error.code = body.code;
  if (body.details !== undefined) error.details = body.details;
  return error;
}

function resolveUrl(url, base) {
  if (!url) return null;
  try {
    return new URL(url, base).toString();
  } catch (_) {
    return null;
  }
}

export function createBridgeSessionClient({
  baseUrl,
  eventUrl,
  fetchImpl = typeof fetch === 'function' ? fetch.bind(globalThis) : null,
  WebSocketImpl = typeof WebSocket === 'function' ? WebSocket : null
} = {}) {
  if (!baseUrl) throw new Error('Bridge session baseUrl is required');
  if (typeof fetchImpl !== 'function') throw new Error('fetch is required for bridge session mode');

  const base = resolveUrl(
    baseUrl,
    typeof window !== 'undefined' ? window.location.href : undefined
  );
  if (!base) throw new Error('Invalid bridge session baseUrl');

  const resolvedEventUrl =
    resolveUrl(eventUrl, typeof window !== 'undefined' ? window.location.href : undefined) ?? null;

  let socket = null;
  let socketClosed = false;
  let buffer = '';
  const decoder = typeof TextDecoder === 'function' ? new TextDecoder() : null;

  async function requestJson(path, { method = 'GET', body } = {}) {
    const response = await fetchImpl(resolveUrl(path, base), {
      method,
      headers: body === undefined ? undefined : { 'content-type': 'application/json' },
      body: body === undefined ? undefined : JSON.stringify(body)
    });

    const text = await response.text();
    let payload = null;
    if (text) {
      try {
        payload = JSON.parse(text);
      } catch (_) {
        payload = null;
      }
    }

    if (!response.ok) {
      throw createBridgeError(payload, `Bridge request failed: ${response.status}`);
    }
    return payload ?? {};
  }

  async function getSession({ warm = false } = {}) {
    const suffix = warm ? '?warm=1' : '';
    const payload = await requestJson(`/api/device/session${suffix}`);
    return payload.session ?? null;
  }

  async function stageConfig(config) {
    const payload = await requestJson('/api/device/stage', {
      method: 'POST',
      body: { config }
    });
    return payload.result ?? null;
  }

  async function applyConfig(body = {}) {
    const payload = await requestJson('/api/device/apply', {
      method: 'POST',
      body
    });
    return payload.result ?? null;
  }

  async function rollbackConfig(reason = 'operator_request') {
    const payload = await requestJson('/api/device/rollback', {
      method: 'POST',
      body: { reason }
    });
    return payload.result ?? null;
  }

  function closeEvents() {
    socketClosed = true;
    if (!socket) return;
    if (socket.readyState === WebSocketImpl.CLOSING || socket.readyState === WebSocketImpl.CLOSED) {
      socket = null;
      return;
    }
    socket.close();
    socket = null;
  }

  function openEvents({ onEvent, onClose, onError } = {}) {
    if (!resolvedEventUrl) {
      return Promise.reject(new Error('Bridge eventUrl is required'));
    }
    if (typeof WebSocketImpl !== 'function') {
      return Promise.reject(new Error('WebSocket unsupported'));
    }
    if (socket && socket.readyState <= 1) {
      return Promise.resolve();
    }

    socketClosed = false;
    buffer = '';
    socket = new WebSocketImpl(resolvedEventUrl);
    socket.binaryType = 'arraybuffer';

    const flushBufferedEvent = () => {
      const trimmed = buffer.trim();
      buffer = '';
      if (!trimmed) return;
      try {
        onEvent?.(JSON.parse(trimmed));
      } catch (err) {
        onError?.(err);
      }
    };

    const handleMessage = (event) => {
      const data = event?.data;
      const text = typeof data === 'string' ? data : decoder?.decode(data) ?? '';
      if (!text) return;
      buffer += text;
      let index;
      while ((index = buffer.indexOf('\n')) >= 0) {
        const segment = buffer.slice(0, index).trim();
        buffer = buffer.slice(index + 1);
        if (!segment) continue;
        try {
          onEvent?.(JSON.parse(segment));
        } catch (err) {
          onError?.(err);
        }
      }
    };

    const handleClose = () => {
      if (socketClosed) return;
      socketClosed = true;
      flushBufferedEvent();
      onClose?.();
    };

    return new Promise((resolve, reject) => {
      const cleanup = () => {
        socket?.removeEventListener?.('open', handleOpen);
        socket?.removeEventListener?.('error', handleOpenError);
      };

      const handleOpen = () => {
        cleanup();
        socket?.addEventListener?.('message', handleMessage);
        socket?.addEventListener?.('close', handleClose);
        resolve();
      };

      const handleOpenError = () => {
        cleanup();
        reject(new Error('Bridge event socket failed to open'));
      };

      socket.addEventListener('open', handleOpen, { once: true });
      socket.addEventListener('error', handleOpenError, { once: true });
      socket.addEventListener('error', (event) => {
        onError?.(event instanceof Error ? event : new Error('Bridge event socket error'));
      });
    });
  }

  return {
    getSession,
    stageConfig,
    applyConfig,
    rollbackConfig,
    openEvents,
    closeEvents
  };
}
