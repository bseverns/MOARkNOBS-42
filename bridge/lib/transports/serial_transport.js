function createSerialTransportLifecycle({
  getSerialApi,
  config,
  serialBaud,
  reconnectDelayMs = 1000,
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
  function scheduleReconnect() {
    const state = getRuntimeState();
    if (state.stopping || state.manualStop || getReconnectTimer()) return;
    const timer = setTimeout(() => {
      setReconnectTimer(null);
      const current = getRuntimeState();
      if (current.stopping || current.manualStop || !current.running) return;
      const serial = getSerial();
      if (serial && typeof serial.open === 'function') {
        serial.open((err) => {
          if (err) pushLog('error', `serial reconnect failed: ${err.message}`);
        });
        return;
      }
      attachSerial();
    }, reconnectDelayMs);
    setReconnectTimer(timer);
  }

  function cancelReconnect() {
    const timer = getReconnectTimer();
    if (!timer) return;
    clearTimeout(timer);
    setReconnectTimer(null);
  }

  function attachSerial() {
    const serialApi = getSerialApi();
    const { SerialPort, ReadlineParser } = serialApi || {};
    const serial = new SerialPort({
      path: config.serialName,
      baudRate: serialBaud,
    });
    setSerial(serial);

    const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
    setParser(parser);

    serial.on('open', () => {
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
    attachSerial,
  };
}

module.exports = {
  createSerialTransportLifecycle,
};
