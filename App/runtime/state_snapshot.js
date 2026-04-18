export function createStateSnapshotStore({
  storage,
  storageKey,
  getSchemaVersion,
  now = () => Date.now()
} = {}) {
  function persist(stagedConfig) {
    if (!storage) return;
    try {
      if (stagedConfig === null || stagedConfig === undefined) {
        storage.removeItem(storageKey);
        return;
      }
      storage.setItem(
        storageKey,
        JSON.stringify({
          schema_version: getSchemaVersion?.() ?? null,
          staged: stagedConfig,
          timestamp: now()
        })
      );
    } catch (err) {
      console.debug('persist state snapshot failed', err);
    }
  }

  function read() {
    if (!storage) return null;
    try {
      const raw = storage.getItem(storageKey);
      if (!raw) return null;
      return JSON.parse(raw);
    } catch (err) {
      console.debug('read state snapshot failed', err);
      return null;
    }
  }

  return {
    persist,
    read
  };
}
