const {
  computeDistribution,
  computePerformanceHealth,
  sanitizeTraceId,
} = require('./performance');

function createPerformanceTracker({
  setPerformanceState,
  getPerformanceState,
  getTargets,
  raiseAlert,
  resolveAlert,
  recordRoute,
  now = () => Date.now(),
  roundTripWindow = 200,
  pendingCommandTtlMs = 5000,
} = {}) {
  let pendingSeq = 0;
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

  function pendingCount() {
    return pendingCommandsById.size;
  }

  function refresh(hostTimestampMs = now(), bumpUpdated = false) {
    const distribution = computeDistribution(roundTripSamplesMs);
    const jitterSamples = [];
    for (let index = 1; index < roundTripSamplesMs.length; index += 1) {
      jitterSamples.push(
        Math.abs(roundTripSamplesMs[index] - roundTripSamplesMs[index - 1]),
      );
    }
    const jitter = computeDistribution(jitterSamples);
    const previousUpdatedAt =
      getPerformanceState()?.roundTrip?.lastUpdatedAt || null;
    const thresholds = getTargets();
    const health = computePerformanceHealth({
      roundTrip: {
        ...distribution,
        pending: pendingCount(),
        jitterP95Ms: jitter.p95Ms,
      },
      thresholds,
      hostTimestampMs,
    });

    setPerformanceState({
      roundTrip: {
        windowSize: roundTripWindow,
        pending: pendingCount(),
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
      counters: { ...roundTripCounters },
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
    hostTimestampMs = now(),
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
    refresh(hostTimestampMs, false);
    return pending;
  }

  function expirePendingCommands(nowMs = now()) {
    let expiredAny = false;
    for (const [pendingId, pending] of pendingCommandsById.entries()) {
      if (nowMs - pending.hostTimestampMs <= pendingCommandTtlMs) continue;
      detachPendingCommand(pendingId);
      roundTripCounters.expired += 1;
      expiredAny = true;
    }
    if (expiredAny) refresh(nowMs, true);
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
    if (roundTripSamplesMs.length > roundTripWindow) {
      roundTripSamplesMs.splice(
        0,
        roundTripSamplesMs.length - roundTripWindow,
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
    refresh(hostTimestampMs, true);
  }

  function matchPendingRoundTrips({
    slots,
    telemetryTraceId,
    hostTimestampMs = now(),
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

  function clear() {
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

  return {
    queuePendingCommand,
    matchPendingRoundTrips,
    refresh,
    clear,
  };
}

module.exports = {
  createPerformanceTracker,
};
