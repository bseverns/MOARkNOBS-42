function createSerialLineHandler({
  maxSerialLineLen,
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
  now,
}) {
  const timestampNow = typeof now === 'function' ? now : () => Date.now();
  const isObject = (value) =>
    Boolean(value) && typeof value === 'object' && !Array.isArray(value);
  const isFiniteNumber = (value) =>
    typeof value === 'number' && Number.isFinite(value);
  const isNumericVector = (value) =>
    Array.isArray(value) && value.length > 0 && value.every(isFiniteNumber);

  function sendJsonOsc(address, payload, routeMeta) {
    if (payload === undefined || payload === null) return;
    sendOscTelemetry(address, [JSON.stringify(payload)], {
      ...routeMeta,
      kind: 'telemetry_json',
    });
  }

  function sendNumberOsc(address, value, routeMeta) {
    if (!isFiniteNumber(value)) return;
    sendOscTelemetry(address, [value], routeMeta);
  }

  function sendBooleanOsc(address, value, routeMeta) {
    if (typeof value !== 'boolean') return;
    sendOscTelemetry(address, [value ? 1 : 0], routeMeta);
  }

  function forwardRichTelemetry(data, routeMeta) {
    if (isFiniteNumber(data.currentSlot)) {
      sendNumberOsc('/mn42/current-slot', data.currentSlot, routeMeta);
      sendNumberOsc(
        '/mn42/telemetry/current-slot',
        data.currentSlot,
        routeMeta,
      );
    }

    if (isNumericVector(data.lfos)) {
      sendOscTelemetry('/mn42/lfos', data.lfos, routeMeta);
      sendOscTelemetry('/mn42/telemetry/lfos', data.lfos, routeMeta);
    }

    if (isNumericVector(data.efStatus)) {
      sendOscTelemetry('/mn42/ef/status', data.efStatus, routeMeta);
      sendOscTelemetry('/mn42/telemetry/ef-status', data.efStatus, routeMeta);
    }

    if (Array.isArray(data.lfo_config)) {
      sendJsonOsc('/mn42/lfo/config', data.lfo_config, routeMeta);
      sendJsonOsc('/mn42/telemetry/lfo-config', data.lfo_config, routeMeta);
    }

    if (Array.isArray(data.slotArgs)) {
      const payload = {
        scope: typeof data.scope === 'string' ? data.scope : null,
        slotArgs: data.slotArgs,
      };
      sendJsonOsc('/mn42/arg/slots', payload, routeMeta);
      sendJsonOsc('/mn42/telemetry/slot-args', payload, routeMeta);
    }

    if (isNumericVector(data.slotOutputs)) {
      sendOscTelemetry('/mn42/slot-outputs', data.slotOutputs, routeMeta);
      sendOscTelemetry(
        '/mn42/telemetry/slot-outputs',
        data.slotOutputs,
        routeMeta,
      );
    }

    if (Array.isArray(data.slotContributions)) {
      const payload = {
        scope: typeof data.scope === 'string' ? data.scope : null,
        slotContributions: data.slotContributions,
      };
      sendJsonOsc('/mn42/slot-contributions', payload, routeMeta);
      sendJsonOsc(
        '/mn42/telemetry/slot-contributions',
        payload,
        routeMeta,
      );
    }

    if (Array.isArray(data.argPair)) {
      sendOscTelemetry('/mn42/arg/pair', data.argPair, routeMeta);
      sendOscTelemetry('/mn42/telemetry/arg-pair', data.argPair, routeMeta);
    }

    sendBooleanOsc('/mn42/arg/enabled', data.argEnabled, routeMeta);
    if (typeof data.argMethod === 'string') {
      sendOscTelemetry('/mn42/arg/method', [data.argMethod], routeMeta);
    }

    if (isFiniteNumber(data.active_profile)) {
      sendNumberOsc('/mn42/profile/active', data.active_profile, routeMeta);
      sendNumberOsc(
        '/mn42/telemetry/active-profile',
        data.active_profile,
        routeMeta,
      );
    }

    if (isObject(data.diagnostics)) {
      sendJsonOsc('/mn42/diagnostics', data.diagnostics, routeMeta);
      sendJsonOsc('/mn42/telemetry/diagnostics', data.diagnostics, routeMeta);
    }

    if (isObject(data.clock)) {
      sendJsonOsc('/mn42/clock', data.clock, routeMeta);
      sendJsonOsc('/mn42/telemetry/clock', data.clock, routeMeta);
    }

    if (isObject(data.note_dynamics)) {
      sendJsonOsc('/mn42/note-dynamics', data.note_dynamics, routeMeta);
    }

    if (isObject(data.jitter)) {
      sendJsonOsc('/mn42/jitter', data.jitter, routeMeta);
    }
  }

  return function handleSerialLine(line) {
    const trimmed = String(line || '').trim();
    if (!trimmed) return;
    if (trimmed === '{"hello":"mn42"}') {
      // A raw HELLO proves the serial lane is alive, but bridge-ready requires
      // the cached device session to finish manifest/schema/config hydration.
      setState({ serialConnected: true, lastError: null });
    }
    broadcastLine(trimmed);
    if (trimmed.length > maxSerialLineLen) {
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
        deviceSession?.handleMalformedMessage?.(trimmed, _);
      }
      return;
    }

    inspectManifest(data);

    const slotTelemetry = isNumericVector(data.slots);
    const envelopeTelemetry = isNumericVector(data.envelopes);
    const hasTelemetry =
      data.type === 'telemetry' || slotTelemetry || envelopeTelemetry;
    const hostTimestampMs = timestampNow();
    const sourceTimestampMs = extractTimestampMs(data);
    const telemetryTraceId = extractTraceId(data) || nextTraceId('serial');
    const telemetryRouteMeta = {
      traceId: telemetryTraceId,
      sourceTimestampMs,
      hostTimestampMs,
      reason: typeof data.scope === 'string' ? data.scope : null,
    };
    Promise.resolve(
      deviceSession?.handleMessage?.(data, {
        rawLine: trimmed,
        hostTimestampMs,
        sourceTimestampMs,
        traceId: telemetryTraceId,
      }),
    ).catch((err) => {
      pushLog('error', `device session parse failed: ${err.message}`);
    });

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

    if (slotTelemetry) {
      matchPendingRoundTrips({
        slots: data.slots,
        telemetryTraceId,
        hostTimestampMs,
        sourceTimestampMs,
      });
      sendOscTelemetry('/mn42/slots', data.slots, {
        ...telemetryRouteMeta,
      });
      sendOscTelemetry('/mn42/telemetry/slots', data.slots, {
        ...telemetryRouteMeta,
      });
      sendMidiTelemetry(0xb0, data.slots, {
        ...telemetryRouteMeta,
      });
    }
    if (envelopeTelemetry) {
      sendOscTelemetry('/mn42/envelopes', data.envelopes, {
        ...telemetryRouteMeta,
      });
      sendOscTelemetry('/mn42/telemetry/envelopes', data.envelopes, {
        ...telemetryRouteMeta,
      });
      sendMidiTelemetry(0xb1, data.envelopes, {
        ...telemetryRouteMeta,
      });
    }
    if (hasTelemetry) {
      forwardRichTelemetry(data, telemetryRouteMeta);
    }
  };
}

module.exports = {
  createSerialLineHandler,
};
