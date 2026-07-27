const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {
  usageText,
  getArg,
  parsePositiveInt,
  parseConfigFromArgv: parseCliConfigFromArgv,
} = require('./config/cli_config');
const { MN42_MANIFEST_CONTRACT } = require('./manifest_contract');
const { normalizeMidiToOscMappings } = require('./config/midi_osc_mappings');

function copyDirectoryRecursive(srcDir, dstDir) {
  fs.mkdirSync(dstDir, { recursive: true });
  const entries = fs.readdirSync(srcDir, { withFileTypes: true });
  for (const entry of entries) {
    const srcPath = path.join(srcDir, entry.name);
    const dstPath = path.join(dstDir, entry.name);
    if (entry.isDirectory()) {
      copyDirectoryRecursive(srcPath, dstPath);
      continue;
    }
    if (entry.isFile()) {
      fs.copyFileSync(srcPath, dstPath);
    }
  }
}

function preparePackagedSerialportBindings(pushLog) {
  if (!process.pkg) return;

  const snapshotBindingsRoot = path.resolve(
    __dirname,
    '..',
    'node_modules',
    '@serialport',
    'bindings-cpp',
  );
  const snapshotPrebuilds = path.join(snapshotBindingsRoot, 'prebuilds');
  if (!fs.existsSync(snapshotPrebuilds)) {
    pushLog(
      'warn',
      `serialport prebuilds were not packaged (${snapshotPrebuilds})`,
    );
    return;
  }

  const stagedRoot = path.join(
    os.tmpdir(),
    'mn42-bridge-native',
    'serialport-bindings-cpp',
  );
  const stagedPrebuilds = path.join(stagedRoot, 'prebuilds');
  // Temp directories survive application upgrades; refresh native bindings so
  // the current executable never loads an old ABI from a fixed staging path.
  fs.rmSync(stagedPrebuilds, { recursive: true, force: true });
  copyDirectoryRecursive(snapshotPrebuilds, stagedPrebuilds);

  // node-gyp-build resolves prebuild paths via "<PACKAGE_NAME>_PREBUILD".
  process.env['@SERIALPORT/BINDINGS_CPP_PREBUILD'] = stagedRoot;
  process.env.SERIALPORT_BINDINGS_CPP_PREBUILD = stagedRoot;
}

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
// Full GET_CONFIG payloads can exceed 16 kB once 42 slots, EF metadata, and LED
// state are serialized, so keep the debug guard comfortably above normal config frames.
const MAX_SERIAL_LINE_LEN = 128 * 1024;
const MAX_MSG_LEN = MAX_CMD_LEN;
const OSC_CMD_ADDRESS = '/mn42/cmd';
const OSC_EVENT_PREFIX = '/mn42/event/';
const {
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
const {
  createSerialTransportLifecycle,
} = require('./transports/serial_transport');
const { createSerialLineHandler } = require('./transports/serial_ingress');
const { createOscTransport } = require('./transports/osc_transport');
const { createMidiTransportLifecycle } = require('./transports/midi_transport');
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
const {
  normalizeBridgeRuntimeConfig,
  startBridgeRuntime,
  stopBridgeRuntime,
  configureBridgeRuntime,
} = require('./runtime/lifecycle');
const { createBridgeStateStore } = require('./runtime/state_store');
const { createDeviceSession } = require('./device/session');
const { createStructuredEvent } = require('./device/transport_contract');

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
    configPath: null,
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
    midiToOscMappings: [],
    ...initialConfig,
  };
  config.midiToOscMappings = normalizeMidiToOscMappings(
    config.midiToOscMappings,
  );

  let depsLoaded = false;
  let serialApi = injected.serialport || null;
  let oscApi = injected.osc || null;
  let jzzFactory = injected.jzz || null;

  let serial = null;
  let parser = null;
  let udp = null;
  let midiOut = null;
  let midiIn = null;
  let reconnectTimer = null;
  let running = false;
  let stopping = false;
  let manualStop = false;
  let traceSeq = 0;
  let deviceSeq = 0;
  const midiRpnStateByChannel = new Map();

  const state = {
    running: false,
    serialConnected: false,
    ready: false,
    manifest: null,
    deviceSession: null,
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

  const { emitState, pushLog, setState, bumpCounter, getState } =
    createBridgeStateStore({
      events,
      state,
      clone,
      getConfig: () => config,
      logLimit: LOG_LIMIT,
    });

  function emitStructuredEvent(eventName, payload) {
    events.emit('structured-event', createStructuredEvent(eventName, payload));
  }

  const deviceSession = createDeviceSession({
    sendLine: (line) => sendLine(line),
    pushLog,
    nextSeq: () => {
      deviceSeq += 1;
      return deviceSeq;
    },
    onStateChange: (nextSessionState) => {
      setState({
        ready: Boolean(nextSessionState?.ready),
        manifest: clone(nextSessionState?.manifest),
        deviceSession: clone(nextSessionState),
        lastError:
          nextSessionState?.lastError === undefined
            ? state.lastError
            : nextSessionState.lastError,
      });
    },
    onStructuredEvent: (event) => {
      events.emit('structured-event', event);
    },
  });

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
      emitStructuredEvent('bridge.performance', {
        performance: nextPerformance,
      });
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

  // Mirror manifest packets into the legacy top-level bridge snapshot for back-compat consumers.
  function inspectManifest(msg) {
    if (!msg || typeof msg !== 'object') return;
    const directManifest =
      typeof msg.device_name === 'string' &&
      Number.isFinite(Number(msg.slot_count)) &&
      Number.isFinite(Number(msg.pot_count))
        ? msg
        : null;
    const manifest =
      (msg.result && typeof msg.result === 'object' && msg.result.manifest) ||
      msg.manifest ||
      directManifest ||
      null;
    if (manifest && typeof manifest === 'object') {
      setState({ manifest: manifest });
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
    getMidiToOscMappings: () => config.midiToOscMappings,
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

  function sendMappedMidiEventToOsc(event, routeMeta = {}) {
    return bridgeEgress.sendMappedMidiEventToOsc(event, routeMeta);
  }

  const handleSerialLine = createSerialLineHandler({
    maxSerialLineLen: MAX_SERIAL_LINE_LEN,
    setState,
    broadcastLine,
    bumpCounter,
    pushLog,
    inspectManifest,
    deviceSession,
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
    if (!serialApi) {
      try {
        preparePackagedSerialportBindings(pushLog);
      } catch (err) {
        pushLog(
          'warn',
          `serial native prebuild staging failed: ${err.message}`,
        );
      }
      serialApi = require('serialport');
    }
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

  async function listMidiPorts() {
    await loadDeps();
    if (typeof jzzFactory !== 'function') {
      return { inputs: [], outputs: [] };
    }
    return new Promise((resolve) => {
      try {
        const midi = jzzFactory();
        midi
          .and(function onMidiReady() {
            const info =
              typeof this.info === 'function'
                ? this.info()
                : { inputs: [], outputs: [] };
            resolve({
              inputs: Array.isArray(info.inputs) ? clone(info.inputs) : [],
              outputs: Array.isArray(info.outputs) ? clone(info.outputs) : [],
            });
          })
          .or(function onMidiListError() {
            const message =
              typeof this?._err === 'function'
                ? this._err()
                : 'unknown MIDI engine error';
            pushLog('error', `MIDI port list failed: ${message}`);
            resolve({ inputs: [], outputs: [] });
          });
      } catch (err) {
        pushLog('error', `MIDI port list failed: ${err.message}`);
        resolve({ inputs: [], outputs: [] });
      }
    });
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
      pushLog(
        'info',
        `serial up on ${serial?.path || config.serialName} @${SERIAL_BAUD}`,
      );
      Promise.resolve(deviceSession.handleOpen()).catch((err) => {
        pushLog('error', `device session handshake failed: ${err.message}`);
      });
    },
    onSerialError: (err) => {
      raiseAlert({
        code: 'serial_disconnected',
        severity: 'error',
        message: `Serial transport error: ${err.message}`,
        details: { serialName: serial?.path || config.serialName },
        hostTimestampMs: Date.now(),
      });
      setState({
        serialConnected: false,
        ready: false,
        lastError: err.message,
      });
      deviceSession.handleDisconnect(err.message);
      pushLog('error', `serial error: ${err.message}`);
      scheduleReconnect();
    },
    onSerialClose: () => {
      raiseAlert({
        code: 'serial_disconnected',
        severity: 'warn',
        message: 'Serial transport disconnected',
        details: { serialName: serial?.path || config.serialName },
        hostTimestampMs: Date.now(),
      });
      setState({
        serialConnected: false,
        ready: false,
      });
      deviceSession.handleDisconnect('serial_close');
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
        sendMappedMidiEventToOsc,
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
    let startedState;
    try {
      startedState = await startBridgeRuntime({
        isRunning: () => running,
        getState,
        loadDeps,
        setRuntimeFlags: ({ running: r, stopping: s, manualStop: m }) => {
        if (typeof r === 'boolean') running = r;
        if (typeof s === 'boolean') stopping = s;
        if (typeof m === 'boolean') manualStop = m;
        },
      resetTraceSeq: () => {
        traceSeq = 0;
      },
      clearFeedbackGuard: () => feedbackGuard.clear(),
      clearMidiRpnState: () => midiRpnStateByChannel.clear(),
      resetRouteAlerts: () => routeAlertPolicy.resetForRun(),
      clearPerformanceTracking,
      setState,
      createTimingState,
      createPerformanceState,
      createAlertState,
      config,
      clone,
      attachOsc,
      attachMidi,
        attachSerial,
      });
    } catch (error) {
      // Attach order is necessarily incremental; compensate through the same
      // teardown path used by an operator stop so no OSC/MIDI/serial fragment
      // survives a failed startup.
      await stop().catch(() => {});
      throw error;
    }
    emitStructuredEvent('bridge.performance', {
      performance: getState().performance,
    });
    return startedState;
  }

  // Tear down serial, OSC, and MIDI cleanly so the bridge can restart without zombie listeners.
  async function stop() {
    // Cancel handshake/apply timers before closing transports; a stopped bridge
    // must never retry HELLO into a serial lane that has already been released.
    deviceSession.handleDisconnect('service_stop');
    return stopBridgeRuntime({
      setRuntimeFlags: ({ running: r, stopping: s, manualStop: m }) => {
        if (typeof r === 'boolean') running = r;
        if (typeof s === 'boolean') stopping = s;
        if (typeof m === 'boolean') manualStop = m;
      },
      cancelSerialReconnect: () => serialLifecycle?.cancelReconnect(),
      cancelMidiRetry: () => midiLifecycle?.cancelRetry(),
      getParser: () => parser,
      setParser: (next) => {
        parser = next;
      },
      getSerial: () => serial,
      setSerial: (next) => {
        serial = next;
      },
      getUdp: () => udp,
      setUdp: (next) => {
        udp = next;
      },
      getMidiIn: () => midiIn,
      setMidiIn: (next) => {
        midiIn = next;
      },
      getMidiOut: () => midiOut,
      setMidiOut: (next) => {
        midiOut = next;
      },
      detachListeners,
      closeSerialDevice,
      closeQuietly,
      pushLog,
      setState,
      config,
      clone,
      getState,
    });
  }

  // Update bridge config and optionally restart live transports to apply it.
  async function configure(nextConfig = {}, { restart = true } = {}) {
    await configureBridgeRuntime(
      nextConfig,
      { restart },
      {
        config,
        normalizeConfig: (mutableConfig) => {
          normalizeBridgeRuntimeConfig(mutableConfig, {
            parsePositiveInt,
            defaultFeedbackWindowMs: DEFAULT_FEEDBACK_WINDOW_MS,
            defaultRtP95TargetMs: DEFAULT_RT_P95_TARGET_MS,
            defaultRtJitterP95TargetMs: DEFAULT_RT_JITTER_P95_TARGET_MS,
            defaultAlertSuppressionMs: DEFAULT_ALERT_SUPPRESSION_MS,
          });
          mutableConfig.midiToOscMappings = normalizeMidiToOscMappings(
            mutableConfig.midiToOscMappings,
          );
        },
        setState,
        clone,
        refreshPerformance,
        isRunning: () => running,
        stop,
        start,
        clearFeedbackGuard: () => feedbackGuard.clear(),
        now: () => Date.now(),
      },
    );
    return getState();
  }

  async function stageDeviceConfig(configPayload) {
    return deviceSession.stageConfig(configPayload);
  }

  async function applyDeviceConfig(options = {}) {
    return deviceSession.applyStagedConfig(options);
  }

  async function rollbackDeviceConfig(reason = 'operator_request') {
    return deviceSession.rollback(reason);
  }

  async function prewarmDeviceSession() {
    await deviceSession.ensureAuthority();
    return deviceSession.getState();
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

  return {
    applyDeviceConfig,
    start,
    stageDeviceConfig,
    stop,
    configure,
    prewarmDeviceSession,
    rollbackDeviceConfig,
    resetPerformance,
    clearAlerts,
    sendLine,
    getState,
    getDeviceSessionState: () => deviceSession.getState(),
    on,
    listSerialPorts,
    listMidiPorts,
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
  MN42_MANIFEST_CONTRACT,
  usageText,
  getArg,
  parseConfigFromArgv,
  createBridgeService,
  validateCmd,
  formatLiveValueCommand,
};
