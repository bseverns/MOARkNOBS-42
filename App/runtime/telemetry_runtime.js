export function createTelemetryRuntime({
  clone,
  emit,
  notifyStatus,
  flushDelayMs = 50,
  setTimeoutFn = setTimeout,
  clearTimeoutFn = clearTimeout
} = {}) {
  let queuedTelemetry = null;
  let telemetryTraceId = null;
  let telemetryTimer = null;

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
    emit('telemetry', frame);
    notifyStatus({ type: 'telemetry', ...frame });
    queuedTelemetry = null;
  }

  function queueTelemetryFrame(msg) {
    if (!msg || typeof msg !== 'object') return;

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

  return {
    flushTelemetry,
    queueTelemetryFrame
  };
}
