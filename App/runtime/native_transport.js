// Native serial transport wrapper that exposes the same API as simulator and WS bridge.
export function createTransportPort(port, options = {}, transportDeps = {}) {
  const { makeEncoder = () => new TextEncoder(), makeDecoder = () => new TextDecoder() } =
    transportDeps;
  const textEncoder = makeEncoder();
  const textDecoder = makeDecoder();
  let reader;
  let writer;
  let pipeClosed;
  let active = true;
  const lineQueue = [];
  const waiters = [];
  const maxQueuedLines = 512;
  const maxQueuedBytes = 1024 * 1024;
  let queuedBytes = 0;
  let droppedLines = 0;

  function isDroppableTelemetry(line) {
    try {
      const message = JSON.parse(line);
      return message?.type === 'telemetry' || message?.telemetry !== undefined;
    } catch (_) {
      return false;
    }
  }

  function enqueueLine(line) {
    const bytes = new TextEncoder().encode(line).byteLength;
    while (
      lineQueue.length >= maxQueuedLines || queuedBytes + bytes > maxQueuedBytes
    ) {
      const index = lineQueue.findIndex(isDroppableTelemetry);
      if (index < 0) break; // Never discard ACK/error/contract traffic.
      const [dropped] = lineQueue.splice(index, 1);
      queuedBytes -= new TextEncoder().encode(dropped).byteLength;
      droppedLines += 1;
    }
    if (lineQueue.length >= maxQueuedLines || queuedBytes + bytes > maxQueuedBytes) {
      if (isDroppableTelemetry(line)) {
        droppedLines += 1;
        return;
      }
      // Critical messages have a reserved place only while the bounded queue
      // can hold them. Do not turn a malfunctioning device into unbounded
      // browser memory growth: fail the transport visibly instead.
      throw new Error('Native transport critical receive queue exhausted');
    }
    lineQueue.push(line);
    queuedBytes += bytes;
  }

  async function open() {
    if (port.readable || port.writable) {
      await port.close();
    }
    active = true;
    await port.open({ baudRate: 115200, ...options });
    const decoderStream = new TextDecoderStream();
    pipeClosed = port.readable.pipeTo(decoderStream.writable).catch((err) => {
      if (active) throw err;
    });
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
          else enqueueLine(line);
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
    if (lineQueue.length) {
      const line = lineQueue.shift();
      queuedBytes -= new TextEncoder().encode(line).byteLength;
      return Promise.resolve(line);
    }
    return new Promise((resolve, reject) => waiters.push({ resolve, reject }));
  }

  async function close() {
    active = false;
    while (waiters.length) waiters.shift().reject(new Error('Native port closed'));
    try {
      await reader?.cancel();
    } catch (err) {
      console.debug('reader cancel', err);
    } finally {
      try {
        reader?.releaseLock?.();
      } catch (err) {
        console.debug('reader release', err);
      }
      reader = null;
    }
    try {
      await pipeClosed;
    } catch (err) {
      console.debug('decoder pipe close', err);
    } finally {
      pipeClosed = null;
    }
    try {
      await writer?.close();
    } catch (err) {
      console.debug('writer close', err);
    } finally {
      try {
        writer?.releaseLock?.();
      } catch (err) {
        console.debug('writer release', err);
      }
      writer = null;
    }
    try {
      if (port.readable || port.writable) {
        await port.close();
      }
    } catch (err) {
      console.debug('port close', err);
    }
  }

  return {
    open, writeLine, nextLine, close, rawPort: port, protocol: 'native', textDecoder,
    getDropStats: () => ({ droppedLines, queuedLines: lineQueue.length, queuedBytes })
  };
}
