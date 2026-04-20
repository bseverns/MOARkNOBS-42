// Native serial transport wrapper that exposes the same API as simulator and WS bridge.
export function createTransportPort(port, options = {}, transportDeps = {}) {
  const { makeEncoder = () => new TextEncoder(), makeDecoder = () => new TextDecoder() } =
    transportDeps;
  const textEncoder = makeEncoder();
  const textDecoder = makeDecoder();
  let reader;
  let writer;
  let active = true;
  const lineQueue = [];
  const waiters = [];

  async function open() {
    await port.open({ baudRate: 115200, ...options });
    const decoderStream = new TextDecoderStream();
    port.readable.pipeTo(decoderStream.writable);
    reader = decoderStream.readable.getReader();
    writer = port.writable.getWriter();
    readLoop();
  }

  async function readLoop() {
    let buffer = '';
    try {
      while (active) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += value;
        let idx;
        while ((idx = buffer.indexOf('\n')) >= 0) {
          const line = buffer.slice(0, idx).trim();
          buffer = buffer.slice(idx + 1);
          if (!line) continue;
          if (waiters.length) waiters.shift().resolve(line);
          else lineQueue.push(line);
        }
      }
    } catch (err) {
      console.error('transport read error', err);
    } finally {
      active = false;
      while (waiters.length) waiters.shift().reject(new Error('Native port closed'));
    }
  }

  async function writeLine(line) {
    if (!writer) throw new Error('Writer unavailable');
    await writer.write(textEncoder.encode(line + '\n'));
  }

  function nextLine() {
    if (!active) return Promise.reject(new Error('Native port closed'));
    if (lineQueue.length) return Promise.resolve(lineQueue.shift());
    return new Promise((resolve, reject) => waiters.push({ resolve, reject }));
  }

  async function close() {
    active = false;
    while (waiters.length) waiters.shift().reject(new Error('Native port closed'));
    try {
      reader?.cancel();
    } catch (err) {
      console.debug('reader cancel', err);
    }
    try {
      writer?.close();
    } catch (err) {
      console.debug('writer close', err);
    }
    try {
      await port.close();
    } catch (err) {
      console.debug('port close', err);
    }
  }

  return { open, writeLine, nextLine, close, rawPort: port, protocol: 'native', textDecoder };
}
