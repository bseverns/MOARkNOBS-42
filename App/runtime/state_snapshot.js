export function createStateSnapshotStore({
  storage,
  storageKey,
  getSchemaVersion,
  getDeviceIdentity = () => ({}),
  now = () => Date.now(),
  maxAgeMs = 7 * 24 * 60 * 60 * 1000
} = {}) {
  function currentSchemaVersion() {
    return getSchemaVersion?.() ?? null;
  }

  function clear() {
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
    if (!saved.firmware_git_sha || !current.firmware_git_sha) return 'compatible';
    return saved.firmware_git_sha === current.firmware_git_sha ? 'same-device' : 'different-firmware';
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
    if (identityDecision(snapshot) === 'different-firmware' && !allowDifferentFirmware) return null;
    return snapshot.staged;
  }

  return {
    clear,
    persist,
    read,
    identityDecision,
    readStagedConfig
  };
}
