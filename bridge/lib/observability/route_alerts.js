function createRouteAlertPolicy({
  state,
  emitState,
  emitEvent,
  cloneValue,
  normalizeTimestampMs,
  sanitizeTraceId,
  parsePositiveInt,
  getAlertSuppressionMs,
  defaultAlertSuppressionMs,
  routeLogLimit,
  alertLogLimit,
  now,
}) {
  let routeSeq = 0;
  let alertSeq = 0;
  const alertLastRaisedByKey = new Map();
  const timestampNow = typeof now === 'function' ? now : () => Date.now();

  function recordRoute(partial = {}) {
    const hostTimestampMs =
      normalizeTimestampMs(partial.hostTimestampMs) || timestampNow();
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
    if (state.routes.length > routeLogLimit) {
      state.routes.splice(0, state.routes.length - routeLogLimit);
    }
    state.lastRouteAt = entry.at;
    state.lastRouteTraceId = entry.traceId;
    emitEvent('route', cloneValue(entry));
    emitState();
    return entry;
  }

  function raiseAlert({
    code,
    severity = 'warn',
    message,
    details = null,
    sticky = true,
    hostTimestampMs = timestampNow(),
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
      getAlertSuppressionMs(),
      defaultAlertSuppressionMs,
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
      details: details ? cloneValue(details) : null,
    };

    state.alerts.recent.push(alert);
    if (state.alerts.recent.length > alertLogLimit) {
      state.alerts.recent.splice(0, state.alerts.recent.length - alertLogLimit);
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
    emitEvent('alert', cloneValue(alert));
    emitState();
    return alert;
  }

  function resolveAlert(
    code,
    { reason = 'resolved', hostTimestampMs = timestampNow() } = {},
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
    hostTimestampMs = timestampNow(),
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
  }

  function resetForRun() {
    routeSeq = 0;
    alertSeq = 0;
    alertLastRaisedByKey.clear();
  }

  return {
    recordRoute,
    raiseAlert,
    resolveAlert,
    clearAlerts,
    resetForRun,
  };
}

module.exports = {
  createRouteAlertPolicy,
};
