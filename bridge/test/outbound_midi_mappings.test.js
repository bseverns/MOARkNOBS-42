const { strict: assert } = require('node:assert');

const {
  normalizeOutboundMidiMappings,
  mappedMidiPackets,
} = require('../lib/config/outbound_midi_mappings');
const { createBridgeEgress } = require('../lib/transports/egress');

const mappings = normalizeOutboundMidiMappings([
  { id: 'macro', source: 'slots', sourceIndex: 3, channel: 5, controller: 21 },
  { id: 'ef', source: 'envelopes', index: 1, channel: 8, cc: 74 },
  { id: 'bad', source: 'lfos', sourceIndex: 0, channel: 0, controller: 999 },
]);

assert.equal(mappings.length, 2);
assert.deepEqual(mappedMidiPackets('slots', [0, 1, 2, 96], mappings), [
  { id: 'macro', packet: [0xb4, 21, 96] },
]);
assert.deepEqual(mappedMidiPackets('envelopes', [0, 200], mappings), [
  { id: 'ef', packet: [0xb7, 74, 127] },
]);

const sent = [];
const routes = [];
const egress = createBridgeEgress({
  getMidiOut: () => ({ send: (packet) => sent.push(packet) }),
  getMidiTelemetryMode: () => 'mapped',
  getOutboundMidiMappings: () => mappings,
  markTelemetryMidi() {},
  recordRoute: (route) => routes.push(route),
  pushLog() {},
});
egress.sendMidiTelemetry('slots', 0xb0, [0, 1, 2, 96], {
  traceId: 'mapped-1',
  hostTimestampMs: 1234,
});
assert.deepEqual(sent, [[0xb4, 21, 96]]);
assert.equal(routes[0].reason, 'configured_mapping');

console.log('outbound MIDI mappings are explicit, bounded, and source-aware');
