function resolveStorage(explicitStorage) {
  if (explicitStorage) return explicitStorage;
  if (typeof localStorage === 'undefined') return null;
  return localStorage;
}

export function normalizeLocalSlotMetaEntry(entry = {}) {
  return {
    pot: entry?.pot === undefined ? true : Boolean(entry.pot),
    label: typeof entry?.label === 'string' ? entry.label : '',
    takeover: Boolean(entry?.takeover)
  };
}

export function normalizeLocalSlotMeta(meta, slotCount) {
  const count = Math.max(0, Math.floor(Number(slotCount) || 0));
  return Array.from({ length: count }, (_, index) => normalizeLocalSlotMetaEntry(meta?.[index]));
}

function defaultShallowEqual(a, b) {
  if (a === b) return true;
  if (typeof a !== 'object' || typeof b !== 'object' || !a || !b) return false;
  const keys = new Set([...Object.keys(a), ...Object.keys(b)]);
  for (const key of keys) {
    if (a[key] !== b[key]) return false;
  }
  return true;
}

function sameMetaArray(left, right, compareFn) {
  if (!Array.isArray(left) || !Array.isArray(right)) return false;
  if (left.length !== right.length) return false;
  for (let idx = 0; idx < left.length; idx += 1) {
    if (!compareFn(left[idx], right[idx])) return false;
  }
  return true;
}

export function createLocalSlotMetaManager({
  storage,
  storageKey,
  initialSlotCount = 0,
  getSlotCount,
  cloneValue,
  shallowEqual
} = {}) {
  const compare = typeof shallowEqual === 'function' ? shallowEqual : defaultShallowEqual;
  const clone =
    typeof cloneValue === 'function'
      ? cloneValue
      : (value) => {
          if (value === undefined) return value;
          if (typeof structuredClone === 'function') return structuredClone(value);
          return JSON.parse(JSON.stringify(value));
        };

  let localSlotMeta = normalizeLocalSlotMeta([], initialSlotCount);

  function persistToStorage() {
    const store = resolveStorage(storage);
    if (!store || !storageKey) return;
    try {
      if (!localSlotMeta?.length) {
        store.removeItem(storageKey);
        return;
      }
      store.setItem(storageKey, JSON.stringify(localSlotMeta));
    } catch (err) {
      console.debug('persist local slot meta failed', err);
    }
  }

  function readFromStorage(slotCount = initialSlotCount) {
    const store = resolveStorage(storage);
    if (!store || !storageKey) {
      localSlotMeta = normalizeLocalSlotMeta(localSlotMeta, slotCount);
      return localSlotMeta;
    }
    try {
      const raw = store.getItem(storageKey);
      const parsed = raw ? JSON.parse(raw) : [];
      localSlotMeta = normalizeLocalSlotMeta(parsed, slotCount);
      return localSlotMeta;
    } catch (err) {
      console.debug('read local slot meta failed', err);
      localSlotMeta = normalizeLocalSlotMeta([], slotCount);
      return localSlotMeta;
    }
  }

  function currentSlotCount() {
    const count = typeof getSlotCount === 'function' ? Number(getSlotCount()) : 0;
    return Number.isFinite(count) ? Math.max(0, Math.floor(count)) : 0;
  }

  function ensureCount(slotCount = currentSlotCount()) {
    const normalized = normalizeLocalSlotMeta(localSlotMeta, slotCount);
    if (!sameMetaArray(normalized, localSlotMeta, compare)) {
      localSlotMeta = normalized;
      persistToStorage();
    }
    return localSlotMeta;
  }

  function extractFromConfig(config) {
    if (!config || typeof config !== 'object' || !Array.isArray(config.slots)) return false;
    ensureCount(config.slots.length);
    let changed = false;
    config.slots.forEach((slot, index) => {
      if (!slot || typeof slot !== 'object') return;
      const nextEntry = { ...localSlotMeta[index] };
      if (slot.pot !== undefined) nextEntry.pot = Boolean(slot.pot);
      if (slot.label !== undefined && typeof slot.label === 'string') nextEntry.label = slot.label;
      if (slot.takeover !== undefined) nextEntry.takeover = Boolean(slot.takeover);
      const normalized = normalizeLocalSlotMetaEntry(nextEntry);
      if (!compare(localSlotMeta[index], normalized)) {
        localSlotMeta[index] = normalized;
        changed = true;
      }
    });
    if (changed) persistToStorage();
    return changed;
  }

  function mergeIntoConfig(config) {
    if (!config || typeof config !== 'object') return clone(config);
    const merged = clone(config);
    if (!Array.isArray(merged.slots)) return merged;
    ensureCount(merged.slots.length);
    merged.slots = merged.slots.map((slot, index) => ({
      ...(slot ?? {}),
      ...normalizeLocalSlotMetaEntry(localSlotMeta[index])
    }));
    return merged;
  }

  function updateEntry(index, patch = {}) {
    const slotCount = currentSlotCount();
    if (slotCount <= 0) return false;
    ensureCount(slotCount);
    const idx = Math.max(0, Math.min(slotCount - 1, Math.floor(Number(index) || 0)));
    const current = normalizeLocalSlotMetaEntry(localSlotMeta[idx]);
    const next = normalizeLocalSlotMetaEntry({ ...current, ...patch });
    if (compare(current, next)) return false;
    localSlotMeta[idx] = next;
    persistToStorage();
    return true;
  }

  return {
    readFromStorage,
    ensureCount,
    extractFromConfig,
    mergeIntoConfig,
    updateEntry
  };
}
