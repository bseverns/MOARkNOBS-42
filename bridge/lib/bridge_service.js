const { EventEmitter } = require('node:events');
const {
  usageText,
  getArg,
  parsePositiveInt,
  parseConfigFromArgv: parseCliConfigFromArgv,
} = require('./config/cli_config');

const SERIAL_BAUD = 115200;
const DEFAULT_OSC_PORT = 9000;
const DEFAULT_HTTP_PORT = 8787;
const DEFAULT_FEEDBACK_WINDOW_MS = 120;
const DEFAULT_RT_P95_TARGET_MS = 10;
const DEFAULT_RT_JITTER_P95_TARGET_MS = 5;
const DEFAULT_ALERT_SUPPRESSION_MS = 3000;
const LOG_LIMIT = 200;
const ROUTE_LOG_LIMIT = 200;
const ALERT_LOG_LIMIT = 200;
const ROUND_TRIP_WINDOW = 200;
const PENDING_COMMAND_TTL_MS = 5000;
const MAX_CMD_LEN = 128;
const MAX_SERIAL_LINE_LEN = 16 * 1024;
const MAX_MSG_LEN = MAX_CMD_LEN;
const OSC_CMD_ADDRESS = '/mn42/cmd';
const OSC_EVENT_PREFIX = '/mn42/event/';
const {
  normalizeTypedEvent,
  parseMidiMessageToTypedEvents,
  normalizeOscTypedEventMessage,
  buildSlotCommandFromCcEvent,
} = require('./codec/typed_events');
const {
  validateSlotValueCommand: validateLiveValueCommand,
  formatLiveValueCommand,
} = require('./codec/live_value_command');
const { createOscMessageHandler } = require('./transports/osc_ingress');
const { createMidiMessageHandler } = require('./transports/midi_ingress');
const { createSerialTransportLifecycle } = require('./transports/serial_transport');
const { createSerialLineHandler } = require('./transports/serial_ingress');
const { createOscTransport } = require('./transports/osc_transport');
const {
  createMidiTransportLifecycle,
} = require('./transports/midi_transport');
const {
  detachListeners,
  closeSerialDevice,
  closeQuietly,
} = require('./transports/teardown');
const { createBridgeEgress } = require('./transports/egress');
const {
  createTimingState,
  createCounterState,
  createPerformanceState,
  createAlertState,
  normalizeTimestampMs,
  sanitizeTraceId,
  extractTimestampMs,
  extractTraceId,
} = require('./observability/performance');
const { createRouteAlertPolicy } = require('./observability/route_alerts');
const {
  createPerformanceTracker,
} = require('./observability/performance_tracker');
const { createFeedbackGuard } = require('./transports/feedback_guard');

// Translate CLI flags into the bridge's runtime config object.
function parseConfigFromArgv(argv = process.argv) {
  return parseCliConfigFromArgv(argv, {
    defaultOscPort: DEFAULT_OSC_PORT,
    defaultFeedbackWindowMs: DEFAULT_FEEDBACK_WINDOW_MS,
    defaultRtP95TargetMs: DEFAULT_RT_P95_TARGET_MS,
    defaultRtJitterP95TargetMs: DEFAULT_RT_JITTER_P95_TARGET_MS,
    defaultAlertSuppressionMs: DEFAULT_ALERT_SUPPRESSION_MS,
    defaultHttpPort: DEFAULT_HTTP_PORT,
  });
}

// Back-compat alias kept for tests and legacy imports.
function validateCmd(m) {
  return validateLiveValueCommand(m, MAX_CMD_LEN);
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
    allowFeedbackLoops: false,
    feedbackWindowMs: DEFAULT_FEEDBACK_WINDOW_MS,
    rtP95TargetMs: DEFAULT_RT_P95_TARGET_MS,
    rtJitterP95TargetMs: DEFAULT_RT_JITTER_P95_TARGET_MS,
    alertSuppressionMs: DEFAULT_ALERT_SUPPRESSION_MS,
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
  let traceSeq = 0;
  const midiRpnStateByChannel = new Map();

  const state = {
    running: false,
    serialConnected: false,
    ready: false,
    manifest: null,
    lastError: null,
    lastTelemetryAt: null,
    lastRouteAt: null,
    lastRouteTraceId: null,
    logs: [],
    routes: [],
    timing: createTimingState(),
    performance: createPerformanceState({
      rtP95TargetMs: config.rtP95TargetMs,
      rtJitterP95TargetMs: config.rtJitterP95TargetMs,
    }),
    alerts: createAlertState(),
    counters: createCounterState(),
    config: clone(config),
  };

  // Push a fresh state snapshot to UI/CLI listeners after every meaningful change.
  function emitState() {
    events.emit('state', getState());
  }

  // Keep a bounded in-memory log so the browser console can show recent events without growing forever.
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

  // Merge partial state updates and notify subscribers immediately.
  function setState(partial) {
    Object.assign(state, partial);
    emitState();
  }

  // Keep drop/parse counters in state for browser/CLI diagnostics.
  function bumpCounter(name, amount = 1) {
    if (!Object.prototype.hasOwnProperty.call(state.counters, name)) return;
    state.counters[name] += amount;
    emitState();
  }

  function nextTraceId(prefix = 'trace') {
    traceSeq += 1;
    return `${prefix}-${Date.now().toString(36)}-${traceSeq.toString(36)}`;
  }

  const routeAlertPolicy = createRouteAlertPolicy({
    state,
    emitState,
    emitEvent: (eventName, payload) => events.emit(eventName, payload),
    cloneValue: clone,
    normalizeTimestampMs,
    sanitizeTraceId,
    parsePositiveInt,
    getAlertSuppressionMs: () => config.alertSuppressionMs,
    defaultAlertSuppressionMs: DEFAULT_ALERT_SUPPRESSION_MS,
    routeLogLimit: ROUTE_LOG_LIMIT,
    alertLogLimit: ALERT_LOG_LIMIT,
    now: () => Date.now(),
  });

  function recordRoute(partial = {}) {
    return routeAlertPolicy.recordRoute(partial);
  }

  function raiseAlert(options = {}) {
    return routeAlertPolicy.raiseAlert(options);
  }

  function resolveAlert(code, options = {}) {
    return routeAlertPolicy.resolveAlert(code, options);
  }

  function clearAlerts(options = {}) {
    routeAlertPolicy.clearAlerts(options);
    return getState();
  }

  const feedbackGuard = createFeedbackGuard({
    getAllowFeedbackLoops: () => config.allowFeedbackLoops,
    getFeedbackWindowMs: () => config.feedbackWindowMs,
    now: () => Date.now(),
  });

  const performanceTracker = createPerformanceTracker({
    setPerformanceState: (nextPerformance) => {
      setState({ performance: nextPerformance });
    },
    getPerformanceState: () => state.performance,
    getTargets: () => ({
      p95Ms: config.rtP95TargetMs,
      jitterP95Ms: config.rtJitterP95TargetMs,
    }),
    raiseAlert,
    resolveAlert,
    recordRoute,
    now: () => Date.now(),
    roundTripWindow: ROUND_TRIP_WINDOW,
    pendingCommandTtlMs: PENDING_COMMAND_TTL_MS,
  });

  function refreshPerformance(
    hostTimestampMs = Date.now(),
    bumpUpdated = false,
  ) {
    performanceTracker.refresh(hostTimestampMs, bumpUpdated);
  }

  function queuePendingCommand({
    slot,
    value,
    traceId,
    hostTimestampMs = Date.now(),
    source = 'unknown',
  }) {
    return performanceTracker.queuePendingCommand({
      slot,
      value,
      traceId,
      hostTimestampMs,
      source,
    });
  }

  function matchPendingRoundTrips({
    slots,
    telemetryTraceId,
    hostTimestampMs,
    sourceTimestampMs,
  }) {
    performanceTracker.matchPendingRoundTrips({
      slots,
      telemetryTraceId,
      hostTimestampMs,
      sourceTimestampMs,
    });
  }

  function clearPerformanceTracking() {
    performanceTracker.clear();
  }

  function resetPerformance() {
    clearPerformanceTracking();
    refreshPerformance(Date.now(), true);
    recordRoute({
      flow: 'metrics',
      kind: 'reset',
      hostTimestampMs: Date.now(),
      reason: 'operator_reset',
    });
    return getState();
  }

  function markTelemetryMidi(status, cc, value) {
    feedbackGuard.markTelemetryMidi(status, cc, value);
  }

  function shouldSuppressMidiEcho(status, cc, value) {
    return feedbackGuard.shouldSuppressMidiEcho(status, cc, value);
  }

  // Watch firmware hello/manifest traffic and mark the bridge ready once identity is known.
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

  // Fan raw serial lines out to any UI/diagnostic listeners.
  function broadcastLine(line) {
    events.emit('line', line);
  }

  const bridgeEgress = createBridgeEgress({
    getUdp: () => udp,
    getMidiOut: () => midiOut,
    getOscTarget: () => ({ host: config.oscHost, port: config.oscPort }),
    markTelemetryMidi,
    recordRoute,
    pushLog,
  });

  function sendOscTelemetry(address, args, routeMeta = {}) {
    bridgeEgress.sendOscTelemetry(address, args, routeMeta);
  }

  function sendMidiTelemetry(channelBase, values, routeMeta = {}) {
    bridgeEgress.sendMidiTelemetry(channelBase, values, routeMeta);
  }

  function sendTypedEventToOsc(event, routeMeta = {}) {
    return bridgeEgress.sendTypedEventToOsc(event, routeMeta);
  }

  function sendTypedEventToMidi(event, routeMeta = {}) {
    return bridgeEgress.sendTypedEventToMidi(event, routeMeta);
  }

  const handleSerialLine = createSerialLineHandler({
    maxSerialLineLen: MAX_SERIAL_LINE_LEN,
    setState,
    broadcastLine,
    bumpCounter,
    pushLog,
    inspectManifest,
    extractTimestampMs,
    extractTraceId,
    nextTraceId,
    matchPendingRoundTrips,
    sendOscTelemetry,
    sendMidiTelemetry,
    now: () => Date.now(),
  });

  // Lazy-load runtime dependencies so tests can inject fakes without touching require().
  async function loadDeps() {
    if (depsLoaded) return;
    if (!serialApi) serialApi = require('serialport');
    if (!oscApi) oscApi = require('osc');
    if (!jzzFactory) jzzFactory = require('jzz');
    depsLoaded = true;
  }

  // Surface serial choices for the browser console before the bridge starts.
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

  let serialLifecycle = null;
  let oscTransport = null;
  let midiLifecycle = null;

  // Reopen the serial side after transient disconnects unless the operator explicitly stopped the service.
  function scheduleReconnect() {
    serialLifecycle?.scheduleReconnect();
  }

  // Wire the serial device and newline parser into the bridge event loop.
  function attachSerial() {
    serialLifecycle?.attachSerial();
  }

  serialLifecycle = createSerialTransportLifecycle({
    getSerialApi: () => serialApi,
    config,
    serialBaud: SERIAL_BAUD,
    getRuntimeState: () => ({ running, stopping, manualStop }),
    getSerial: () => serial,
    setSerial: (next) => {
      serial = next;
    },
    setParser: (next) => {
      parser = next;
    },
    getReconnectTimer: () => reconnectTimer,
    setReconnectTimer: (next) => {
      reconnectTimer = next;
    },
    pushLog,
    onSerialOpen: () => {
      resolveAlert('serial_disconnected', {
        reason: 'serial_open',
        hostTimestampMs: Date.now(),
      });
      setState({ serialConnected: true, lastError: null });
      pushLog('info', `serial up on ${config.serialName} @${SERIAL_BAUD}`);
      try {
        serial.write('HELLO\n');
      } catch (err) {
        pushLog('error', `HELLO write failed: ${err.message}`);
      }
    },
    onSerialError: (err) => {
      raiseAlert({
        code: 'serial_disconnected',
        severity: 'error',
        message: `Serial transport error: ${err.message}`,
        details: { serialName: config.serialName },
        hostTimestampMs: Date.now(),
      });
      setState({
        serialConnected: false,
        ready: false,
        lastError: err.message,
      });
      pushLog('error', `serial error: ${err.message}`);
      scheduleReconnect();
    },
    onSerialClose: () => {
      raiseAlert({
        code: 'serial_disconnected',
        severity: 'warn',
        message: 'Serial transport disconnected',
        details: { serialName: config.serialName },
        hostTimestampMs: Date.now(),
      });
      setState({
        serialConnected: false,
        ready: false,
      });
      if (manualStop || stopping) return;
      pushLog('error', 'serial disconnected');
      scheduleReconnect();
    },
    onSerialLine: handleSerialLine,
  });

  oscTransport = createOscTransport({
    getOscApi: () => oscApi,
    getConfig: () => config,
    getRuntimeState: () => ({ running, stopping, manualStop }),
    setUdp: (next) => {
      udp = next;
    },
    pushLog,
    createMessageHandler: () =>
      createOscMessageHandler({
        oscCmdAddress: OSC_CMD_ADDRESS,
        oscEventPrefix: OSC_EVENT_PREFIX,
        maxCmdLen: MAX_CMD_LEN,
        validateCmd,
        formatLiveValueCommand,
        normalizeOscTypedEventMessage,
        buildSlotCommandFromCcEvent,
        sendTypedEventToMidi,
        queuePendingCommand,
        sendLine,
        nextTraceId,
        extractTraceId,
        extractTimestampMs,
        bumpCounter,
        recordRoute,
        pushLog,
        now: () => Date.now(),
      }),
  });

  midiLifecycle = createMidiTransportLifecycle({
    getJzzFactory: () => jzzFactory,
    getConfig: () => config,
    getRuntimeState: () => ({ running, stopping, manualStop }),
    setMidi: (next) => {
      midi = next;
    },
    setMidiIn: (next) => {
      midiIn = next;
    },
    setMidiOut: (next) => {
      midiOut = next;
    },
    pushLog,
    createMessageHandler: () =>
      createMidiMessageHandler({
        nextTraceId,
        now: () => Date.now(),
        shouldSuppressMidiEcho,
        bumpCounter,
        recordRoute,
        parseMidiMessageToTypedEvents,
        midiRpnStateByChannel,
        sendTypedEventToOsc,
        buildSlotCommandFromCcEvent,
        pushLog,
        queuePendingCommand,
        sendLine,
        formatLiveValueCommand,
      }),
  });

  function attachOsc() {
    oscTransport?.attachOsc();
  }

  function attachMidi() {
    midiLifecycle?.attachMidi();
  }

  // Start all bridge transports and publish a fresh runtime snapshot.
  async function start() {
    if (running) return getState();
    await loadDeps();
    stopping = false;
    manualStop = false;
    running = true;
    traceSeq = 0;
    feedbackGuard.clear();
    midiRpnStateByChannel.clear();
    routeAlertPolicy.resetForRun();
    clearPerformanceTracking();
    setState({
      running: true,
      ready: false,
      lastError: null,
      lastRouteAt: null,
      lastRouteTraceId: null,
      routes: [],
      timing: createTimingState(),
      performance: createPerformanceState({
        rtP95TargetMs: config.rtP95TargetMs,
        rtJitterP95TargetMs: config.rtJitterP95TargetMs,
      }),
      alerts: createAlertState(),
      config: clone(config),
    });
    attachOsc();
    attachMidi();
    attachSerial();
    return getState();
  }

  // Tear down serial, OSC, and MIDI cleanly so the bridge can restart without zombie listeners.
  async function stop() {
    manualStop = true;
    stopping = true;
    running = false;
    serialLifecycle?.cancelReconnect();
    midiLifecycle?.cancelRetry();

    detachListeners(parser);
    parser = null;

    detachListeners(serial);
    await closeSerialDevice(serial, {
      onCloseError: (err) => {
        pushLog('error', `serial close failed: ${err.message}`);
      },
    });
    serial = null;

    closeQuietly(udp);
    udp = null;

    closeQuietly(midiIn);
    midiIn = null;

    closeQuietly(midiOut);
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

  // Update bridge config and optionally restart live transports to apply it.
  async function configure(nextConfig = {}, { restart = true } = {}) {
    Object.assign(config, nextConfig || {});
    const normalizedWindow = parsePositiveInt(
      config.feedbackWindowMs,
      DEFAULT_FEEDBACK_WINDOW_MS,
    );
    config.feedbackWindowMs = normalizedWindow;
    config.rtP95TargetMs = parsePositiveInt(
      config.rtP95TargetMs,
      DEFAULT_RT_P95_TARGET_MS,
    );
    config.rtJitterP95TargetMs = parsePositiveInt(
      config.rtJitterP95TargetMs,
      DEFAULT_RT_JITTER_P95_TARGET_MS,
    );
    config.alertSuppressionMs = parsePositiveInt(
      config.alertSuppressionMs,
      DEFAULT_ALERT_SUPPRESSION_MS,
    );
    if (config.allowFeedbackLoops) {
      feedbackGuard.clear();
    }
    setState({ config: clone(config) });
    refreshPerformance(Date.now(), false);
    if (restart && running) {
      await stop();
      await start();
    }
    return getState();
  }

  // Send one native line to firmware, surfacing an immediate error if serial is down.
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

  // Subscribe to bridge state/log/line events with an unsubscribe helper.
  function on(eventName, handler) {
    events.on(eventName, handler);
    return () => events.off(eventName, handler);
  }

  // Return a serializable snapshot for the browser console and CLI status output.
  function getState() {
    return {
      running: state.running,
      serialConnected: state.serialConnected,
      ready: state.ready,
      manifest: clone(state.manifest),
      lastError: state.lastError,
      lastTelemetryAt: state.lastTelemetryAt,
      lastRouteAt: state.lastRouteAt,
      lastRouteTraceId: state.lastRouteTraceId,
      timing: clone(state.timing),
      performance: clone(state.performance),
      alerts: clone(state.alerts),
      logs: clone(state.logs),
      routes: clone(state.routes),
      counters: clone(state.counters),
      config: clone(config),
    };
  }

  return {
    start,
    stop,
    configure,
    resetPerformance,
    clearAlerts,
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
  DEFAULT_FEEDBACK_WINDOW_MS,
  DEFAULT_RT_P95_TARGET_MS,
  DEFAULT_RT_JITTER_P95_TARGET_MS,
  DEFAULT_ALERT_SUPPRESSION_MS,
  ROUTE_LOG_LIMIT,
  ROUND_TRIP_WINDOW,
  PENDING_COMMAND_TTL_MS,
  MAX_CMD_LEN,
  MAX_SERIAL_LINE_LEN,
  MAX_MSG_LEN,
  usageText,
  getArg,
  parseConfigFromArgv,
  createBridgeService,
  validateCmd,
  formatLiveValueCommand,
};
