export function formatRouteRange(route) {
  const min = route?.range?.min;
  const max = route?.range?.max;
  if (Number.isFinite(Number(min)) && Number.isFinite(Number(max))) return `${min}-${max}`;
  return '-';
}

export function formatRouteDepth(route) {
  if (Number.isFinite(Number(route?.depth))) return Number(route.depth).toFixed(2);
  if (Number.isFinite(Number(route?.amount))) return Number(route.amount).toFixed(2);
  return '-';
}

export function formatRouteLastValue(route) {
  return Number.isFinite(Number(route?.last_value)) ? String(route.last_value) : '-';
}

function isLfoMatrixRoute(route = {}) {
  return route?.source_type === 'lfo' || /^lfo\d+$/i.test(String(route?.source ?? ''));
}

function isSlotMatrixRoute(route = {}) {
  return String(route?.route_type ?? '').toLowerCase() === 'slot_value' || /^slot\d+\.value$/i.test(String(route?.destination ?? ''));
}

function readConflictWriterIds(conflict = {}) {
  const candidates = [];
  if (Array.isArray(conflict?.route_ids)) candidates.push(...conflict.route_ids);
  if (Array.isArray(conflict?.routeIds)) candidates.push(...conflict.routeIds);
  if (Array.isArray(conflict?.writers)) candidates.push(...conflict.writers);
  if (typeof conflict?.writers === 'string') candidates.push(...conflict.writers.split(','));
  return Array.from(new Set(candidates.map((value) => String(value ?? '').trim()).filter(Boolean)));
}

function routeMatchesConflict(route = {}, conflict = {}) {
  const routeId = String(route?.id ?? '').trim();
  const writerIds = readConflictWriterIds(conflict);
  if (routeId && writerIds.includes(routeId)) return true;
  if (conflict?.target === 'slot.value' && Number.isFinite(Number(conflict?.slot))) {
    return String(route?.destination ?? '') === `slot${Number(conflict.slot)}.value`;
  }
  if (conflict?.target !== 'midi.cc') return false;
  const channel = Number(conflict?.channel);
  const cc = Number(conflict?.cc);
  if (!Number.isFinite(channel) || !Number.isFinite(cc)) return false;
  const routeChannel = Number(route?.channel ?? route?.midi?.channel);
  return Number.isFinite(routeChannel) && routeChannel === channel &&
    [route?.cc_msb, route?.cc_lsb, route?.midi?.cc].some((value) => Number(value) === cc);
}

export function formatConflictMessage(conflict = {}) {
  if (typeof conflict?.message === 'string' && conflict.message.trim()) return conflict.message;
  if (conflict?.target === 'slot.value' && Number.isFinite(Number(conflict?.slot))) {
    return `Slot ${Number(conflict.slot)} value collision`;
  }
  if (conflict?.target === 'midi.cc') {
    const channel = Number(conflict?.channel);
    const cc = Number(conflict?.cc);
    if (Number.isFinite(channel) && Number.isFinite(cc)) return `MIDI CC ${cc} on channel ${channel} has multiple writers`;
  }
  return `${conflict?.target ?? 'target'} collision`;
}

export function buildModMatrixView(report = {}, filters = {}, { previewLimit = 80 } = {}) {
  const routes = Array.isArray(report?.routes) ? report.routes : [];
  const conflicts = Array.isArray(report?.conflicts) ? report.conflicts : [];
  const selectedFilters = {
    conflictsOnly: Boolean(filters.conflictsOnly),
    lfoOnly: Boolean(filters.lfoOnly),
    slotOnly: Boolean(filters.slotOnly),
    activeOnly: Boolean(filters.activeOnly)
  };
  const filteredRoutes = routes.filter((route) => {
    if (selectedFilters.conflictsOnly && !conflicts.some((conflict) => routeMatchesConflict(route, conflict))) return false;
    if (selectedFilters.lfoOnly && !isLfoMatrixRoute(route)) return false;
    if (selectedFilters.slotOnly && !isSlotMatrixRoute(route)) return false;
    return !(selectedFilters.activeOnly && route?.active === false);
  });
  const visibleRouteIds = new Set(filteredRoutes.map((route) => String(route?.id ?? '').trim()).filter(Boolean));
  const filteredConflicts = conflicts.filter((conflict) => {
    const writerIds = readConflictWriterIds(conflict);
    return writerIds.length ? writerIds.some((writerId) => visibleRouteIds.has(writerId)) : filteredRoutes.some((route) => routeMatchesConflict(route, conflict));
  });
  return { filters: selectedFilters, routes, conflicts, filteredRoutes, filteredConflicts, limitedRoutes: filteredRoutes.slice(0, previewLimit) };
}
