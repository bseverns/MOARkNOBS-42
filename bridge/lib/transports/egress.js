const {
  eventAddressForKind,
  typedEventToMidiPackets,
} = require('../codec/typed_events');
const {
  sanitizeTraceId,
  normalizeTimestampMs,
} = require('../observability/performance');
const {
  eventMatchesMidiToOscMapping,
  buildMappedOscArgs,
} = require('../config/midi_osc_mappings');
const { mappedMidiPackets } = require('../config/outbound_midi_mappings');

function createBridgeEgress({
  getUdp,
  getMidiOut,
  getOscTarget,
  getMidiToOscMappings,
  getMidiTelemetryMode,
  getOutboundMidiMappings,
  markTelemetryMidi,
  recordRoute,
  pushLog,
} = {}) {
  function sendOscTelemetry(address, args, routeMeta = {}) {
    const udp = getUdp?.();
    if (!udp || typeof udp.send !== 'function') return;
    const target = getOscTarget?.() || {};
    try {
      udp.send({ address, args }, target.host, target.port);
      recordRoute({
        flow: routeMeta.flow || 'serial->osc',
        kind: routeMeta.kind || 'telemetry',
        address,
        count: Array.isArray(args) ? args.length : null,
        traceId: routeMeta.traceId,
        sourceTimestampMs: routeMeta.sourceTimestampMs,
        hostTimestampMs: routeMeta.hostTimestampMs,
        reason: routeMeta.reason || null,
      });
    } catch (err) {
      pushLog('error', `udp send error: ${err.message}`);
    }
  }

  function sendMidiTelemetry(source, channelBase, values, routeMeta = {}) {
    const midiOut = getMidiOut?.();
    if (!midiOut || !Array.isArray(values)) return;
    const mappedMode = getMidiTelemetryMode?.() === 'mapped';
    const messages = mappedMode
      ? mappedMidiPackets(source, values, getOutboundMidiMappings?.() || [])
      : values.map((value, index) => ({
          id: null,
          packet: [channelBase, index, value],
        }));
    let sentCount = 0;
    messages.forEach(({ packet }) => {
      try {
        markTelemetryMidi(packet[0], packet[1], packet[2]);
        midiOut.send(packet);
        sentCount += 1;
      } catch (err) {
        pushLog('error', `MIDI out error: ${err.message}`);
      }
    });
    if (sentCount) {
      recordRoute({
        flow: 'serial->midi',
        kind: 'telemetry',
        status: mappedMode ? null : channelBase,
        count: sentCount,
        traceId: routeMeta.traceId,
        sourceTimestampMs: routeMeta.sourceTimestampMs,
        hostTimestampMs: routeMeta.hostTimestampMs,
        reason: mappedMode ? 'configured_mapping' : 'legacy_compatibility',
      });
    }
  }

  function sendTypedEventToOsc(event, routeMeta = {}) {
    if (!event || typeof event !== 'object') return false;
    const payload = { ...event };
    const traceId = sanitizeTraceId(routeMeta.traceId);
    if (traceId) payload.traceId = traceId;
    const sourceTimestampMs = normalizeTimestampMs(routeMeta.sourceTimestampMs);
    if (sourceTimestampMs !== null) payload.timestampMs = sourceTimestampMs;
    const json = JSON.stringify(payload);
    sendOscTelemetry(eventAddressForKind(event.kind), [json], {
      flow: routeMeta.flow || 'midi->osc',
      kind: routeMeta.kind || 'event',
      reason: event.kind,
      traceId,
      sourceTimestampMs,
      hostTimestampMs: routeMeta.hostTimestampMs,
    });
    return true;
  }

  function sendTypedEventToMidi(event, routeMeta = {}) {
    const midiOut = getMidiOut?.();
    if (!midiOut || !event || typeof event !== 'object') return 0;
    const packets = typedEventToMidiPackets(event);
    if (!packets.length) return 0;
    let sentCount = 0;
    packets.forEach((packet) => {
      try {
        if (
          packet.length >= 3 &&
          Number.isInteger(packet[0]) &&
          Number.isInteger(packet[1]) &&
          Number.isInteger(packet[2]) &&
          (packet[0] & 0xf0) === 0xb0
        ) {
          markTelemetryMidi(packet[0], packet[1], packet[2]);
        }
        midiOut.send(packet);
        sentCount += 1;
      } catch (err) {
        pushLog('error', `MIDI out error: ${err.message}`);
      }
    });
    if (sentCount) {
      recordRoute({
        flow: routeMeta.flow || 'osc->midi',
        kind: routeMeta.kind || 'event',
        reason: routeMeta.reason || event.kind,
        count: sentCount,
        traceId: routeMeta.traceId,
        sourceTimestampMs: routeMeta.sourceTimestampMs,
        hostTimestampMs: routeMeta.hostTimestampMs,
      });
    }
    return sentCount;
  }

  function sendMappedMidiEventToOsc(event, routeMeta = {}) {
    const mappings = getMidiToOscMappings?.() || [];
    if (!Array.isArray(mappings) || !mappings.length) return 0;
    let sentCount = 0;
    mappings.forEach((mapping) => {
      if (!eventMatchesMidiToOscMapping(event, mapping)) return;
      const args = buildMappedOscArgs(event, mapping);
      if (!args) return;
      sendOscTelemetry(mapping.address, args, {
        flow: routeMeta.flow || 'midi->osc',
        kind: routeMeta.kind || 'mapping',
        reason: routeMeta.reason || mapping.id || 'cc_mapping',
        traceId: routeMeta.traceId,
        sourceTimestampMs: routeMeta.sourceTimestampMs,
        hostTimestampMs: routeMeta.hostTimestampMs,
      });
      sentCount += 1;
    });
    return sentCount;
  }

  return {
    sendOscTelemetry,
    sendMidiTelemetry,
    sendTypedEventToOsc,
    sendTypedEventToMidi,
    sendMappedMidiEventToOsc,
  };
}

module.exports = {
  createBridgeEgress,
};
