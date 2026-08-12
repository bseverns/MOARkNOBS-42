const SOURCES = new Set(['slots', 'envelopes']);

function integerInRange(value, min, max) {
  const parsed = Number(value);
  return Number.isInteger(parsed) && parsed >= min && parsed <= max
    ? parsed
    : null;
}

function normalizeOutboundMidiMappings(rawMappings) {
  if (!Array.isArray(rawMappings)) return [];
  return rawMappings.slice(0, 128).flatMap((mapping, index) => {
    if (!mapping || typeof mapping !== 'object' || Array.isArray(mapping)) return [];
    const source = String(mapping.source || '').trim().toLowerCase();
    const sourceIndex = integerInRange(mapping.sourceIndex ?? mapping.index, 0, 127);
    const channel = integerInRange(mapping.channel, 1, 16);
    const controller = integerInRange(mapping.controller ?? mapping.cc, 0, 127);
    if (!SOURCES.has(source) || sourceIndex === null || channel === null || controller === null) {
      return [];
    }
    return [{
      id: String(mapping.id || `outbound-${index + 1}`).trim().slice(0, 80),
      source,
      sourceIndex,
      channel,
      controller,
    }];
  });
}

function mappedMidiPackets(source, values, mappings) {
  if (!Array.isArray(values)) return [];
  return normalizeOutboundMidiMappings(mappings).flatMap((mapping) => {
    if (mapping.source !== source || mapping.sourceIndex >= values.length) return [];
    const rawValue = Number(values[mapping.sourceIndex]);
    if (!Number.isFinite(rawValue)) return [];
    const value = Math.max(0, Math.min(127, Math.round(rawValue)));
    return [{
      id: mapping.id,
      packet: [0xb0 | (mapping.channel - 1), mapping.controller, value],
    }];
  });
}

module.exports = {
  normalizeOutboundMidiMappings,
  mappedMidiPackets,
};
