// Bridge websocket transport shim so the App can reuse the same runtime contract off-WebSerial.
export function createWebSocketTransport(url) {
  let socket = null;
  let queue = [];
  let resolver = null;
  let buffer = '';
  let closed = false;
  const decoder = typeof TextDecoder === 'function' ? new TextDecoder() : null;

  const enqueueLine = (line) => {
    queue.push(line);
    if (resolver) {
      const pending = resolver;
      resolver = null;
      const next = queue.shift();
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
      return Promise.resolve(queue.shift());
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
  };
}
