const DEFAULT_PROFILE_COUNT = 4;
const DEFAULT_MAX_LENGTH = 24;

function resolveStorage(explicitStorage) {
  if (explicitStorage) return explicitStorage;
  if (typeof localStorage === 'undefined') return null;
  return localStorage;
}

export function normalizeProfileName(value, maxLength = DEFAULT_MAX_LENGTH) {
  if (typeof value !== 'string') return '';
  return value.trim().replace(/\s+/g, ' ').slice(0, maxLength);
}

export function createLocalProfileNames({
  storage,
  storageKey = 'moarknobs:profile-names',
  profileCount = DEFAULT_PROFILE_COUNT,
  maxLength = DEFAULT_MAX_LENGTH
} = {}) {
  const count = Math.max(0, Math.floor(Number(profileCount) || 0));
  let names = Array.from({ length: count }, () => '');

  function read() {
    const store = resolveStorage(storage);
    if (!store) return [...names];
    try {
      const parsed = JSON.parse(store.getItem(storageKey) || '[]');
      names = Array.from({ length: count }, (_, index) =>
        normalizeProfileName(parsed?.[index], maxLength)
      );
    } catch (err) {
      console.debug('read local profile names failed', err);
      names = Array.from({ length: count }, () => '');
    }
    return [...names];
  }

  function persist() {
    const store = resolveStorage(storage);
    if (!store) return;
    try {
      if (names.every((name) => !name)) store.removeItem(storageKey);
      else store.setItem(storageKey, JSON.stringify(names));
    } catch (err) {
      console.debug('persist local profile names failed', err);
    }
  }

  function update(index, value) {
    const bounded = Math.max(0, Math.min(count - 1, Math.floor(Number(index) || 0)));
    const next = normalizeProfileName(value, maxLength);
    if (names[bounded] === next) return false;
    names[bounded] = next;
    persist();
    return true;
  }

  function get(index) {
    const bounded = Math.max(0, Math.min(count - 1, Math.floor(Number(index) || 0)));
    return names[bounded] ?? '';
  }

  return { read, update, get, all: () => [...names] };
}
