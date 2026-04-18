const PROFILE_STORAGE_KEY = 'moarknobs:selected-profile';
const UI_MODE_STORAGE_KEY = 'moarknobs:ui-mode';

function resolveStorage(storage) {
  if (storage) return storage;
  if (typeof localStorage === 'undefined') return null;
  return localStorage;
}

export function clampProfileSlot(value, slotCount = 4) {
  const limit = Math.max(1, Math.floor(Number(slotCount) || 0));
  const idx = Number(value);
  if (!Number.isFinite(idx)) return 0;
  return Math.max(0, Math.min(limit - 1, Math.floor(idx)));
}

export function readProfileSlotPreference({ storage, slotCount = 4 } = {}) {
  const store = resolveStorage(storage);
  if (!store) return 0;
  try {
    const raw = store.getItem(PROFILE_STORAGE_KEY);
    return clampProfileSlot(raw, slotCount);
  } catch (err) {
    console.debug('read profile slot preference failed', err);
    return 0;
  }
}

export function persistProfileSlot(index, { storage, slotCount = 4 } = {}) {
  const store = resolveStorage(storage);
  if (!store) return;
  try {
    store.setItem(PROFILE_STORAGE_KEY, String(clampProfileSlot(index, slotCount)));
  } catch (err) {
    console.debug('persist profile slot failed', err);
  }
}

export function normalizeUIMode(mode) {
  return mode === 'advanced' ? 'advanced' : 'basic';
}

export function readUIModePreference({ storage } = {}) {
  const store = resolveStorage(storage);
  if (!store) return 'basic';
  try {
    return normalizeUIMode(store.getItem(UI_MODE_STORAGE_KEY));
  } catch (err) {
    console.debug('read ui mode preference failed', err);
    return 'basic';
  }
}

export function persistUIMode(mode, { storage } = {}) {
  const store = resolveStorage(storage);
  if (!store) return;
  try {
    store.setItem(UI_MODE_STORAGE_KEY, normalizeUIMode(mode));
  } catch (err) {
    console.debug('persist ui mode failed', err);
  }
}
