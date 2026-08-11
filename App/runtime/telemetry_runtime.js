export function createTelemetryRuntime({
  clone,
  emit,
  notifyStatus,
  flushDelayMs = 50,
  delayedAfterMs = 1000,
  staleAfterMs = 3000,
  now = () => Date.now(),
  setTimeoutFn = setTimeout,
  clearTimeoutFn = clearTimeout
} = {}) {
  let queuedTelemetry = null;
  let telemetryTraceId = null;
  let telemetryTimer = null;
  let freshnessTimer = null;
  let receivedAt = null;
  let freshness = 'stale';

  function updateFreshness() {
    const ageMs = receivedAt === null ? null : Math.max(0, now() - receivedAt);
    const next = ageMs === null || ageMs >= staleAfterMs ? 'stale' : ageMs >= delayedAfterMs ? 'delayed' : 'live';
    if (next !== freshness) {
      freshness = next;
      emit('telemetry-health', { freshness, receivedAt, ageMs });
    }
    if (freshnessTimer) clearTimeoutFn(freshnessTimer);
    if (next !== 'stale') {
      const untilNext = next === 'live' ? delayedAfterMs - ageMs : staleAfterMs - ageMs;
      freshnessTimer = setTimeoutFn(updateFreshness, Math.max(1, untilNext));
    } else {
      freshnessTimer = null;
    }
  }

  function mergeTelemetryChunk(current, msg) {
    const next = { ...(current || {}), ...msg };

    if (Array.isArray(msg.slotArgs)) {
      const byIndex = new Map();

      for (const arg of current?.slotArgs || []) {
        if (Number.isInteger(arg.index)) byIndex.set(arg.index, arg);
      }

      for (const arg of msg.slotArgs) {
        if (Number.isInteger(arg.index)) byIndex.set(arg.index, arg);
      }

      next.slotArgs = [...byIndex.entries()].sort(([a], [b]) => a - b).map(([, arg]) => arg);
    }

    if (Array.isArray(msg.slotContributions)) {
      const byIndex = new Map();

      for (const entry of current?.slotContributions || []) {
        if (Number.isInteger(entry?.index)) byIndex.set(entry.index, entry);
      }

      for (const entry of msg.slotContributions) {
        if (Number.isInteger(entry?.index)) byIndex.set(entry.index, entry);
      }

      next.slotContributions = [...byIndex.entries()]
        .sort(([a], [b]) => a - b)
        .map(([, entry]) => entry);
    }

    next.scopes = [...(current?.scopes || []), msg.scope].filter(Boolean);
    return next;
  }

  function flushTelemetry() {
    if (telemetryTimer) {
      clearTimeoutFn(telemetryTimer);
      telemetryTimer = null;
    }
    telemetryTraceId = null;
    if (!queuedTelemetry) return;

    const frame = clone(queuedTelemetry);
    emit('telemetry', { ...frame, receivedAt });
    notifyStatus({ type: 'telemetry', ...frame });
    queuedTelemetry = null;
  }

  function queueTelemetryFrame(msg) {
    if (!msg || typeof msg !== 'object') return;
    receivedAt = now();
    updateFreshness();

    const traceId = msg.traceId || null;

    if (telemetryTraceId && traceId && traceId !== telemetryTraceId) {
      flushTelemetry();
    }

    if (traceId) {
      telemetryTraceId = traceId;
    }

    queuedTelemetry = mergeTelemetryChunk(queuedTelemetry, msg);

    if (!telemetryTimer) {
      telemetryTimer = setTimeoutFn(flushTelemetry, flushDelayMs);
    }
  }

  function reset({ freshness: nextFreshness = 'stale' } = {}) {
    if (telemetryTimer) clearTimeoutFn(telemetryTimer);
    if (freshnessTimer) clearTimeoutFn(freshnessTimer);
    telemetryTimer = null;
    freshnessTimer = null;
    queuedTelemetry = null;
    telemetryTraceId = null;
    receivedAt = null;
    if (freshness !== nextFreshness) {
      freshness = nextFreshness;
      emit('telemetry-health', { freshness, receivedAt: null, ageMs: null });
    } else {
      freshness = nextFreshness;
    }
  }

  return {
    flushTelemetry,
    queueTelemetryFrame,
    reset,
    getHealth: () => ({ freshness, receivedAt, ageMs: receivedAt === null ? null : now() - receivedAt })
  };
}
