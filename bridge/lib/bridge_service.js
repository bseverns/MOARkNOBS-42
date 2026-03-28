const { EventEmitter } = require('node:events');

const SERIAL_BAUD = 115200;
const DEFAULT_OSC_PORT = 9000;
const DEFAULT_HTTP_PORT = 8787;
const LOG_LIMIT = 200;
const MAX_MSG_LEN = 128;

function usageText() {
  return (
    'mn42_bridge.js - link MOARkNOBS-42 to OSC & MIDI\n' +
    'Usage: node mn42_bridge.js [--serial PORT] [--osc PORT] [--osc-listen PORT] [--host ADDR] [--bind ADDR] [--midi LABEL]'
  );
}

function getArg(argv, flag, def) {
  const idx = argv.indexOf(flag);
  if (idx >= 0 && idx + 1 < argv.length) return argv[idx + 1];
  return def;
}

function parsePositiveInt(raw, fallback) {
  const parsed = parseInt(String(raw), 10);
  if (!Number.isInteger(parsed) || parsed <= 0) return fallback;
  return parsed;
}

function parseConfigFromArgv(argv = process.argv) {
  const oscPort = parsePositiveInt(
    getArg(argv, '--osc', getArg(argv, '-o', String(DEFAULT_OSC_PORT))),
    null,
  );
  if (oscPort === null) {
    const error = new Error('bad --osc port');
    error.showUsage = true;
    throw error;
  }

  const oscListen = parsePositiveInt(
    getArg(argv, '--osc-listen', String(DEFAULT_OSC_PORT)),
    DEFAULT_OSC_PORT,
  );

  return {
    serialName: getArg(argv, '--serial', getArg(argv, '-s', '/dev/ttyACM0')),
    oscPort,
    oscListen,
    oscHost: getArg(argv, '--host', getArg(argv, '-H', '127.0.0.1')),
    oscBind: getArg(argv, '--bind', getArg(argv, '-b', '127.0.0.1')),
    midiLabel: getArg(argv, '--midi', getArg(argv, '-m', 'MN42 Bridge')),
    httpPort: parsePositiveInt(
      getArg(argv, '--http-port', String(DEFAULT_HTTP_PORT)),
      DEFAULT_HTTP_PORT,
    ),
    httpHost: getArg(argv, '--http-host', '127.0.0.1'),
  };
}

function validateCmd(m) {
  if (
    !m ||
    m.cmd !== 'SET_SLOT_VALUE' ||
    !Number.isInteger(m.slot) ||
    !Number.isInteger(m.value)
  ) {
    return null;
  }
  if (m.slot < 0 || m.slot > 41 || m.value < 0 || m.value > 127) return null;
  const cmd = { cmd: m.cmd, slot: m.slot, value: m.value };
  if (JSON.stringify(cmd).length > MAX_MSG_LEN) return null;
  return cmd;
}

function formatLiveValueCommand(cmd) {
  return `SET_SLOT_VALUE,${cmd.slot},${cmd.value}`;
}

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function createBridgeService(initialConfig = {}, injected = {}) {
  const events = new EventEmitter();
  const config = {
    serialName: '/dev/ttyACM0',
    oscPort: DEFAULT_OSC_PORT,
    oscListen: DEFAULT_OSC_PORT,
    oscHost: '127.0.0.1',
    oscBind: '127.0.0.1',
    midiLabel: 'MN42 Bridge',
    ...initialConfig,
  };

  let depsLoaded = false;
  let serialApi = injected.serialport || null;
  let oscApi = injected.osc || null;
  let jzzFactory = injected.jzz || null;

  let serial = null;
  let parser = null;
  let udp = null;
  let midi = null;
  let midiOut = null;
  let midiIn = null;
  let reconnectTimer = null;
  let running = false;
  let stopping = false;
  let manualStop = false;

  const state = {
    running: false,
    serialConnected: false,
    ready: false,
    manifest: null,
    lastError: null,
    lastTelemetryAt: null,
    logs: [],
    config: clone(config),
  };

  function emitState() {
    events.emit('state', getState());
  }

  function pushLog(level, message, extra = undefined) {
    const entry = {
      at: new Date().toISOString(),
      level,
      message,
      extra: extra === undefined ? undefined : clone(extra),
    };
    state.logs.push(entry);
    if (state.logs.length > LOG_LIMIT) {
      state.logs.splice(0, state.logs.length - LOG_LIMIT);
    }
    events.emit('log', entry);
    emitState();
  }

  function setState(partial) {
    Object.assign(state, partial);
    emitState();
  }

  function inspectManifest(msg) {
    if (!msg || typeof msg !== 'object') return;
    if (msg.hello === 'mn42') {
      setState({ ready: true });
      return;
    }
    const manifest =
      (msg.result && typeof msg.result === 'object' && msg.result.manifest) ||
      msg.manifest ||
      null;
    if (manifest && typeof manifest === 'object') {
      setState({ manifest: manifest, ready: true });
    }
  }

  function broadcastLine(line) {
    events.emit('line', line);
  }

  function sendOscTelemetry(address, args) {
    if (!udp || typeof udp.send !== 'function') return;
    try {
      udp.send({ address, args }, config.oscHost, config.oscPort);
    } catch (err) {
      pushLog('error', `udp send error: ${err.message}`);
    }
  }

  function sendMidiTelemetry(channelBase, values) {
    if (!midiOut || !Array.isArray(values)) return;
    values.forEach((value, index) => {
      try {
        midiOut.send([channelBase, index, value]);
      } catch (err) {
        pushLog('error', `MIDI out error: ${err.message}`);
      }
    });
  }

  function handleSerialLine(line) {
    const trimmed = String(line || '').trim();
    if (!trimmed) return;
    if (trimmed === '{"hello":"mn42"}') {
      setState({ ready: true, serialConnected: true, lastError: null });
    }
    broadcastLine(trimmed);
    if (trimmed.length > MAX_MSG_LEN) {
      pushLog('warn', 'serial packet too big');
      return;
    }

    let data;
    try {
      data = JSON.parse(trimmed);
    } catch (_) {
      return;
    }

    inspectManifest(data);

    if (data.type === 'telemetry' || data.slots || data.envelopes) {
      setState({ lastTelemetryAt: new Date().toISOString() });
    }

    if (Array.isArray(data.slots)) {
      sendOscTelemetry('/mn42/slots', data.slots);
      sendMidiTelemetry(0xb0, data.slots);
    }
    if (Array.isArray(data.envelopes)) {
      sendOscTelemetry('/mn42/envelopes', data.envelopes);
      sendMidiTelemetry(0xb1, data.envelopes);
    }
  }

  async function loadDeps() {
    if (depsLoaded) return;
    if (!serialApi) serialApi = require('serialport');
    if (!oscApi) oscApi = require('osc');
    if (!jzzFactory) jzzFactory = require('jzz');
    depsLoaded = true;
  }

  async function listSerialPorts() {
    await loadDeps();
    const listFn = serialApi?.SerialPort?.list;
    if (typeof listFn !== 'function') return [];
    try {
      return await listFn();
    } catch (err) {
      pushLog('error', `serial list failed: ${err.message}`);
      return [];
    }
  }

  function scheduleReconnect() {
    if (stopping || manualStop || reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      if (stopping || manualStop || !running) return;
      if (serial && typeof serial.open === 'function') {
        serial.open((err) => {
          if (err) pushLog('error', `serial reconnect failed: ${err.message}`);
        });
        return;
      }
      attachSerial();
    }, 1000);
  }

  function attachSerial() {
    const { SerialPort, ReadlineParser } = serialApi;
    serial = new SerialPort({
      path: config.serialName,
      baudRate: SERIAL_BAUD,
    });
    parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));

    serial.on('open', () => {
      setState({ serialConnected: true, lastError: null });
      pushLog('info', `serial up on ${config.serialName} @${SERIAL_BAUD}`);
      try {
        serial.write('HELLO\n');
      } catch (err) {
        pushLog('error', `HELLO write failed: ${err.message}`);
      }
    });

    serial.on('error', (err) => {
      setState({
        serialConnected: false,
        ready: false,
        lastError: err.message,
      });
      pushLog('error', `serial error: ${err.message}`);
      scheduleReconnect();
    });

    serial.on('close', () => {
      setState({
        serialConnected: false,
        ready: false,
      });
      if (manualStop || stopping) return;
      pushLog('error', 'serial disconnected');
      scheduleReconnect();
    });

    parser.on('data', handleSerialLine);
  }

  function attachOsc() {
    udp = new oscApi.UDPPort({
      localAddress: config.oscBind,
      localPort: config.oscListen,
    });

    udp.on('error', (err) => {
      pushLog('error', `udp error: ${err.message}`);
      try {
        udp.close();
      } catch (_) {
        // no-op
      }
      setTimeout(() => {
        if (!running || stopping || manualStop) return;
        try {
          udp.open();
        } catch (openErr) {
          pushLog('error', `udp reopen failed: ${openErr.message}`);
        }
      }, 1000);
    });

    udp.on('message', (msg) => {
      if (
        !msg ||
        msg.address !== '/mn42/cmd' ||
        !Array.isArray(msg.args) ||
        !msg.args.length
      ) {
        return;
      }
      let data = msg.args[0];
      if (typeof data === 'string') {
        if (data.length > MAX_MSG_LEN) {
          pushLog('warn', 'OSC cmd too big');
          return;
        }
        try {
          data = JSON.parse(data);
        } catch (_) {
          pushLog('warn', 'bad OSC JSON');
          return;
        }
      }
      const cmd = validateCmd(data);
      if (!cmd) {
        pushLog('warn', 'bad OSC cmd', data);
        return;
      }
      sendLine(formatLiveValueCommand(cmd));
    });

    udp.open();
  }

  function attachMidi() {
    midi = jzzFactory();
    midiOut = midi.openMidiOut(config.midiLabel).or(() => {
      pushLog('error', 'MIDI out failed');
      setTimeout(() => {
        if (!running || stopping || manualStop) return;
        attachMidi();
      }, 1000);
    });

    if (midiOut && typeof midiOut.on === 'function') {
      midiOut.on('error', (err) => {
        pushLog('error', `MIDI out error: ${err.message}`);
      });
    }

    midiIn = midi.openMidiIn(config.midiLabel).or(() => {
      pushLog('error', 'MIDI in failed');
      setTimeout(() => {
        if (!running || stopping || manualStop) return;
        attachMidi();
      }, 1000);
    });

    if (midiIn && typeof midiIn.on === 'function') {
      midiIn.on('error', (err) => {
        pushLog('error', `MIDI in error: ${err.message}`);
      });
    }

    if (midiIn && typeof midiIn.connect === 'function') {
      midiIn.connect((msg) => {
        if (!msg || typeof msg.toArray !== 'function') return;
        const arr = msg.toArray();
        if ((arr[0] & 0xf0) !== 0xb0) return;
        const cmd = validateCmd({
          cmd: 'SET_SLOT_VALUE',
          slot: arr[1],
          value: arr[2],
        });
        if (!cmd) {
          pushLog('warn', 'dropping bad MIDI CC', arr);
          return;
        }
        sendLine(formatLiveValueCommand(cmd));
      });
    }
  }

  async function start() {
    if (running) return getState();
    await loadDeps();
    stopping = false;
    manualStop = false;
    running = true;
    setState({
      running: true,
      ready: false,
      lastError: null,
      config: clone(config),
    });
    attachOsc();
    attachMidi();
    attachSerial();
    return getState();
  }

  async function stop() {
    manualStop = true;
    stopping = true;
    running = false;
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    if (parser && typeof parser.removeAllListeners === 'function') {
      parser.removeAllListeners();
    }
    parser = null;

    if (serial && typeof serial.removeAllListeners === 'function') {
      serial.removeAllListeners();
    }
    if (serial && typeof serial.close === 'function') {
      try {
        await new Promise((resolve) => {
          const maybe = serial.close((err) => {
            if (err) pushLog('error', `serial close failed: ${err.message}`);
            resolve();
          });
          if (maybe && typeof maybe.then === 'function') {
            maybe.then(resolve).catch(() => resolve());
          } else if (serial.close.length === 0) {
            resolve();
          }
        });
      } catch (_) {
        // no-op
      }
    }
    serial = null;

    if (udp && typeof udp.close === 'function') {
      try {
        udp.close();
      } catch (_) {
        // no-op
      }
    }
    udp = null;

    if (midiIn && typeof midiIn.close === 'function') {
      try {
        midiIn.close();
      } catch (_) {
        // no-op
      }
    }
    midiIn = null;

    if (midiOut && typeof midiOut.close === 'function') {
      try {
        midiOut.close();
      } catch (_) {
        // no-op
      }
    }
    midiOut = null;
    midi = null;

    setState({
      running: false,
      serialConnected: false,
      ready: false,
      config: clone(config),
    });
    stopping = false;
    return getState();
  }

  async function configure(nextConfig = {}, { restart = true } = {}) {
    Object.assign(config, nextConfig || {});
    setState({ config: clone(config) });
    if (restart && running) {
      await stop();
      await start();
    }
    return getState();
  }

  function sendLine(line) {
    const normalized = `${String(line).trim()}\n`;
    if (!serial || typeof serial.write !== 'function') {
      const error = new Error('serial not connected');
      setState({ lastError: error.message });
      throw error;
    }
    serial.write(normalized);
    return normalized;
  }

  function on(eventName, handler) {
    events.on(eventName, handler);
    return () => events.off(eventName, handler);
  }

  function getState() {
    return {
      running: state.running,
      serialConnected: state.serialConnected,
      ready: state.ready,
      manifest: clone(state.manifest),
      lastError: state.lastError,
      lastTelemetryAt: state.lastTelemetryAt,
      logs: clone(state.logs),
      config: clone(config),
    };
  }

  return {
    start,
    stop,
    configure,
    sendLine,
    getState,
    on,
    listSerialPorts,
  };
}

module.exports = {
  SERIAL_BAUD,
  DEFAULT_OSC_PORT,
  DEFAULT_HTTP_PORT,
  MAX_MSG_LEN,
  usageText,
  getArg,
  parseConfigFromArgv,
  createBridgeService,
  validateCmd,
  formatLiveValueCommand,
};
