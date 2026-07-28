export function createStateSnapshotStore({
  storage,
  storageKey,
  getSchemaVersion,
  getDeviceIdentity = () => ({}),
  now = () => Date.now(),
  maxAgeMs = 7 * 24 * 60 * 60 * 1000,
  persistDelayMs = 350,
  setTimeoutFn = setTimeout,
  clearTimeoutFn = clearTimeout
} = {}) {
  let persistTimer = null;
  let pendingStagedConfig = null;
  function currentSchemaVersion() {
    return getSchemaVersion?.() ?? null;
  }

  function clear() {
    if (persistTimer) clearTimeoutFn(persistTimer);
    persistTimer = null;
    pendingStagedConfig = null;
    if (!storage) return;
    try {
      storage.removeItem(storageKey);
    } catch (err) {
      console.debug('clear state snapshot failed', err);
    }
  }

  function isStale(snapshot) {
    const limit = Number(maxAgeMs);
    if (!Number.isFinite(limit) || limit <= 0) return false;
    const timestamp = Number(snapshot?.timestamp);
    if (!Number.isFinite(timestamp)) return true;
    return now() - timestamp > limit;
  }

  function schemaMatches(snapshot) {
    const expected = currentSchemaVersion();
    const actual = snapshot?.schema_version;
    if (expected === null || expected === undefined) return true;
    if (actual === null || actual === undefined) return true;
    return String(actual) === String(expected);
  }

  function isRestorableSnapshot(snapshot) {
    return (
      snapshot &&
      typeof snapshot === 'object' &&
      !Array.isArray(snapshot) &&
      snapshot.staged &&
      typeof snapshot.staged === 'object' &&
      !Array.isArray(snapshot.staged) &&
      schemaMatches(snapshot) &&
      !isStale(snapshot)
    );
  }

  function identityDecision(snapshot) {
    const current = getDeviceIdentity?.() ?? {};
    const saved = snapshot?.device ?? {};
    if (saved.firmware_git_sha && current.firmware_git_sha) {
      return saved.firmware_git_sha === current.firmware_git_sha ? 'same-device' : 'different-firmware';
    }
    const comparable = ['device_name', 'slot_count'];
    if (comparable.every((key) => saved[key] != null && current[key] != null)) {
      return comparable.every((key) => String(saved[key]) === String(current[key]))
        ? 'compatible-fallback-identity'
        : 'different-firmware';
    }
    return 'unknown-identity';
  }

  function persist(stagedConfig) {
    if (!storage) return;
    try {
      if (stagedConfig === null || stagedConfig === undefined) {
        clear();
        return;
      }
      const identity = getDeviceIdentity?.() ?? {};
      storage.setItem(
        storageKey,
        JSON.stringify({
          schema_version: currentSchemaVersion(),
          device: {
            device_name: identity.device_name ?? null,
            firmware_git_sha: identity.firmware_git_sha ?? null,
            slot_count: identity.slot_count ?? null
          },
          staged: stagedConfig,
          timestamp: now(),
          saved_at: new Date(now()).toISOString()
        })
      );
    } catch (err) {
      console.debug('persist state snapshot failed', err);
    }
  }

  function schedulePersist(stagedConfig) {
    if (stagedConfig === null || stagedConfig === undefined) {
      clear();
      return;
    }
    pendingStagedConfig = stagedConfig;
    if (persistTimer) clearTimeoutFn(persistTimer);
    persistTimer = setTimeoutFn(() => {
      persistTimer = null;
      const next = pendingStagedConfig;
      pendingStagedConfig = null;
      persist(next);
    }, Math.max(0, Number(persistDelayMs) || 0));
  }

  function flushPersist() {
    if (!persistTimer) return;
    clearTimeoutFn(persistTimer);
    persistTimer = null;
    const next = pendingStagedConfig;
    pendingStagedConfig = null;
    persist(next);
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

  function readStagedConfig({ allowDifferentFirmware = false } = {}) {
    const snapshot = read();
    if (!isRestorableSnapshot(snapshot)) return null;
    if (['different-firmware', 'unknown-identity'].includes(identityDecision(snapshot)) && !allowDifferentFirmware) return null;
    return snapshot.staged;
  }

  return {
    clear,
    flushPersist,
    persist,
    schedulePersist,
    read,
    identityDecision,
    readStagedConfig
  };
}
