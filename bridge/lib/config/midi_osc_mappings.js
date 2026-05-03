function normalizeIntegerInRange(raw, min, max) {
  if (raw === null || raw === undefined) return null;
  const value =
    typeof raw === 'number'
      ? Math.trunc(raw)
      : Number.parseInt(String(raw).trim(), 10);
  if (!Number.isInteger(value) || value < min || value > max) return null;
  return value;
}

function normalizeNumber(raw, fallback) {
  if (raw === null || raw === undefined || raw === '') return fallback;
  const value = Number(raw);
  return Number.isFinite(value) ? value : fallback;
}

function normalizeString(raw) {
  if (typeof raw !== 'string') return '';
  return raw.trim();
}

function normalizeMidiToOscMappings(rawMappings) {
  if (!Array.isArray(rawMappings)) return [];
  const mappings = [];
  rawMappings.forEach((entry, index) => {
    if (!entry || typeof entry !== 'object' || Array.isArray(entry)) return;
    const kind = normalizeString(entry.kind || 'cc').toLowerCase();
    if (kind !== 'cc') return;
    const controller = normalizeIntegerInRange(
      entry.controller ?? entry.cc,
      0,
      127,
    );
    const address = normalizeString(entry.address);
    if (controller === null || !address || !address.startsWith('/')) return;
    const channel = normalizeIntegerInRange(entry.channel, 1, 16);
    const valueMode = normalizeString(
      entry.valueMode ?? entry.output ?? entry.valueType ?? 'raw',
    ).toLowerCase();
    const normalizedValueMode =
      valueMode === 'normalized' ? 'normalized' : 'raw';
    const argType =
      normalizeString(entry.argType).toLowerCase() === 'int' ? 'int' : 'float';
    mappings.push({
      id: normalizeString(entry.id) || `mapping-${index + 1}`,
      kind: 'cc',
      controller,
      channel: channel === null ? null : channel,
      address,
      valueMode: normalizedValueMode,
      scale: normalizeNumber(entry.scale, 1),
      offset: normalizeNumber(entry.offset, 0),
      argType,
    });
  });
  return mappings;
}

function eventMatchesMidiToOscMapping(event, mapping) {
  if (!event || !mapping || event.kind !== 'cc' || mapping.kind !== 'cc') {
    return false;
  }
  if (event.controller !== mapping.controller) return false;
  if (mapping.channel !== null && event.channel !== mapping.channel)
    return false;
  return true;
}

function buildMappedOscArgs(event, mapping) {
  if (!eventMatchesMidiToOscMapping(event, mapping)) return null;
  const baseValue =
    mapping.valueMode === 'normalized' ? event.value / 127 : event.value;
  let nextValue = baseValue * mapping.scale + mapping.offset;
  if (mapping.argType === 'int') {
    nextValue = Math.round(nextValue);
  }
  return [nextValue];
}

module.exports = {
  normalizeMidiToOscMappings,
  eventMatchesMidiToOscMapping,
  buildMappedOscArgs,
};
