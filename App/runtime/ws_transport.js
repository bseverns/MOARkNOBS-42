// Bridge websocket transport shim so the App can reuse the same runtime contract off-WebSerial.
export function createWebSocketTransport(url) {
  let socket = null;
  let queue = [];
  let resolver = null;
  let buffer = '';
  let closed = false;
  const maxQueuedLines = 512;
  const maxQueuedBytes = 1024 * 1024;
  let queuedBytes = 0;
  let droppedLines = 0;
  const decoder = typeof TextDecoder === 'function' ? new TextDecoder() : null;

  const enqueueLine = (line) => {
    const bytes = new TextEncoder().encode(line).byteLength;
    const isTelemetry = (candidate) => {
      try {
        const message = JSON.parse(candidate);
        return message?.type === 'telemetry' || message?.telemetry !== undefined;
      } catch (_) { return false; }
    };
    while (queue.length >= maxQueuedLines || queuedBytes + bytes > maxQueuedBytes) {
      const index = queue.findIndex(isTelemetry);
      if (index < 0) break;
      queuedBytes -= new TextEncoder().encode(queue[index]).byteLength;
      queue.splice(index, 1);
      droppedLines += 1;
    }
    if ((queue.length >= maxQueuedLines || queuedBytes + bytes > maxQueuedBytes) && isTelemetry(line)) {
      droppedLines += 1;
      return;
    }
    queue.push(line);
    queuedBytes += bytes;
    if (resolver) {
      const pending = resolver;
      resolver = null;
      const next = queue.shift();
      queuedBytes -= new TextEncoder().encode(next).byteLength;
      pending.resolve(next);
    }
  };

  const flushBuffer = () => {
    if (!buffer) return;
    const trimmed = buffer.trim();
    buffer = '';
    if (trimmed) enqueueLine(trimmed);
  };

  const handleMessage = (event) => {
    if (!event || closed) return;
    const data = event.data;
    const text = typeof data === 'string' ? data : decoder?.decode(data) ?? '';
    if (!text) return;
    buffer += text;
    let index;
    while ((index = buffer.indexOf('\n')) >= 0) {
      const segment = buffer.slice(0, index);
      buffer = buffer.slice(index + 1);
      const trimmed = segment.trim();
      if (trimmed) enqueueLine(trimmed);
    }
  };

  const handleClose = () => {
    closed = true;
    flushBuffer();
    if (resolver) {
      const pending = resolver;
      resolver = null;
      pending.reject(new Error('WebSocket closed'));
    }
  };

  function open() {
    if (socket) {
      if (socket.readyState === WebSocket.OPEN) return Promise.resolve();
      if (socket.readyState === WebSocket.CONNECTING) {
        return new Promise((resolve, reject) => {
          const cleanup = () => {
            socket.removeEventListener('open', onOpen);
            socket.removeEventListener('error', onError);
          };
          const onOpen = () => {
            cleanup();
            resolve();
          };
          const onError = () => {
            cleanup();
            reject(new Error('WebSocket error'));
          };
          socket.addEventListener('open', onOpen);
          socket.addEventListener('error', onError);
        });
      }
    }
    if (typeof WebSocket === 'undefined') return Promise.reject(new Error('WebSocket unsupported'));
    closed = false;
    buffer = '';
    queue = [];
    queuedBytes = 0;
    socket = new WebSocket(url);
    socket.binaryType = 'arraybuffer';
    return new Promise((resolve, reject) => {
      const onOpen = () => {
        socket.addEventListener('message', handleMessage);
        socket.addEventListener('close', handleClose);
        resolve();
      };
      const onError = () => {
        handleClose();
        reject(new Error('WebSocket error'));
      };
      socket.addEventListener('open', onOpen, { once: true });
      socket.addEventListener('error', onError, { once: true });
    });
  }

  function writeLine(line) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      return Promise.reject(new Error('WebSocket not connected'));
    }
    socket.send(`${typeof line === 'string' ? line.trim() : String(line)}\n`);
    return Promise.resolve();
  }

  function nextLine() {
    if (queue.length) {
      const line = queue.shift();
      queuedBytes -= new TextEncoder().encode(line).byteLength;
      return Promise.resolve(line);
    }
    if (closed) {
      return Promise.reject(new Error('WebSocket closed'));
    }
    return new Promise((resolve, reject) => {
      resolver = { resolve, reject };
    });
  }

  function close() {
    if (!socket) return Promise.resolve();
    if (socket.readyState === WebSocket.CLOSING || socket.readyState === WebSocket.CLOSED) {
      return Promise.resolve();
    }
    socket.close();
    return Promise.resolve();
  }

  return {
    open,
    writeLine,
    nextLine,
    close,
    rawPort: { getInfo: () => null },
    protocol: 'native'
    ,getDropStats: () => ({ droppedLines, queuedLines: queue.length, queuedBytes })
  };
}
