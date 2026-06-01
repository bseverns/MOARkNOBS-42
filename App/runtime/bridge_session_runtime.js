export function createBridgeSessionRuntime({
  baseUrl,
  eventUrl,
  clone,
  emit,
  createClient,
  compileSchema,
  configSession,
  localManifest,
  currentSlotCount,
  localSlotMetaManager,
  getConnectedPayload,
  setRemoteManifest,
  setSchema,
  setSchemaSource,
  onTelemetry
} = {}) {
  let bridgeSessionClient = null;
  let bridgeSessionCache = null;
  let bridgeStageSyncTimer = null;
  let bridgeStageSyncPromise = null;

  function ensureClient() {
    if (!baseUrl) return null;
    if (bridgeSessionClient) return bridgeSessionClient;
    bridgeSessionClient = createClient({
      baseUrl,
      eventUrl
    });
    return bridgeSessionClient;
  }

  function syncCachedSession() {
    configSession.syncFromSession({
      ...bridgeSessionCache,
      liveConfig: bridgeSessionCache?.liveConfig,
      stagedConfig: bridgeSessionCache?.stagedConfig,
      dirty: bridgeSessionCache?.dirty
    });
    configSession.broadcastConfig({ persist: false });
  }

  function applySessionSnapshot(sessionPayload = {}, { emitConnectedConfig = true } = {}) {
    bridgeSessionCache = clone(sessionPayload);

    const manifest = sessionPayload.manifest;
    if (manifest && typeof manifest === 'object') {
      setRemoteManifest(manifest);
      localSlotMetaManager.ensureCount(
        manifest?.slot_count ?? localManifest?.slot_count ?? currentSlotCount()
      );
      emit('manifest', manifest);
    }

    if (sessionPayload.schema && typeof sessionPayload.schema === 'object') {
      setSchema(sessionPayload.schema);
      setSchemaSource(sessionPayload.schemaSource ?? 'bundled');
      compileSchema(sessionPayload.schema);
      emit('schema', sessionPayload.schema);
    } else if (sessionPayload.schemaSource) {
      setSchemaSource(sessionPayload.schemaSource);
    }

    if (sessionPayload.liveConfig || sessionPayload.stagedConfig) {
      configSession.syncFromSession(sessionPayload);
      configSession.broadcastConfig({ persist: false });
    }

    if (emitConnectedConfig) {
      emit('connected', getConnectedPayload());
    }
  }

  async function refreshSessionSnapshot({ warm = false, emitConnectedConfig = false } = {}) {
    const client = ensureClient();
    if (!client) throw new Error('Bridge session unavailable');
    const session = await client.getSession({ warm });
    if (!session || typeof session !== 'object') {
      throw new Error('Bridge session unavailable');
    }
    if (!session.schema || typeof session.schema !== 'object') {
      throw new Error('Bridge session did not provide a schema');
    }
    applySessionSnapshot(session, { emitConnectedConfig });
    return session;
  }

  async function flushStageSync({ active = false } = {}) {
    if (!active || !ensureClient()) return null;
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
    if (bridgeStageSyncPromise) return bridgeStageSyncPromise;
    const stagedConfig = clone(configSession.getStagedConfig());
    bridgeStageSyncPromise = ensureClient()
      .stageConfig(stagedConfig)
      .finally(() => {
        bridgeStageSyncPromise = null;
      });
    return bridgeStageSyncPromise;
  }

  function scheduleStageSync({ active = false } = {}) {
    if (!active) return;
    if (bridgeStageSyncTimer) clearTimeout(bridgeStageSyncTimer);
    bridgeStageSyncTimer = setTimeout(() => {
      void flushStageSync({ active: true }).catch((err) => {
        emit('status', {
          stage: 'bridge-session',
          level: 'warn',
          message: `Bridge stage sync failed: ${err.message || String(err)}`
        });
      });
    }, 120);
  }

  async function openStructuredEvents() {
    const client = ensureClient();
    if (!client) throw new Error('Bridge session unavailable');
    await client.openEvents({
      onEvent(message) {
        if (!message || typeof message !== 'object') return;
        const payload = message.payload ?? {};
        switch (message.event) {
          case 'device.ready':
            if (payload.manifest && typeof payload.manifest === 'object') {
              setRemoteManifest(payload.manifest);
              emit('manifest', payload.manifest);
            }
            if (payload.schemaSource) setSchemaSource(payload.schemaSource);
            void refreshSessionSnapshot({ warm: false, emitConnectedConfig: false }).catch(
              () => {}
            );
            break;
          case 'device.config.live':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.liveConfig = payload.config ?? null;
            bridgeSessionCache.lastApplyResult = payload.lastApplyResult ?? null;
            syncCachedSession();
            break;
          case 'device.config.staged':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.stagedConfig = payload.config ?? null;
            syncCachedSession();
            break;
          case 'device.config.dirty':
            bridgeSessionCache = bridgeSessionCache ?? {};
            bridgeSessionCache.dirty = Boolean(payload.dirty);
            syncCachedSession();
            break;
          case 'device.telemetry':
            if (payload.telemetry && typeof payload.telemetry === 'object') {
              onTelemetry(payload.telemetry);
            }
            break;
          default:
            break;
        }
      },
      onClose() {
        // Raw /ws stays connected for compatibility and live RPC lanes.
      },
      onError(err) {
        emit('status', {
          stage: 'bridge-session',
          level: 'warn',
          message: `Bridge event stream error: ${err?.message || 'event stream failed'}`
        });
      }
    });
  }

  function cancelStageSync() {
    if (bridgeStageSyncTimer) {
      clearTimeout(bridgeStageSyncTimer);
      bridgeStageSyncTimer = null;
    }
  }

  function closeEvents() {
    ensureClient()?.closeEvents();
  }

  function reset() {
    cancelStageSync();
    closeEvents();
    bridgeSessionCache = null;
  }

  return {
    ensureClient,
    refreshSessionSnapshot,
    flushStageSync,
    scheduleStageSync,
    openStructuredEvents,
    cancelStageSync,
    closeEvents,
    reset
  };
}
