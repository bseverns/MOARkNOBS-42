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

  return function handleSerialLine(line) {
    const trimmed = String(line || '').trim();
    if (!trimmed) return;
    if (trimmed === '{"hello":"mn42"}') {
      setState({ ready: true, serialConnected: true, lastError: null });
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

    const hasTelemetry =
      data.type === 'telemetry' || data.slots || data.envelopes;
    const hostTimestampMs = timestampNow();
    const sourceTimestampMs = extractTimestampMs(data);
    const telemetryTraceId = extractTraceId(data) || nextTraceId('serial');
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
      sendOscTelemetry('/mn42/telemetry/slots', data.slots, {
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
      sendOscTelemetry('/mn42/telemetry/envelopes', data.envelopes, {
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
  };
}

module.exports = {
  createSerialLineHandler,
};
