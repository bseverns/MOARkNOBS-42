const DEFAULT_RT_P95_TARGET_MS = 10;
const DEFAULT_RT_JITTER_P95_TARGET_MS = 5;
const ROUND_TRIP_WINDOW = 200;

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
  roundTripWindow = ROUND_TRIP_WINDOW,
} = {}) {
  return {
    roundTrip: {
      windowSize: roundTripWindow,
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

module.exports = {
  DEFAULT_RT_P95_TARGET_MS,
  DEFAULT_RT_JITTER_P95_TARGET_MS,
  ROUND_TRIP_WINDOW,
  createTimingState,
  createCounterState,
  createPerformanceState,
  createAlertState,
  computePerformanceHealth,
  quantileFromSorted,
  computeDistribution,
  normalizeTimestampMs,
  extractTimestampMs,
  sanitizeTraceId,
  extractTraceId,
  midiEchoKey,
};
