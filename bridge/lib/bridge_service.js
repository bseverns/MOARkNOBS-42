const { EventEmitter } = require('node:events');

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

// Shared CLI/server help text so both bridge entrypoints describe the same contract.
function usageText() {
  return (
    'mn42_bridge.js - link MOARkNOBS-42 to OSC & MIDI\n' +
    'Usage: node mn42_bridge.js [--serial PORT] [--osc PORT] [--osc-listen PORT] [--host ADDR] [--bind ADDR] [--midi LABEL] [--allow-feedback-loops] [--feedback-window-ms N] [--rt-p95-target-ms N] [--rt-jitter-p95-target-ms N] [--alert-suppression-ms N]'
  );
}

// Minimal argv reader; the bridge keeps parsing intentionally lightweight.
function getArg(argv, flag, def) {
  const idx = argv.indexOf(flag);
  if (idx >= 0 && idx + 1 < argv.length) return argv[idx + 1];
  return def;
}

// Parse port-like CLI values and fall back cleanly when they are absent or invalid.
function parsePositiveInt(raw, fallback) {
  const parsed = parseInt(String(raw), 10);
  if (!Number.isInteger(parsed) || parsed <= 0) return fallback;
  return parsed;
}

// Translate CLI flags into the bridge's runtime config object.
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
  const feedbackWindowMs = parsePositiveInt(
    getArg(argv, '--feedback-window-ms', String(DEFAULT_FEEDBACK_WINDOW_MS)),
    null,
  );
  if (feedbackWindowMs === null) {
    const error = new Error('bad --feedback-window-ms');
    error.showUsage = true;
    throw error;
  }
  const rtP95TargetMs = parsePositiveInt(
    getArg(argv, '--rt-p95-target-ms', String(DEFAULT_RT_P95_TARGET_MS)),
    null,
  );
  if (rtP95TargetMs === null) {
    const error = new Error('bad --rt-p95-target-ms');
    error.showUsage = true;
    throw error;
  }
  const rtJitterP95TargetMs = parsePositiveInt(
    getArg(
      argv,
      '--rt-jitter-p95-target-ms',
      String(DEFAULT_RT_JITTER_P95_TARGET_MS),
    ),
    null,
  );
  if (rtJitterP95TargetMs === null) {
    const error = new Error('bad --rt-jitter-p95-target-ms');
    error.showUsage = true;
    throw error;
  }
  const alertSuppressionMs = parsePositiveInt(
    getArg(
      argv,
      '--alert-suppression-ms',
      String(DEFAULT_ALERT_SUPPRESSION_MS),
    ),
    null,
  );
  if (alertSuppressionMs === null) {
    const error = new Error('bad --alert-suppression-ms');
    error.showUsage = true;
    throw error;
  }

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
    allowFeedbackLoops: argv.includes('--allow-feedback-loops'),
    feedbackWindowMs,
    rtP95TargetMs,
    rtJitterP95TargetMs,
    alertSuppressionMs,
  };
}

// Reject malformed inbound host-control messages before they reach serial.
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
  if (JSON.stringify(cmd).length > MAX_CMD_LEN) return null;
  return cmd;
}

// Native firmware live-control verb used by OSC and MIDI input paths.
function formatLiveValueCommand(cmd) {
  return `SET_SLOT_VALUE,${cmd.slot},${cmd.value}`;
}

// Plain JSON clone is enough for bridge state snapshots and log payloads.
function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function createTimingState() {
  return {
    lastSerialSourceTimestampMs: null,
    lastSerialHostTimestampMs: null,
    lastSerialSkewMs: null,
  };
}

function createCounterState() {
  return {
    serialParseErrors: 0,
    serialOversizeDrops: 0,
    badOscCmdDrops: 0,
    badMidiCmdDrops: 0,
    feedbackSuppressed: 0,
  };
}

function createPerformanceState({
  rtP95TargetMs = DEFAULT_RT_P95_TARGET_MS,
  rtJitterP95TargetMs = DEFAULT_RT_JITTER_P95_TARGET_MS,
} = {}) {
  return {
    roundTrip: {
      windowSize: ROUND_TRIP_WINDOW,
      pending: 0,
      sampleCount: 0,
      minMs: null,
      maxMs: null,
      meanMs: null,
      p50Ms: null,
      p95Ms: null,
      lastMs: null,
      jitterMeanMs: null,
      jitterP95Ms: null,
      lastUpdatedAt: null,
    },
    health: {
      status: 'no_data',
      reasons: ['No round-trip samples yet'],
      thresholds: {
        p95Ms: rtP95TargetMs,
        jitterP95Ms: rtJitterP95TargetMs,
      },
      lastEvaluatedAt: null,
    },
    counters: {
      completed: 0,
      expired: 0,
      matchedByTrace: 0,
      matchedBySlotValue: 0,
    },
  };
}

function createAlertState() {
  return {
    active: [],
    recent: [],
  };
}

function computePerformanceHealth({
  roundTrip = {},
  thresholds = {},
  hostTimestampMs = Date.now(),
}) {
  const reasons = [];
  if (!roundTrip.sampleCount) {
    return {
      status: 'no_data',
      reasons: ['No round-trip samples yet'],
      thresholds,
      lastEvaluatedAt: new Date(hostTimestampMs).toISOString(),
    };
  }

  if (
    Number.isFinite(roundTrip.p95Ms) &&
    Number.isFinite(thresholds.p95Ms) &&
    roundTrip.p95Ms > thresholds.p95Ms
  ) {
    reasons.push(
      `Round-trip p95 ${roundTrip.p95Ms} ms exceeds ${thresholds.p95Ms} ms`,
    );
  }
  if (
    Number.isFinite(roundTrip.jitterP95Ms) &&
    Number.isFinite(thresholds.jitterP95Ms) &&
    roundTrip.jitterP95Ms > thresholds.jitterP95Ms
  ) {
    reasons.push(
      `Round-trip jitter p95 ${roundTrip.jitterP95Ms} ms exceeds ${thresholds.jitterP95Ms} ms`,
    );
  }

  return {
    status: reasons.length ? 'warn' : 'ok',
    reasons: reasons.length
      ? reasons
      : ['Round-trip metrics are within target'],
    thresholds,
    lastEvaluatedAt: new Date(hostTimestampMs).toISOString(),
  };
}

function quantileFromSorted(sortedValues, percentile) {
  if (!Array.isArray(sortedValues) || !sortedValues.length) return null;
  const clamped = Math.min(1, Math.max(0, Number(percentile) || 0));
  const index = Math.max(0, Math.ceil(clamped * sortedValues.length) - 1);
  return sortedValues[index];
}

function computeDistribution(values = []) {
  if (!Array.isArray(values) || !values.length) {
    return {
      sampleCount: 0,
      minMs: null,
      maxMs: null,
      meanMs: null,
      p50Ms: null,
      p95Ms: null,
      lastMs: null,
    };
  }
  const sorted = [...values].sort((a, b) => a - b);
  const sum = values.reduce((acc, value) => acc + value, 0);
  return {
    sampleCount: values.length,
    minMs: sorted[0],
    maxMs: sorted[sorted.length - 1],
    meanMs: Number((sum / values.length).toFixed(3)),
    p50Ms: quantileFromSorted(sorted, 0.5),
    p95Ms: quantileFromSorted(sorted, 0.95),
    lastMs: values[values.length - 1],
  };
}

function normalizeTimestampMs(raw) {
  if (raw === undefined || raw === null) return null;
  if (typeof raw === 'number' && Number.isFinite(raw)) {
    return Math.trunc(raw);
  }
  if (typeof raw === 'string') {
    const trimmed = raw.trim();
    if (!trimmed) return null;
    const asInt = Number(trimmed);
    if (Number.isFinite(asInt)) return Math.trunc(asInt);
    const parsedDate = Date.parse(trimmed);
    if (Number.isFinite(parsedDate)) return Math.trunc(parsedDate);
  }
  return null;
}

function extractTimestampMs(payload) {
  if (!payload || typeof payload !== 'object') return null;
  const directCandidates = [
    payload.timestamp,
    payload.timestampMs,
    payload.ts,
    payload.tsMs,
    payload.time,
    payload.timeMs,
  ];
  for (const candidate of directCandidates) {
    const value = normalizeTimestampMs(candidate);
    if (value !== null) return value;
  }
  const nestedCandidates = [
    payload.meta?.timestamp,
    payload.meta?.timestampMs,
    payload.meta?.ts,
    payload.meta?.tsMs,
    payload.trace?.timestamp,
    payload.trace?.timestampMs,
  ];
  for (const candidate of nestedCandidates) {
    const value = normalizeTimestampMs(candidate);
    if (value !== null) return value;
  }
  return null;
}

function sanitizeTraceId(raw) {
  if (typeof raw !== 'string') return null;
  const trimmed = raw.trim();
  if (!trimmed) return null;
  return trimmed.slice(0, 96);
}

function extractTraceId(payload) {
  if (!payload || typeof payload !== 'object') return null;
  const directCandidates = [
    payload.traceId,
    payload.trace_id,
    payload.traceID,
    payload.id,
  ];
  for (const candidate of directCandidates) {
    const value = sanitizeTraceId(candidate);
    if (value) return value;
  }
  const nestedCandidates = [
    payload.meta?.traceId,
    payload.meta?.trace_id,
    payload.trace?.id,
    payload.trace?.traceId,
  ];
  for (const candidate of nestedCandidates) {
    const value = sanitizeTraceId(candidate);
    if (value) return value;
  }
  return null;
}

function midiEchoKey(status, cc, value) {
  return `${status}:${cc}:${value}`;
}

// Core bridge service used by both the CLI and the browser-driven bridge server.
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
  let routeSeq = 0;
  let traceSeq = 0;
  let pendingSeq = 0;
  let alertSeq = 0;
  const recentTelemetryMidi = new Map();
  const alertLastRaisedByKey = new Map();
  const pendingCommandsById = new Map();
  const pendingCommandsByTraceId = new Map();
  const pendingCommandIdsBySlot = new Map();
  const roundTripSamplesMs = [];
  const roundTripCounters = {
    completed: 0,
    expired: 0,
    matchedByTrace: 0,
    matchedBySlotValue: 0,
  };

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

  function recordRoute(partial = {}) {
    const hostTimestampMs =
      normalizeTimestampMs(partial.hostTimestampMs) || Date.now();
    const entry = {
      id: ++routeSeq,
      at: new Date(hostTimestampMs).toISOString(),
      hostTimestampMs,
      flow: partial.flow || 'unknown',
      kind: partial.kind || 'unknown',
      traceId: sanitizeTraceId(partial.traceId) || null,
      sourceTimestampMs: normalizeTimestampMs(partial.sourceTimestampMs),
      slot:
        partial.slot === undefined || partial.slot === null
          ? null
          : Number(partial.slot),
      value:
        partial.value === undefined || partial.value === null
          ? null
          : Number(partial.value),
      count:
        partial.count === undefined || partial.count === null
          ? null
          : Number(partial.count),
      status:
        partial.status === undefined || partial.status === null
          ? null
          : Number(partial.status),
      latencyMs:
        partial.latencyMs === undefined || partial.latencyMs === null
          ? null
          : Number(partial.latencyMs),
      address: partial.address || null,
      reason: partial.reason || null,
    };
    state.routes.push(entry);
    if (state.routes.length > ROUTE_LOG_LIMIT) {
      state.routes.splice(0, state.routes.length - ROUTE_LOG_LIMIT);
    }
    state.lastRouteAt = entry.at;
    state.lastRouteTraceId = entry.traceId;
    events.emit('route', clone(entry));
    emitState();
    return entry;
  }

  function raiseAlert({
    code,
    severity = 'warn',
    message,
    details = null,
    sticky = true,
    hostTimestampMs = Date.now(),
  }) {
    const normalizedCode =
      typeof code === 'string' && code.trim()
        ? code.trim().slice(0, 64)
        : 'bridge_alert';
    const normalizedSeverity =
      severity === 'error' || severity === 'warn' || severity === 'info'
        ? severity
        : 'warn';
    const normalizedMessage =
      typeof message === 'string' && message.trim()
        ? message.trim()
        : normalizedCode;
    const dedupeKey = normalizedCode;
    const suppressionMs = parsePositiveInt(
      config.alertSuppressionMs,
      DEFAULT_ALERT_SUPPRESSION_MS,
    );
    const lastRaisedAt = alertLastRaisedByKey.get(dedupeKey);
    if (
      Number.isFinite(lastRaisedAt) &&
      hostTimestampMs - lastRaisedAt < suppressionMs
    ) {
      return null;
    }
    alertLastRaisedByKey.set(dedupeKey, hostTimestampMs);

    alertSeq += 1;
    const alert = {
      id: alertSeq,
      at: new Date(hostTimestampMs).toISOString(),
      code: normalizedCode,
      severity: normalizedSeverity,
      message: normalizedMessage,
      details: details ? clone(details) : null,
    };

    state.alerts.recent.push(alert);
    if (state.alerts.recent.length > ALERT_LOG_LIMIT) {
      state.alerts.recent.splice(
        0,
        state.alerts.recent.length - ALERT_LOG_LIMIT,
      );
    }

    if (sticky) {
      const activeIndex = state.alerts.active.findIndex(
        (entry) => entry.code === normalizedCode,
      );
      if (activeIndex >= 0) {
        state.alerts.active[activeIndex] = alert;
      } else {
        state.alerts.active.push(alert);
      }
    }
    events.emit('alert', clone(alert));
    emitState();
    return alert;
  }

  function resolveAlert(
    code,
    { reason = 'resolved', hostTimestampMs = Date.now() } = {},
  ) {
    const normalizedCode =
      typeof code === 'string' && code.trim() ? code.trim() : null;
    if (!normalizedCode || !state.alerts.active.length) return 0;
    const before = state.alerts.active.length;
    state.alerts.active = state.alerts.active.filter(
      (entry) => entry.code !== normalizedCode,
    );
    const removed = before - state.alerts.active.length;
    if (removed > 0) {
      recordRoute({
        flow: 'alerts',
        kind: 'resolved',
        reason,
        hostTimestampMs,
        count: removed,
      });
    }
    return removed;
  }

  function clearAlerts({
    reason = 'operator_clear',
    hostTimestampMs = Date.now(),
  } = {}) {
    const cleared = state.alerts.active.length;
    state.alerts.active = [];
    if (cleared > 0) {
      raiseAlert({
        code: 'alerts_cleared',
        severity: 'info',
        message: `Operator cleared ${cleared} active alert(s)`,
        details: { count: cleared, reason },
        sticky: false,
        hostTimestampMs,
      });
      recordRoute({
        flow: 'alerts',
        kind: 'clear',
        reason,
        hostTimestampMs,
        count: cleared,
      });
    } else {
      emitState();
    }
    return getState();
  }

  function refreshPerformance(
    hostTimestampMs = Date.now(),
    bumpUpdated = false,
  ) {
    const distribution = computeDistribution(roundTripSamplesMs);
    const jitterSamples = [];
    for (let index = 1; index < roundTripSamplesMs.length; index += 1) {
      jitterSamples.push(
        Math.abs(roundTripSamplesMs[index] - roundTripSamplesMs[index - 1]),
      );
    }
    const jitter = computeDistribution(jitterSamples);
    const previousUpdatedAt =
      state.performance?.roundTrip?.lastUpdatedAt || null;
    const thresholds = {
      p95Ms: config.rtP95TargetMs,
      jitterP95Ms: config.rtJitterP95TargetMs,
    };
    const health = computePerformanceHealth({
      roundTrip: {
        ...distribution,
        pending: pendingCommandsById.size,
        jitterP95Ms: jitter.p95Ms,
      },
      thresholds,
      hostTimestampMs,
    });
    setState({
      performance: {
        roundTrip: {
          windowSize: ROUND_TRIP_WINDOW,
          pending: pendingCommandsById.size,
          sampleCount: distribution.sampleCount,
          minMs: distribution.minMs,
          maxMs: distribution.maxMs,
          meanMs: distribution.meanMs,
          p50Ms: distribution.p50Ms,
          p95Ms: distribution.p95Ms,
          lastMs: distribution.lastMs,
          jitterMeanMs: jitter.meanMs,
          jitterP95Ms: jitter.p95Ms,
          lastUpdatedAt: bumpUpdated
            ? new Date(hostTimestampMs).toISOString()
            : previousUpdatedAt,
        },
        health,
        counters: clone(roundTripCounters),
      },
    });
    if (health.status === 'warn') {
      raiseAlert({
        code: 'performance_warn',
        severity: 'warn',
        message:
          Array.isArray(health.reasons) && health.reasons.length
            ? health.reasons[0]
            : 'Round-trip performance exceeded thresholds',
        details: {
          reasons: health.reasons,
          thresholds: health.thresholds,
          p95Ms: distribution.p95Ms,
          jitterP95Ms: jitter.p95Ms,
          sampleCount: distribution.sampleCount,
        },
        hostTimestampMs,
      });
    } else {
      resolveAlert('performance_warn', {
        reason: health.status === 'ok' ? 'back_within_target' : 'no_data',
        hostTimestampMs,
      });
    }
  }

  function detachPendingCommand(pendingId) {
    const pending = pendingCommandsById.get(pendingId);
    if (!pending) return null;
    pendingCommandsById.delete(pendingId);
    if (pending.traceId) {
      const mappedId = pendingCommandsByTraceId.get(pending.traceId);
      if (mappedId === pendingId) {
        pendingCommandsByTraceId.delete(pending.traceId);
      }
    }
    const slotQueue = pendingCommandIdsBySlot.get(pending.slot);
    if (slotQueue) {
      const nextQueue = slotQueue.filter((id) => id !== pendingId);
      if (nextQueue.length) {
        pendingCommandIdsBySlot.set(pending.slot, nextQueue);
      } else {
        pendingCommandIdsBySlot.delete(pending.slot);
      }
    }
    return pending;
  }

  function queuePendingCommand({
    slot,
    value,
    traceId,
    hostTimestampMs = Date.now(),
    source = 'unknown',
  }) {
    pendingSeq += 1;
    const pending = {
      id: pendingSeq,
      slot,
      value,
      traceId: sanitizeTraceId(traceId),
      hostTimestampMs,
      source,
    };
    pendingCommandsById.set(pending.id, pending);
    if (pending.traceId) {
      pendingCommandsByTraceId.set(pending.traceId, pending.id);
    }
    const slotQueue = pendingCommandIdsBySlot.get(slot) || [];
    slotQueue.push(pending.id);
    pendingCommandIdsBySlot.set(slot, slotQueue);
    refreshPerformance(hostTimestampMs, false);
    return pending;
  }

  function expirePendingCommands(now = Date.now()) {
    let expiredAny = false;
    for (const [pendingId, pending] of pendingCommandsById.entries()) {
      if (now - pending.hostTimestampMs <= PENDING_COMMAND_TTL_MS) continue;
      detachPendingCommand(pendingId);
      roundTripCounters.expired += 1;
      expiredAny = true;
    }
    if (expiredAny) {
      refreshPerformance(now, true);
    }
  }

  function pushRoundTripSample({
    latencyMs,
    pending,
    hostTimestampMs,
    sourceTimestampMs = null,
    matchedBy,
    traceId = null,
  }) {
    if (!Number.isFinite(latencyMs) || latencyMs < 0) return;
    roundTripSamplesMs.push(latencyMs);
    if (roundTripSamplesMs.length > ROUND_TRIP_WINDOW) {
      roundTripSamplesMs.splice(
        0,
        roundTripSamplesMs.length - ROUND_TRIP_WINDOW,
      );
    }
    roundTripCounters.completed += 1;
    if (matchedBy === 'trace') {
      roundTripCounters.matchedByTrace += 1;
    } else {
      roundTripCounters.matchedBySlotValue += 1;
    }
    recordRoute({
      flow: 'roundtrip',
      kind: 'latency_sample',
      slot: pending?.slot ?? null,
      value: pending?.value ?? null,
      traceId: traceId || pending?.traceId || null,
      sourceTimestampMs,
      hostTimestampMs,
      latencyMs,
      reason: matchedBy,
    });
    refreshPerformance(hostTimestampMs, true);
  }

  function matchPendingRoundTrips({
    slots,
    telemetryTraceId,
    hostTimestampMs,
    sourceTimestampMs,
  }) {
    if (!Array.isArray(slots)) return;
    expirePendingCommands(hostTimestampMs);

    if (telemetryTraceId && pendingCommandsByTraceId.has(telemetryTraceId)) {
      const pendingId = pendingCommandsByTraceId.get(telemetryTraceId);
      const pending = pendingCommandsById.get(pendingId);
      if (
        pending &&
        slots[pending.slot] !== undefined &&
        slots[pending.slot] === pending.value
      ) {
        detachPendingCommand(pendingId);
        pushRoundTripSample({
          latencyMs: hostTimestampMs - pending.hostTimestampMs,
          pending,
          hostTimestampMs,
          sourceTimestampMs,
          matchedBy: 'trace',
          traceId: telemetryTraceId,
        });
      }
    }

    for (let slot = 0; slot < slots.length; slot += 1) {
      const queue = pendingCommandIdsBySlot.get(slot);
      if (!queue || !queue.length) continue;
      const slotValue = slots[slot];
      let matchingId = null;
      for (const pendingId of queue) {
        const pending = pendingCommandsById.get(pendingId);
        if (!pending) continue;
        if (pending.value === slotValue) {
          matchingId = pendingId;
          break;
        }
      }
      if (matchingId === null) continue;
      const pending = detachPendingCommand(matchingId);
      if (!pending) continue;
      pushRoundTripSample({
        latencyMs: hostTimestampMs - pending.hostTimestampMs,
        pending,
        hostTimestampMs,
        sourceTimestampMs,
        matchedBy: 'slot_value',
        traceId: telemetryTraceId,
      });
    }
  }

  function clearPerformanceTracking() {
    pendingSeq = 0;
    pendingCommandsById.clear();
    pendingCommandsByTraceId.clear();
    pendingCommandIdsBySlot.clear();
    roundTripSamplesMs.splice(0, roundTripSamplesMs.length);
    roundTripCounters.completed = 0;
    roundTripCounters.expired = 0;
    roundTripCounters.matchedByTrace = 0;
    roundTripCounters.matchedBySlotValue = 0;
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

  // Evict stale telemetry markers so feedback suppression stays bounded.
  function pruneTelemetryMarkers(now = Date.now()) {
    const maxAge = Math.max(1, Number(config.feedbackWindowMs) || 1);
    for (const [key, seenAt] of recentTelemetryMidi.entries()) {
      if (now - seenAt > maxAge) {
        recentTelemetryMidi.delete(key);
      }
    }
  }

  function markTelemetryMidi(status, cc, value) {
    if (config.allowFeedbackLoops) return;
    pruneTelemetryMarkers();
    recentTelemetryMidi.set(midiEchoKey(status, cc, value), Date.now());
  }

  function shouldSuppressMidiEcho(status, cc, value) {
    if (config.allowFeedbackLoops) return false;
    const now = Date.now();
    pruneTelemetryMarkers(now);
    const seenAt = recentTelemetryMidi.get(midiEchoKey(status, cc, value));
    if (!seenAt) return false;
    return now - seenAt <= config.feedbackWindowMs;
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

  // Forward slot/envelope telemetry to OSC listeners using the configured outbound port.
  function sendOscTelemetry(address, args, routeMeta = {}) {
    if (!udp || typeof udp.send !== 'function') return;
    try {
      udp.send({ address, args }, config.oscHost, config.oscPort);
      recordRoute({
        flow: 'serial->osc',
        kind: 'telemetry',
        address,
        count: Array.isArray(args) ? args.length : null,
        traceId: routeMeta.traceId,
        sourceTimestampMs: routeMeta.sourceTimestampMs,
        hostTimestampMs: routeMeta.hostTimestampMs,
      });
    } catch (err) {
      pushLog('error', `udp send error: ${err.message}`);
    }
  }

  // Mirror telemetry onto virtual MIDI CC lanes for DAW/host consumption.
  function sendMidiTelemetry(channelBase, values, routeMeta = {}) {
    if (!midiOut || !Array.isArray(values)) return;
    let sentCount = 0;
    values.forEach((value, index) => {
      try {
        markTelemetryMidi(channelBase, index, value);
        midiOut.send([channelBase, index, value]);
        sentCount += 1;
      } catch (err) {
        pushLog('error', `MIDI out error: ${err.message}`);
      }
    });
    if (sentCount) {
      recordRoute({
        flow: 'serial->midi',
        kind: 'telemetry',
        status: channelBase,
        count: sentCount,
        traceId: routeMeta.traceId,
        sourceTimestampMs: routeMeta.sourceTimestampMs,
        hostTimestampMs: routeMeta.hostTimestampMs,
      });
    }
  }

  // Parse firmware serial output, update bridge state, and rebroadcast telemetry.
  function handleSerialLine(line) {
    const trimmed = String(line || '').trim();
    if (!trimmed) return;
    if (trimmed === '{"hello":"mn42"}') {
      setState({ ready: true, serialConnected: true, lastError: null });
    }
    broadcastLine(trimmed);
    if (trimmed.length > MAX_SERIAL_LINE_LEN) {
      bumpCounter('serialOversizeDrops');
      pushLog('warn', 'serial packet too big');
      return;
    }

    let data;
    const looksJson = trimmed.startsWith('{') || trimmed.startsWith('[');
    try {
      data = JSON.parse(trimmed);
    } catch (_) {
      if (looksJson) {
        bumpCounter('serialParseErrors');
      }
      return;
    }

    inspectManifest(data);

    const hasTelemetry =
      data.type === 'telemetry' || data.slots || data.envelopes;
    const hostTimestampMs = Date.now();
    const sourceTimestampMs = extractTimestampMs(data);
    const telemetryTraceId = extractTraceId(data) || nextTraceId('serial');

    if (hasTelemetry) {
      setState({
        lastTelemetryAt: new Date(hostTimestampMs).toISOString(),
        timing: {
          lastSerialSourceTimestampMs: sourceTimestampMs,
          lastSerialHostTimestampMs: hostTimestampMs,
          lastSerialSkewMs:
            sourceTimestampMs === null
              ? null
              : hostTimestampMs - sourceTimestampMs,
        },
      });
    }

    if (Array.isArray(data.slots)) {
      matchPendingRoundTrips({
        slots: data.slots,
        telemetryTraceId,
        hostTimestampMs,
        sourceTimestampMs,
      });
      sendOscTelemetry('/mn42/slots', data.slots, {
        traceId: telemetryTraceId,
        sourceTimestampMs,
        hostTimestampMs,
      });
      sendMidiTelemetry(0xb0, data.slots, {
        traceId: telemetryTraceId,
        sourceTimestampMs,
        hostTimestampMs,
      });
    }
    if (Array.isArray(data.envelopes)) {
      sendOscTelemetry('/mn42/envelopes', data.envelopes, {
        traceId: telemetryTraceId,
        sourceTimestampMs,
        hostTimestampMs,
      });
      sendMidiTelemetry(0xb1, data.envelopes, {
        traceId: telemetryTraceId,
        sourceTimestampMs,
        hostTimestampMs,
      });
    }
  }

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

  // Reopen the serial side after transient disconnects unless the operator explicitly stopped the service.
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

  // Wire the serial device and newline parser into the bridge event loop.
  function attachSerial() {
    const { SerialPort, ReadlineParser } = serialApi;
    serial = new SerialPort({
      path: config.serialName,
      baudRate: SERIAL_BAUD,
    });
    parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));

    serial.on('open', () => {
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
    });

    serial.on('error', (err) => {
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
    });

    serial.on('close', () => {
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
    });

    parser.on('data', handleSerialLine);
  }

  // Bind inbound OSC control and outbound OSC telemetry.
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
        if (data.length > MAX_CMD_LEN) {
          bumpCounter('badOscCmdDrops');
          pushLog('warn', 'OSC cmd too big');
          return;
        }
        try {
          data = JSON.parse(data);
        } catch (_) {
          bumpCounter('badOscCmdDrops');
          pushLog('warn', 'bad OSC JSON');
          return;
        }
      }
      const cmd = validateCmd(data);
      if (!cmd) {
        bumpCounter('badOscCmdDrops');
        recordRoute({
          flow: 'osc->serial',
          kind: 'drop_invalid',
          address: '/mn42/cmd',
          traceId: extractTraceId(data) || nextTraceId('osc'),
          sourceTimestampMs: extractTimestampMs(data),
          hostTimestampMs: Date.now(),
        });
        pushLog('warn', 'bad OSC cmd', data);
        return;
      }
      recordRoute({
        flow: 'osc->serial',
        kind: 'command',
        address: '/mn42/cmd',
        slot: cmd.slot,
        value: cmd.value,
        traceId: extractTraceId(data) || nextTraceId('osc'),
        sourceTimestampMs: extractTimestampMs(data),
        hostTimestampMs: Date.now(),
      });
      queuePendingCommand({
        slot: cmd.slot,
        value: cmd.value,
        traceId: extractTraceId(data),
        hostTimestampMs: Date.now(),
        source: 'osc',
      });
      sendLine(formatLiveValueCommand(cmd));
    });

    udp.open();
  }

  // Bind the virtual MIDI in/out pair used for DAW-facing workflows.
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
        const midiTraceId = nextTraceId('midi');
        const hostTimestampMs = Date.now();
        if (shouldSuppressMidiEcho(arr[0], arr[1], arr[2])) {
          bumpCounter('feedbackSuppressed');
          recordRoute({
            flow: 'midi->serial',
            kind: 'drop_feedback',
            status: arr[0],
            slot: arr[1],
            value: arr[2],
            traceId: midiTraceId,
            reason: 'feedback_window',
            hostTimestampMs,
          });
          return;
        }
        const cmd = validateCmd({
          cmd: 'SET_SLOT_VALUE',
          slot: arr[1],
          value: arr[2],
        });
        if (!cmd) {
          bumpCounter('badMidiCmdDrops');
          recordRoute({
            flow: 'midi->serial',
            kind: 'drop_invalid',
            status: arr[0],
            slot: arr[1],
            value: arr[2],
            traceId: midiTraceId,
            hostTimestampMs,
          });
          pushLog('warn', 'dropping bad MIDI CC', arr);
          return;
        }
        recordRoute({
          flow: 'midi->serial',
          kind: 'command',
          status: arr[0],
          slot: cmd.slot,
          value: cmd.value,
          traceId: midiTraceId,
          hostTimestampMs,
        });
        queuePendingCommand({
          slot: cmd.slot,
          value: cmd.value,
          traceId: midiTraceId,
          hostTimestampMs,
          source: 'midi',
        });
        sendLine(formatLiveValueCommand(cmd));
      });
    }
  }

  // Start all bridge transports and publish a fresh runtime snapshot.
  async function start() {
    if (running) return getState();
    await loadDeps();
    stopping = false;
    manualStop = false;
    running = true;
    routeSeq = 0;
    traceSeq = 0;
    alertSeq = 0;
    recentTelemetryMidi.clear();
    alertLastRaisedByKey.clear();
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
      recentTelemetryMidi.clear();
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
