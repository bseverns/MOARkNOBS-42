const fs = require('node:fs');

function normalizeSerialPath(path) {
  const value = String(path || '').trim();
  if (
    value.startsWith('/dev/tty.usbmodem') ||
    value.startsWith('/dev/tty.usbserial')
  ) {
    const calloutPath = value.replace('/dev/tty.', '/dev/cu.');
    if (fs.existsSync(calloutPath)) {
      return calloutPath;
    }
  }
  return value;
}

function createSerialTransportLifecycle({
  getSerialApi,
  config,
  serialBaud,
  reconnectDelaysMs = [1000, 1000, 2000, 3000, 5000, 8000, 15000],
  getRuntimeState,
  getSerial,
  setSerial,
  setParser,
  getReconnectTimer,
  setReconnectTimer,
  pushLog,
  onSerialOpen,
  onSerialError,
  onSerialClose,
  onSerialLine,
}) {
  let reconnectAttempt = 0;

  function scheduleReconnect() {
    const state = getRuntimeState();
    if (state.stopping || state.manualStop || getReconnectTimer()) return;
    const delayMs = reconnectDelaysMs[Math.min(reconnectAttempt, reconnectDelaysMs.length - 1)];
    reconnectAttempt += 1;
    pushLog('warn', `serial reconnect scheduled in ${delayMs}ms`);
    const timer = setTimeout(() => {
      setReconnectTimer(null);
      const current = getRuntimeState();
      if (current.stopping || current.manualStop || !current.running) return;
      pushLog('info', 'serial reopen attempt');
      const serial = getSerial();
      if (serial && typeof serial.open === 'function') {
        serial.open((err) => {
          if (err) pushLog('error', `serial reconnect failed: ${err.message}`);
        });
        return;
      }
      attachSerial();
    }, delayMs);
    setReconnectTimer(timer);
  }

  function cancelReconnect() {
    const timer = getReconnectTimer();
    if (!timer) return;
    clearTimeout(timer);
    setReconnectTimer(null);
  }

  function resetReconnectBackoff() {
    reconnectAttempt = 0;
  }

  function attachSerial() {
    const serialApi = getSerialApi();
    const { SerialPort, ReadlineParser } = serialApi || {};
    const serialPath = normalizeSerialPath(config.serialName);
    const serial = new SerialPort({
      path: serialPath,
      baudRate: serialBaud,
    });
    setSerial(serial);

    const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
    setParser(parser);

    serial.on('open', () => {
      resetReconnectBackoff();
      onSerialOpen?.(serial);
    });

    serial.on('error', (err) => {
      onSerialError?.(err);
    });

    serial.on('close', () => {
      onSerialClose?.();
    });

    parser.on('data', onSerialLine);
  }

  return {
    scheduleReconnect,
    cancelReconnect,
    resetReconnectBackoff,
    attachSerial,
  };
}

module.exports = {
  createSerialTransportLifecycle,
  normalizeSerialPath,
};
